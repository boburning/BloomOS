#!/usr/bin/env bats

setup() {
    export LOGIN=/workspace/static/build/.tmp_update/bin/bloom-ra-login
    export BLOOM_RA_LOGIN_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_RA_BIN="$BATS_TEST_TMPDIR/bloom-ra"
    export BLOOM_CURL_BIN="$BATS_TEST_TMPDIR/curl"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_RA_LOGIN_CA_FILE="$BATS_TEST_TMPDIR/cacert.pem"
    export CURL_ARGS="$BATS_TEST_TMPDIR/curl-args"
    export CURL_BODY="$BATS_TEST_TMPDIR/curl-body"
    export RA_ARGS="$BATS_TEST_TMPDIR/ra-args"
    : >"$BLOOM_RA_LOGIN_CA_FILE"
    cat >"$BLOOM_CURL_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$@" >"$CURL_ARGS"
cat >"$CURL_BODY"
printf '%s\n' '{"Success":true,"User":"Player_1","Token":"private-token"}'
SH
    cat >"$BLOOM_RA_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$@" >"$RA_ARGS"
IFS= read -r token
[ "$token" = private-token ] || exit 1
printf '%s\n' '{"schema":1,"authenticated":true}'
SH
    chmod +x "$BLOOM_CURL_BIN" "$BLOOM_RA_BIN"
}

@test "device login exchanges stdin credentials without argv or output disclosure" {
    run sh -c "printf '%s\n%s\n' 'Player_1' 'p@ss word&more' | '$LOGIN'"
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-ra-login","configured":true,"authenticated":true}' ]
    [ "$(cat "$CURL_BODY")" = 'r=login2&u=Player_1&p=p%40ss%20word%26more' ]
    [ "$(tr '\n' ' ' <"$RA_ARGS")" = 'account configure Player_1 softcore disabled ' ]
    ! grep -F 'p@ss' "$CURL_ARGS" "$RA_ARGS"
    ! grep -F 'private-token' "$CURL_ARGS" "$RA_ARGS"
    grep -F 'BloomOS/0.1.0 (Miyoo) bloom-ra-login/1.0.0' "$CURL_ARGS"
}

@test "device login rejects unsafe input and unavailable trust without disclosure" {
    run sh -c "printf '%s\n%s\n' 'bad user' 'secret-value' | '$LOGIN'"
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"invalid_username"'* ]]
    [[ "$output" != *'secret-value'* ]]

    export BLOOM_RA_LOGIN_CA_FILE="$BATS_TEST_TMPDIR/missing-ca"
    run sh -c "printf '%s\n%s\n' 'Player_1' 'secret-value' | '$LOGIN'"
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"tls_unavailable"'* ]]
    [[ "$output" != *'secret-value'* ]]
}

@test "bloomctl exposes only the stdin-driven account login shape" {
    wrapper="$BATS_TEST_TMPDIR/login-wrapper"
    cat >"$wrapper" <<'SH'
#!/bin/sh
[ "$#" -eq 0 ] || exit 9
IFS= read -r marker
printf '{"marker":"%s"}\n' "$marker"
SH
    chmod +x "$wrapper"
    export BLOOM_RA_LOGIN_BIN="$wrapper"
    export BLOOM_RA_BIN="$BATS_TEST_TMPDIR/bloom-ra-present"
    printf '#!/bin/sh\nexit 0\n' >"$BLOOM_RA_BIN"
    chmod +x "$BLOOM_RA_BIN"

    run sh -c "printf '%s\n' stdin-only | BLOOM_RA_LOGIN_BIN='$wrapper' BLOOM_RA_BIN='$BLOOM_RA_BIN' sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account login"
    [ "$status" -eq 0 ]
    [ "$output" = '{"marker":"stdin-only"}' ]

    run env BLOOM_RA_LOGIN_BIN="$wrapper" BLOOM_RA_BIN="$BLOOM_RA_BIN" sh /workspace/static/build/.tmp_update/bin/bloomctl achievements account login unexpected
    [ "$status" -eq 2 ]
}
