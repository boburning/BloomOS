#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export SESSION=/workspace/static/build/.tmp_update/bin/bloom-session
    export BLOOM_SESSION_ROOT="$BLOOM_TEST_ROOT/session"
    export BLOOM_PROC_ROOT="$BLOOM_TEST_ROOT/proc"
    export BLOOM_UPTIME_FILE="$BLOOM_TEST_ROOT/uptime"
    export BLOOM_LAUNCH_BIN="$MOCK_BIN/bloom-launch"
    export BLOOM_SEND_UDP_BIN="$MOCK_BIN/sendUDP"
    export BLOOM_SAVE_FLUSH_BIN="$MOCK_BIN/bloom-save-flush"
    export BLOOM_SIGNAL_BIN="$MOCK_BIN/signal"
    export BLOOM_QUIT_WAIT_SECONDS=0
    export BLOOM_TERM_WAIT_SECONDS=0
    export BLOOM_KILL_WAIT_SECONDS=0
    mkdir -p "$BLOOM_PROC_ROOT" "$BLOOM_TEST_ROOT/requests"
    printf '100.00 50.00\n' >"$BLOOM_UPTIME_FILE"
    export REQUEST="$BLOOM_TEST_ROOT/requests/game.json"
    printf '%s\n' '{"schema":1}' >"$REQUEST"
cat >"$BLOOM_LAUNCH_BIN" <<'EOF'
#!/bin/sh
case "$1" in
    validate) [ -f "$2" ] ;;
    get) [ "$3" = core ] && printf '%s\n' gambatte_libretro.so ;;
    *) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_LAUNCH_BIN"
    cat >"$BLOOM_SIGNAL_BIN" <<'EOF'
#!/bin/sh
printf '%s %s\n' "$1" "$2" >>"$MOCK_LOG"
EOF
    chmod +x "$BLOOM_SIGNAL_BIN"
    cat >"$BLOOM_SAVE_FLUSH_BIN" <<'EOF'
#!/bin/sh
printf '{"schema":1,"core":"%s","corename":"Gambatte","files_flushed":2,"directories_flushed":4}\n' "$1"
EOF
    chmod +x "$BLOOM_SAVE_FLUSH_BIN"
}

start_running_session() {
    "$SESSION" start "$REQUEST"
    "$SESSION" transition PREPARING STARTING
    mkdir -p "$BLOOM_PROC_ROOT/123"
    "$SESSION" attach 123
}

stop_to_flushing() {
    cat >"$BLOOM_SEND_UDP_BIN" <<'EOF'
#!/bin/sh
[ "$1" = QUIT ] || exit 1
rm -rf "$BLOOM_PROC_ROOT/$(cat "$BLOOM_SESSION_ROOT/pid")"
EOF
    chmod +x "$BLOOM_SEND_UDP_BIN"
    "$SESSION" stop-retroarch
}

teardown() { teardown_bloom_fixture; }

@test "session follows the explicit successful lifecycle" {
    start_running_session
    printf '112.99 50.00\n' >"$BLOOM_UPTIME_FILE"
    stop_to_flushing
    "$SESSION" flush-saves
    "$SESSION" complete

    run "$SESSION" status --json
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"state":"STOPPED"'
    printf '%s' "$output" | grep -F '"pid":123'
    printf '%s' "$output" | grep -F '"duration_seconds":12'
    grep -Eq '^[0-9a-f]{64}$' "$BLOOM_SESSION_ROOT/request_sha256"
}

@test "session prepares RA policy on its private request copy" {
    export BLOOM_RA_PREPARE_BIN="$MOCK_BIN/bloom-ra-session-prepare"
    cat >"$BLOOM_RA_PREPARE_BIN" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >"$BLOOM_TEST_ROOT/prepared-request"
printf '\n' >>"$1"
EOF
    chmod +x "$BLOOM_RA_PREPARE_BIN"

    "$SESSION" start "$REQUEST"

    case "$(cat "$BLOOM_TEST_ROOT/prepared-request")" in
        "$BLOOM_SESSION_ROOT/request.json.tmp."*) ;;
        *) false ;;
    esac
    [ "$(wc -l <"$REQUEST")" -eq 1 ]
    [ "$(wc -l <"$BLOOM_SESSION_ROOT/request.json")" -eq 2 ]
}

@test "wall-clock changes cannot alter monotonic session duration" {
    start_running_session
    printf '107.75 50.00\n' >"$BLOOM_UPTIME_FILE"

    run stop_to_flushing

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"duration_seconds":7'
    grep -Fx 7 "$BLOOM_SESSION_ROOT/duration_seconds"
}

@test "a regressed monotonic clock fails the session without inventing play time" {
    start_running_session
    printf '99.00 50.00\n' >"$BLOOM_UPTIME_FILE"

    run stop_to_flushing

    [ "$status" -eq 1 ]
    grep -Fx FAILED "$BLOOM_SESSION_ROOT/state"
    grep -Fx monotonic_clock_regressed "$BLOOM_SESSION_ROOT/failure_reason"
    [ ! -e "$BLOOM_SESSION_ROOT/duration_seconds" ]
}

