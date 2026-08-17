#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export SESSION=/workspace/static/build/.tmp_update/bin/bloom-session
    export BLOOM_SESSION_ROOT="$BLOOM_TEST_ROOT/session"
    export BLOOM_PROC_ROOT="$BLOOM_TEST_ROOT/proc"
    export BLOOM_LAUNCH_BIN="$MOCK_BIN/bloom-launch"
    mkdir -p "$BLOOM_PROC_ROOT" "$BLOOM_TEST_ROOT/requests"
    export REQUEST="$BLOOM_TEST_ROOT/requests/game.json"
    printf '%s\n' '{"schema":1}' >"$REQUEST"
    cat >"$BLOOM_LAUNCH_BIN" <<'EOF'
#!/bin/sh
[ "$1" = validate ] && [ -f "$2" ]
EOF
    chmod +x "$BLOOM_LAUNCH_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "session follows the explicit successful lifecycle" {
    run "$SESSION" start "$REQUEST"
    [ "$status" -eq 0 ]
    "$SESSION" transition PREPARING STARTING
    mkdir -p "$BLOOM_PROC_ROOT/123"
    "$SESSION" attach 123
    "$SESSION" transition RUNNING STOP_REQUESTED
    "$SESSION" transition STOP_REQUESTED FLUSHING
    "$SESSION" transition FLUSHING STOPPED

    run "$SESSION" status --json
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"state":"STOPPED"'
    printf '%s' "$output" | grep -F '"pid":123'
    grep -Eq '^[0-9a-f]{64}$' "$BLOOM_SESSION_ROOT/request_sha256"
}

@test "invalid and stale transitions leave state unchanged" {
    "$SESSION" start "$REQUEST"

    run "$SESSION" transition PREPARING RUNNING
    [ "$status" -eq 1 ]
    grep -Fx PREPARING "$BLOOM_SESSION_ROOT/state"

    run "$SESSION" transition STARTING RUNNING
    [ "$status" -eq 1 ]
    grep -Fx PREPARING "$BLOOM_SESSION_ROOT/state"
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
