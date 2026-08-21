#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-lid
    export BLOOM_LID_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export MODEL=mini_plus
    mkdir -p "$BLOOM_LID_ROOT"
    cat >"$BLOOM_PLATFORM_BIN" <<'SH'
#!/bin/sh
[ "$1" = model ] || exit 2
printf '%s\n' "$MODEL"
SH
    chmod +x "$BLOOM_PLATFORM_BIN"
}

@test "non-lid hardware reports unsupported without mutation" {
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-lid","available":false,"closed":null,"state":"unsupported"}' ]
}

@test "current Flip platform path reports closed and open states" {
    export MODEL=mini_flip
    hall="$BLOOM_LID_ROOT/sys/devices/platform/hall-mh248"
    mkdir -p "$hall"
    printf '0\n' >"$hall/hallvalue"

    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"available":true,"closed":true,"state":"closed"'* ]]

    printf '1\n' >"$hall/hallvalue"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"available":true,"closed":false,"state":"open"'* ]]
}

@test "legacy Flip platform path remains supported" {
    export MODEL=mini_flip
    hall="$BLOOM_LID_ROOT/sys/devices/soc0/soc/soc:hall-mh248"
    mkdir -p "$hall"
    printf '1\n' >"$hall/hallvalue"

    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"closed":false,"state":"open"'* ]]
}

@test "Flip sensor absence malformed data and links fail closed" {
    export MODEL=mini_flip
    run "$SERVICE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"sensor_unavailable"'* ]]

    hall="$BLOOM_LID_ROOT/sys/devices/platform/hall-mh248"
    mkdir -p "$hall"
    printf 'closed\n' >"$hall/hallvalue"
    run "$SERVICE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"sensor_error"'* ]]

    rm "$hall/hallvalue"
    printf '0\n' >"$BATS_TEST_TMPDIR/target"
    ln -s "$BATS_TEST_TMPDIR/target" "$hall/hallvalue"
    run "$SERVICE" status
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"sensor_error"'* ]]
}

@test "unknown operations are rejected" {
    run "$SERVICE" watch
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_arguments"'* ]]
}