@test "flushing cannot complete until a scoped save flush succeeds" {
    start_running_session
    stop_to_flushing

    run "$SESSION" transition FLUSHING STOPPED
    [ "$status" -eq 1 ]
    run "$SESSION" complete
    [ "$status" -eq 1 ]
    grep -Fx FLUSHING "$BLOOM_SESSION_ROOT/state"

    run "$SESSION" flush-saves
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"core":"gambatte_libretro.so"'
    "$SESSION" complete
    grep -Fx STOPPED "$BLOOM_SESSION_ROOT/state"
}

@test "failed scoped save flush makes the session terminally failed" {
    start_running_session
    stop_to_flushing
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_SAVE_FLUSH_BIN"
    chmod +x "$BLOOM_SAVE_FLUSH_BIN"

    run "$SESSION" flush-saves
    [ "$status" -eq 1 ]
    grep -Fx FAILED "$BLOOM_SESSION_ROOT/state"
    grep -Fx save_flush_failed "$BLOOM_SESSION_ROOT/failure_reason"
    [ ! -e "$BLOOM_SESSION_ROOT/save_flush.json" ]
}

@test "invalid and stale transitions leave state unchanged" {
    "$SESSION" start "$REQUEST"

    run "$SESSION" transition PREPARING RUNNING
    [ "$status" -eq 1 ]
    grep -Fx PREPARING "$BLOOM_SESSION_ROOT/state"

    run "$SESSION" transition STARTING RUNNING
    [ "$status" -eq 1 ]
    grep -Fx PREPARING "$BLOOM_SESSION_ROOT/state"

    "$SESSION" transition PREPARING STARTING
    mkdir -p "$BLOOM_PROC_ROOT/123"
    run "$SESSION" transition STARTING RUNNING
    [ "$status" -eq 1 ]
    grep -Fx STARTING "$BLOOM_SESSION_ROOT/state"
}

@test "a second active session is rejected but terminal sessions can restart" {
    "$SESSION" start "$REQUEST"
    run "$SESSION" start "$REQUEST"
    [ "$status" -eq 1 ]

    "$SESSION" fail launch_failed
    run "$SESSION" status --json
    printf '%s' "$output" | grep -F '"state":"FAILED"'
    printf '%s' "$output" | grep -F '"failure_reason":"launch_failed"'

    run "$SESSION" start "$REQUEST"
    [ "$status" -eq 0 ]
    grep -Fx PREPARING "$BLOOM_SESSION_ROOT/state"
}

@test "attach requires a numeric live process and failure reasons are data-safe" {
    "$SESSION" start "$REQUEST"
    "$SESSION" transition PREPARING STARTING

    run "$SESSION" attach not-a-pid
    [ "$status" -eq 1 ]
    run "$SESSION" attach 999
    [ "$status" -eq 1 ]
    run "$SESSION" fail 'bad"reason'
    [ "$status" -eq 1 ]
    grep -Fx STARTING "$BLOOM_SESSION_ROOT/state"
}

@test "session owns RetroArch QUIT and enters flushing only after process exit" {
    start_running_session
    cat >"$BLOOM_SEND_UDP_BIN" <<'EOF'
#!/bin/sh
[ "$1" = QUIT ] || exit 1
rm -rf "$BLOOM_PROC_ROOT/$(cat "$BLOOM_SESSION_ROOT/pid")"
EOF
    chmod +x "$BLOOM_SEND_UDP_BIN"

    run "$SESSION" stop-retroarch

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"state":"FLUSHING"'
    printf '%s' "$output" | grep -F '"shutdown_method":"retroarch_quit"'
    grep -Fx FLUSHING "$BLOOM_SESSION_ROOT/state"
    grep -F -- '-CONT 123' "$MOCK_LOG"
}

@test "failed control request uses bounded SIGTERM fallback" {
    start_running_session
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_SEND_UDP_BIN"
    chmod +x "$BLOOM_SEND_UDP_BIN"
    cat >"$BLOOM_SIGNAL_BIN" <<'EOF'
#!/bin/sh
printf '%s %s\n' "$1" "$2" >>"$MOCK_LOG"
[ "$1" != -TERM ] || rm -rf "$BLOOM_PROC_ROOT/$2"
EOF
    chmod +x "$BLOOM_SIGNAL_BIN"

    run "$SESSION" stop-retroarch

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"shutdown_method":"sigterm"'
    grep -F -- '-TERM 123' "$MOCK_LOG"
    grep -Fx FLUSHING "$BLOOM_SESSION_ROOT/state"
}

@test "forced kill is a failed terminal state rather than successful flushing" {
    start_running_session
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_SEND_UDP_BIN"
    chmod +x "$BLOOM_SEND_UDP_BIN"

    run "$SESSION" stop-retroarch

    [ "$status" -eq 1 ]
    grep -Fx FAILED "$BLOOM_SESSION_ROOT/state"
    grep -Fx forced_kill_required "$BLOOM_SESSION_ROOT/failure_reason"
    grep -F -- '-KILL 123' "$MOCK_LOG"
}
