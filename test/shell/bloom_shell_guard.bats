#!/usr/bin/env bats

setup() {
    export BLOOM_SHELL_GUARD_ROOT="$BATS_TEST_TMPDIR/runtime"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_SHELL_FAILURE_THRESHOLD=3
    GUARD=/workspace/static/build/.tmp_update/bin/bloom-shell-guard
}

@test "tracks an interrupted start and enters safe mode at the bounded failure threshold" {
    run "$GUARD" begin
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = false ]

    # A second begin observes that the first start never reached ready/failed.
    run "$GUARD" begin
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.consecutive_failures')" -eq 1 ]
    second_id="$(printf '%s\n' "$output" | jq -er '.launch_id')"
    "$GUARD" failed "$second_id" > /dev/null
    third_id="$("$GUARD" begin | jq -er '.launch_id')"
    run "$GUARD" failed "$third_id"
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.consecutive_failures')" -eq 3 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = true ]

    run "$GUARD" begin
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = true ]
}

@test "ready launch resets failures while safe mode remains explicit until cleared" {
    first_id="$("$GUARD" begin | jq -er '.launch_id')"
    "$GUARD" failed "$first_id" > /dev/null
    second_id="$("$GUARD" begin | jq -er '.launch_id')"
    run "$GUARD" ready "$second_id"
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.phase')" = ready ]
    [ "$(printf '%s\n' "$output" | jq -er '.consecutive_failures')" -eq 0 ]

    run "$GUARD" clear-safe-mode
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.phase')" = idle ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = false ]
}

@test "successful structured launch clears crash state" {
    launch_id="$("$GUARD" begin | jq -er '.launch_id')"
    run "$GUARD" complete "$launch_id"
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.phase')" = idle ]
    [ "$(printf '%s\n' "$output" | jq -er '.consecutive_failures')" -eq 0 ]
}

@test "safe mode remains latched across readiness failure and launch until explicitly cleared" {
    for _ in 1 2 3; do
        launch_id="$("$GUARD" begin | jq -er '.launch_id')"
        "$GUARD" failed "$launch_id" > /dev/null
    done
    launch_id="$("$GUARD" begin | jq -er '.launch_id')"
    "$GUARD" ready "$launch_id" > /dev/null
    run "$GUARD" failed "$launch_id"
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = true ]

    launch_id="$("$GUARD" begin | jq -er '.launch_id')"
    run "$GUARD" complete "$launch_id"
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = true ]

    run "$GUARD" clear-safe-mode
    [ "$status" -eq 0 ]
    [ "$(printf '%s\n' "$output" | jq -er '.safe_mode')" = false ]
}

@test "rejects stale launch ids malformed state and symlink boundaries" {
    launch_id="$("$GUARD" begin | jq -er '.launch_id')"
    run "$GUARD" failed "$((launch_id + 1))"
    [ "$status" -ne 0 ]

    printf 'not json\n' > "$BLOOM_SHELL_GUARD_ROOT/shell-state.json"
    run "$GUARD" status
    [ "$status" -ne 0 ]

    rm -rf "$BLOOM_SHELL_GUARD_ROOT"
    ln -s "$BATS_TEST_TMPDIR" "$BLOOM_SHELL_GUARD_ROOT"
    run "$GUARD" status
    [ "$status" -ne 0 ]
}

@test "runtime supervises readiness and passes only the bounded safe-mode flag" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F 'guard_state="$($shell_guard begin)"' "$runtime"
    grep -F 'BLOOM_SAFE_MODE="$safe_mode"' "$runtime"
    grep -F '"$shell_guard" ready "$launch_id"' "$runtime"
    grep -F '"$shell_guard" failed "$launch_id"' "$runtime"
    grep -F '"$shell_guard" complete "$launch_id"' "$runtime"
    grep -F 'Bloom Safe Mode pending; suppressing automatic resume' "$runtime"
    grep -F 'export BLOOM_RA_FORCE_DISABLED=1' "$runtime"
    grep -F '"$shell_guard" clear-safe-mode' "$runtime"
}

