#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export RUNNER=/workspace/static/build/.tmp_update/bin/bloom-launch-run
    export BLOOM_SESSION_ROOT="$BLOOM_TEST_ROOT/session"
    export BLOOM_LAUNCH_BIN="$MOCK_BIN/bloom-launch"
    export BLOOM_LAUNCH_OVERRIDE_BIN="$MOCK_BIN/bloom-launch-override"
    export BLOOM_JQ_BIN=/usr/bin/jq
    mkdir -p "$BLOOM_SESSION_ROOT"
    export REQUEST="$BLOOM_SESSION_ROOT/request.json"
    printf '%s\n' '{"append_configs":["PLACEHOLDER"]}' | sed "s|PLACEHOLDER|$BLOOM_SESSION_ROOT/ra.cfg|" >"$REQUEST"
    printf '%s\n' private-config >"$BLOOM_SESSION_ROOT/ra.cfg"

    cat >"$BLOOM_LAUNCH_BIN" <<'EOF'
#!/bin/sh
case "$1:$3" in
validate:) exit 0 ;;
get:launcher) printf '%s\n' "$SDCARD/Emu/GB/launch.sh" ;;
get:rom_path) printf '%s\n' "$SDCARD/Roms/GB/Test.gb" ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_LAUNCH_BIN"
    cat >"$BLOOM_LAUNCH_OVERRIDE_BIN" <<'EOF'
#!/bin/sh
case "$1" in
create)
    [ "$3" = "$BLOOM_SESSION_ROOT/ra.cfg" ] || exit 1
    temporary="$BLOOM_TEST_ROOT/temporary-launcher"
    printf '#!/bin/sh\nprintf "%%s\\n" "$1" >"$BLOOM_TEST_ROOT/launched-rom"\n' >"$temporary"
    chmod +x "$temporary"
    printf '%s\n' "$temporary"
    ;;
remove) rm -f "$2" ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_LAUNCH_OVERRIDE_BIN"
}

teardown() { teardown_bloom_fixture; }

@test "session runner applies the private config and removes its temporary launcher" {
    run "$RUNNER" "$REQUEST"
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/launched-rom")" = "$SDCARD/Roms/GB/Test.gb" ]
    [ ! -e "$BLOOM_TEST_ROOT/temporary-launcher" ]
}

@test "session runner rejects requests and configs outside its private root" {
    run "$RUNNER" "$BLOOM_TEST_ROOT/outside.json"
    [ "$status" -eq 1 ]

    printf '%s\n' '{"append_configs":["/tmp/arbitrary.cfg"]}' >"$REQUEST"
    run "$RUNNER" "$REQUEST"
    [ "$status" -eq 1 ]
}
