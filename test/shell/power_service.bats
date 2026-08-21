#!/usr/bin/env bats

setup() {
    export POWER=/workspace/static/build/.tmp_update/bin/bloom-power
    export BLOOM_TEST_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BLOOM_TEST_ROOT/bloom-platform"
    export BLOOM_SHUTDOWN_BIN="$BLOOM_TEST_ROOT/shutdown"
    mkdir -p "$BLOOM_TEST_ROOT"
    cat >"$BLOOM_PLATFORM_BIN" <<'SH'
#!/bin/sh
[ "$1" = model ] || exit 2
printf '%s\n' "${DEVICE_MODEL:-mini_plus}"
SH
    cat >"$BLOOM_SHUTDOWN_BIN" <<'SH'
#!/bin/sh
printf '%s' "$*" >"$BLOOM_TEST_ROOT/shutdown-call"
SH
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_SHUTDOWN_BIN"
}

@test "status reports model-aware poweroff strategy without mutation" {
    run "$POWER" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-power","available":true,"model":"mini_plus","poweroff_strategy":"poweroff","reboot_supported":true}' ]
    [ ! -e "$BLOOM_TEST_ROOT/shutdown-call" ]

    export DEVICE_MODEL=mini
    run "$POWER" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"poweroff_strategy":"reboot"'* ]]
}

@test "requests delegate only fixed reboot and poweroff operations" {
    run "$POWER" request reboot
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/shutdown-call")" = -r ]

    run "$POWER" request poweroff
    [ "$status" -eq 0 ]
    [ ! -s "$BLOOM_TEST_ROOT/shutdown-call" ]

    run "$POWER" request suspend
    [ "$status" -eq 2 ]
    [[ "$output" == *'"code":"invalid_request"'* ]]
}

@test "unknown hardware and unavailable dependencies fail closed" {
    export DEVICE_MODEL=unknown
    run "$POWER" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"available":false'* ]]
    [[ "$output" == *'"poweroff_strategy":"unavailable"'* ]]

    run "$POWER" request reboot
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"unsupported_hardware"'* ]]

    export DEVICE_MODEL=mini_plus
    rm "$BLOOM_SHUTDOWN_BIN"
    run "$POWER" request poweroff
    [ "$status" -eq 1 ]
    [[ "$output" == *'"code":"shutdown_unavailable"'* ]]
}