@test "runtime suppresses auto-resume before an interrupted start crosses the threshold" {
    sysdir="$BATS_TEST_TMPDIR/system"
    mkdir -p "$sysdir/bin"
    cp "$GUARD" "$sysdir/bin/bloom-shell-guard"
    chmod +x "$sysdir/bin/bloom-shell-guard"
    ln -s /usr/bin/jq "$sysdir/bin/jq"
    eval "$(sed -n '/^mainui_development_fallback_enabled() {/,/^launch_bloom_shell() {/p' \
        /workspace/static/build/.tmp_update/runtime.sh | sed '$d')"

    run bloom_shell_safe_mode_pending
    [ "$status" -ne 0 ]
    first_id="$("$GUARD" begin | jq -er '.launch_id')"
    "$GUARD" failed "$first_id" > /dev/null
    second_id="$("$GUARD" begin | jq -er '.launch_id')"
    "$GUARD" failed "$second_id" > /dev/null
    "$GUARD" begin > /dev/null

    run bloom_shell_safe_mode_pending
    [ "$status" -eq 0 ]
    [ "$("$GUARD" status | jq -er '.safe_mode')" = false ]

    printf 'malformed\n' > "$BLOOM_SHELL_GUARD_ROOT/shell-state.json"
    run bloom_shell_safe_mode_pending
    [ "$status" -eq 0 ]
}

@test "runtime dynamically records crashes latches safe mode and clears structured handoff" {
    sysdir="$BATS_TEST_TMPDIR/system"
    miyoodir="$BATS_TEST_TMPDIR/miyoo"
    mkdir -p "$sysdir/bin" "$sysdir/config" "$miyoodir/lib"
    cp "$GUARD" "$sysdir/bin/bloom-shell-guard"
    chmod +x "$sysdir/bin/bloom-shell-guard"
    ln -s /usr/bin/jq "$sysdir/bin/jq"
    export BLOOM_SHELL_READY_SECONDS=0
    bloom_developer_marker="$BATS_TEST_TMPDIR/.bloom-dev"
    calls="$BATS_TEST_TMPDIR/calls"

    log() { :; }
    start_audioserver() { :; }
    launch_main_ui() { printf 'fallback\n' >> "$calls"; }
    set_prev_state() { printf '%s\n' "$1" > "$BATS_TEST_TMPDIR/prev-state"; }
    eval "$(sed -n '/^mainui_development_fallback_enabled() {/,/^launch_bloom_shell() {/p' \
        /workspace/static/build/.tmp_update/runtime.sh | sed '$d')"
    eval "$(sed -n '/^launch_bloom_shell() {/,/^launch_main_ui() {/p' \
        /workspace/static/build/.tmp_update/runtime.sh | sed '$d')"

    cat > "$sysdir/bin/bloom-shell" <<'SH'
#!/bin/sh
printf '%s\n' "$BLOOM_SAFE_MODE" >> "$BLOOM_TEST_SAFE_MODES"
exit 1
SH
    chmod +x "$sysdir/bin/bloom-shell"
    export BLOOM_TEST_SAFE_MODES="$BATS_TEST_TMPDIR/safe-modes"

    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ ! -e "$calls" ]
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ ! -e "$calls" ]
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ "$("$GUARD" status | jq -er '.consecutive_failures')" -eq 3 ]
    [ "$("$GUARD" status | jq -er '.safe_mode')" = true ]
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ "$(tail -n 1 "$BLOOM_TEST_SAFE_MODES")" = true ]

    cat > "$sysdir/bin/bloom-shell" <<'SH'
#!/bin/sh
[ "$BLOOM_SAFE_MODE" = true ]
exit 21
SH
    chmod +x "$sysdir/bin/bloom-shell"
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ "$("$GUARD" status | jq -er '.safe_mode')" = false ]
    [ "${BLOOM_RA_FORCE_DISABLED:-}" = "" ]

    cat > "$sysdir/bin/bloom-shell" <<'SH'
#!/bin/sh
printf 'launch\n' > "$BLOOM_TEST_COMMAND"
exit 20
SH
    chmod +x "$sysdir/bin/bloom-shell"
    export BLOOM_TEST_COMMAND="$sysdir/cmd_to_run.sh"
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    [ "$("$GUARD" status | jq -er '.phase')" = idle ]
    [ "$("$GUARD" status | jq -er '.consecutive_failures')" -eq 0 ]
    [ "$(cat "$BATS_TEST_TMPDIR/prev-state")" = bloom-shell ]

    touch "$bloom_developer_marker" "$sysdir/config/.mainuiFallback"
    cat > "$sysdir/bin/bloom-shell" <<'SH'
#!/bin/sh
exit 1
SH
    chmod +x "$sysdir/bin/bloom-shell"
    run launch_bloom_shell
    [ "$status" -eq 0 ]
    grep -Fx fallback "$calls"
}
