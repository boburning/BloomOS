#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export VERIFY=/workspace/static/build/.tmp_update/bin/bloom-update-verify
    export STAGE=/workspace/static/build/.tmp_update/bin/bloom-update-stage
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_VERIFY_BIN="$VERIFY"
    export BLOOM_OPENSSL_BIN=/usr/bin/openssl
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_UPDATE_PUBLIC_KEY="$BLOOM_TEST_ROOT/public.pem"
    export BLOOM_UPDATE_ROOT="$BLOOM_TEST_ROOT/update"
    export BLOOM_UPDATE_CHANNEL_BIN=/workspace/static/build/.tmp_update/bin/bloom-update-channel
    export ARCHIVE="$BLOOM_TEST_ROOT/BloomOS-v1.2.3.zip"
    export MANIFEST="$BLOOM_TEST_ROOT/manifest.json"
    export SIGNATURE="$BLOOM_TEST_ROOT/manifest.sig"
    printf 'release archive\n' >"$ARCHIVE"
    openssl genpkey -algorithm Ed25519 -out "$BLOOM_TEST_ROOT/private.pem" >/dev/null 2>&1
    openssl pkey -in "$BLOOM_TEST_ROOT/private.pem" -pubout -out "$BLOOM_UPDATE_PUBLIC_KEY" >/dev/null 2>&1
    size="$(wc -c <"$ARCHIVE" | tr -d ' ')"
    digest="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
    printf '{"schema":1,"product":"BloomOS","version":"1.2.3","channel":"beta","devices":["mini","plus","flip"],"artifacts":[{"filename":"BloomOS-v1.2.3.zip","sha256":"%s","size":%s,"media_type":"application/zip"}]}\n' \
        "$digest" "$size" >"$MANIFEST"
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/private.pem" -rawin -in "$MANIFEST" -out "$SIGNATURE"
    "$BLOOM_UPDATE_CHANNEL_BIN" set beta >/dev/null
}

@test "selected channel rejects less stable signed releases before staging" {
    "$BLOOM_UPDATE_CHANNEL_BIN" set stable >/dev/null
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'beta update is not allowed by selected stable channel'
    [ ! -e "$BLOOM_UPDATE_ROOT/staged/1.2.3" ]
}

@test "nightly selection accepts a signed beta release" {
    "$BLOOM_UPDATE_CHANNEL_BIN" set nightly >/dev/null
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"channel":"beta"'
}

@test "development release requires explicit developer mode" {
    sed -i 's/"beta"/"development"/' "$MANIFEST"
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/private.pem" -rawin -in "$MANIFEST" -out "$SIGNATURE"
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'development updates require developer mode'
    touch "$SDCARD/.bloom-dev"
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"
    [ "$status" -eq 0 ]
}

@test "refuses low capacity before creating staging state" {
    cat >"$MOCK_BIN/df" <<'EOF'
#!/bin/sh
printf '%s\n' 'Filesystem 1024-blocks Used Available Capacity Mounted on'
printf '%s\n' '/dev/mock 100000 99999 1 100% /mnt/SDCARD'
EOF
    chmod +x "$MOCK_BIN/df"

    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'insufficient free space'
    [ ! -e "$BLOOM_UPDATE_ROOT/staged" ]
}

@test "rejects an invalid free-space reserve" {
    export BLOOM_UPDATE_MIN_FREE_KB=0

    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'free-space reserve is invalid'
    [ ! -e "$BLOOM_UPDATE_ROOT/staged" ]
}

teardown() { teardown_bloom_fixture; }

@test "publishes an independently verified immutable release triplet" {
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"staged"'
    printf '%s' "$output" | grep -F '"channel":"beta"'
    staged="$BLOOM_UPDATE_ROOT/staged/1.2.3"
    [ -f "$staged/manifest.json" ]
    [ -f "$staged/manifest.sig" ]
    [ -f "$staged/BloomOS-v1.2.3.zip" ]
    grep -F '"status":"verified"' "$staged/verified.json"
    [ -z "$(find "$BLOOM_UPDATE_ROOT/staged" -maxdepth 1 -name '.incoming-*' -print)" ]
}

@test "rejects an unverified release without creating staging state" {
    printf 'tampered\n' >>"$ARCHIVE"

    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -ne 0 ]
    [ ! -e "$BLOOM_UPDATE_ROOT/staged/1.2.3" ]
}

@test "refuses to replace an already staged version" {
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"
    [ "$status" -eq 0 ]

    printf 'sentinel\n' >"$BLOOM_UPDATE_ROOT/staged/1.2.3/sentinel"
    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'version is already staged'
    grep -F 'sentinel' "$BLOOM_UPDATE_ROOT/staged/1.2.3/sentinel"
}

@test "refuses a symlinked update root" {
    rm -rf "$BLOOM_UPDATE_ROOT"
    mkdir -p "$BLOOM_TEST_ROOT/elsewhere"
    ln -s "$BLOOM_TEST_ROOT/elsewhere" "$BLOOM_UPDATE_ROOT"

    run "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'staging path is unsafe'
    [ -z "$(find "$BLOOM_TEST_ROOT/elsewhere" -mindepth 1 -print)" ]
}
