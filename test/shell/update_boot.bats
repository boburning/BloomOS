#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export BOOT=/workspace/static/build/.tmp_update/bin/bloom-update-boot
    export BLOOM_UPDATE_STATE_BIN="$BLOOM_TEST_ROOT/mock-state"
    export BLOOM_HEALTH_BIN="$BLOOM_TEST_ROOT/mock-health"
    export BLOOM_UPDATE_ROLLBACK_BIN="$BLOOM_TEST_ROOT/mock-rollback"
    export BLOOM_POWER_BIN="$BLOOM_TEST_ROOT/mock-power"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_VERSION_FILE="$SDCARD/.tmp_update/onionVersion/version.txt"
    export PHASE=activation_pending
    export PENDING=1.2.3
    cat >"$BLOOM_UPDATE_STATE_BIN" <<'EOF'
#!/bin/sh
case "$1" in
    status) printf '{"schema":1,"phase":"%s","pending_version":"%s","boot_attempts":0}\n' "$PHASE" "$PENDING" ;;
    *) printf '%s:%s\n' "$1" "${2:-}" >>"$BLOOM_TEST_ROOT/calls"; printf '{"schema":1,"phase":"%s"}\n' "${BOOT_PHASE:-testing}" ;;
esac
EOF
    cat >"$BLOOM_HEALTH_BIN" <<'EOF'
#!/bin/sh
printf '%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
[ "${HEALTHY:-1}" = 1 ]
EOF
    cat >"$BLOOM_UPDATE_ROLLBACK_BIN" <<'EOF'
#!/bin/sh
printf '%s\n' rollback >>"$BLOOM_TEST_ROOT/calls"
EOF
    cat >"$BLOOM_POWER_BIN" <<'EOF'
#!/bin/sh
printf 'power:%s\n' "$*" >>"$BLOOM_TEST_ROOT/calls"
EOF
    chmod +x "$BLOOM_UPDATE_STATE_BIN" "$BLOOM_HEALTH_BIN" "$BLOOM_UPDATE_ROLLBACK_BIN" "$BLOOM_POWER_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "begin counts an activated candidate boot" {
    run "$BOOT" begin

    [ "$status" -eq 0 ]
    grep -Fx 'boot-attempt:' "$BLOOM_TEST_ROOT/calls"
}

@test "begin automatically publishes and reboots into recovery at the attempt bound" {
    export BOOT_PHASE=recovery_required

    run "$BOOT" begin

    [ "$status" -eq 1 ]
    grep -Fx 'boot-attempt:' "$BLOOM_TEST_ROOT/calls"
    grep -Fx rollback "$BLOOM_TEST_ROOT/calls"
    grep -Fx 'power:request reboot' "$BLOOM_TEST_ROOT/calls"
    printf '%s' "$output" | grep -F 'automatic rollback reboot returned'
}

@test "begin does not recursively recover a failed rollback" {
    export PHASE=rollback_pending
    export BOOT_PHASE=rollback_failed

    run "$BOOT" begin

    [ "$status" -eq 0 ]
    grep -Fx 'boot-attempt:' "$BLOOM_TEST_ROOT/calls"
    ! grep -Fx rollback "$BLOOM_TEST_ROOT/calls"
    ! grep -q '^power:' "$BLOOM_TEST_ROOT/calls"
    printf '%s' "$output" | grep -F '"phase":"rollback_failed"'
}

@test "begin is a no-op outside candidate validation" {
    export PHASE=known_good

    run "$BOOT" begin

    [ "$status" -eq 0 ]
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
}

@test "confirm requires the installed version and health checks" {
    export PHASE=testing
    printf 'v1.2.3\n' >"$BLOOM_VERSION_FILE"

    run "$BOOT" confirm

    [ "$status" -eq 0 ]
    [ "$(sed -n '1p' "$BLOOM_TEST_ROOT/calls")" = 'health:--json' ]
    [ "$(sed -n '2p' "$BLOOM_TEST_ROOT/calls")" = 'mark-good:1.2.3' ]
}

@test "confirm refuses a version mismatch or unhealthy boot" {
    export PHASE=testing
    printf 'v1.2.4\n' >"$BLOOM_VERSION_FILE"

    run "$BOOT" confirm
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'installed version does not match'

    printf '1.2.3\n' >"$BLOOM_VERSION_FILE"
    export HEALTHY=0
    run "$BOOT" confirm
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'device health checks failed'
    ! grep -q '^mark-good:' "$BLOOM_TEST_ROOT/calls"
}

@test "runtime reconciles update state immediately after installation" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    installer_line="$(grep -n '    check_installer' "$runtime" | cut -d: -f1)"
    reconcile_line="$(grep -n '        bloom-update-boot begin' "$runtime" | cut -d: -f1)"

    [ "$reconcile_line" -gt "$installer_line" ]
    [ "$reconcile_line" -lt $((installer_line + 10)) ]
}

@test "confirm accepts the release version file without a trailing newline" {
    export PHASE=testing
    export PENDING=1.2.3
    printf 'v1.2.3' >"$BLOOM_VERSION_FILE"

    run "$BOOT" confirm

    [ "$status" -eq 0 ]
    grep -Fx 'mark-good:1.2.3' "$BLOOM_TEST_ROOT/calls"
}
