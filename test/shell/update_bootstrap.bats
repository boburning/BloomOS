#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export BOOTSTRAP=/workspace/static/build/.tmp_update/bin/bloom-update-bootstrap
    export BLOOM_UPDATE_STATE_BIN="$BLOOM_TEST_ROOT/mock-state"
    export BLOOM_HEALTH_BIN="$BLOOM_TEST_ROOT/mock-health"
    cat >"$BLOOM_UPDATE_STATE_BIN" <<'EOF'
#!/bin/sh
printf '%s:%s\n' "$1" "$2" >>"$BLOOM_TEST_ROOT/calls"
EOF
    cat >"$BLOOM_HEALTH_BIN" <<'EOF'
#!/bin/sh
[ "$1" = health ] && [ "$2" = --json ] || exit 2
[ "${HEALTHY:-true}" = true ]
EOF
    chmod +x "$BLOOM_UPDATE_STATE_BIN" "$BLOOM_HEALTH_BIN"
}

@test "records a baseline only after structured health passes" {
    run env BLOOM_TEST_ROOT="$BLOOM_TEST_ROOT" "$BOOTSTRAP" 1.2.3

    [ "$status" -eq 0 ]
    grep -Fx 'bootstrap:1.2.3' "$BLOOM_TEST_ROOT/calls"
}

@test "does not mutate baseline state when health fails" {
    run env BLOOM_TEST_ROOT="$BLOOM_TEST_ROOT" HEALTHY=false "$BOOTSTRAP" 1.2.3

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'device health checks failed'
    [ ! -e "$BLOOM_TEST_ROOT/calls" ]
}
