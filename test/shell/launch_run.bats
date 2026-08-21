#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export RUNNER=/workspace/static/build/.tmp_update/bin/bloom-launch-run
    export BLOOM_SESSION_ROOT="$BLOOM_TEST_ROOT/session"
    export BLOOM_LAUNCH_BIN="$MOCK_BIN/bloom-launch"
    export BLOOM_LAUNCH_OVERRIDE_BIN="$MOCK_BIN/bloom-launch-override"
    export BLOOM_SESSION_BIN="$MOCK_BIN/bloom-session"
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_PROC_ROOT="$BLOOM_TEST_ROOT/proc"
    mkdir -p "$BLOOM_SESSION_ROOT" "$BLOOM_PROC_ROOT"
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
    printf '#!/bin/sh\nmkdir -p "$BLOOM_PROC_ROOT/321"\nprintf "retroarch\\n" >"$BLOOM_PROC_ROOT/321/comm"\nprintf "%%s\\n" "$1" >"$BLOOM_TEST_ROOT/launched-rom"\n' >"$temporary"
    chmod +x "$temporary"
    printf '%s\n' "$temporary"
    ;;
remove) rm -f "$2" ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_LAUNCH_OVERRIDE_BIN"
    cat >"$BLOOM_SESSION_BIN" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$MOCK_LOG"
case "$1" in
attach) rm -rf "$BLOOM_PROC_ROOT/$2" ;;
observe-exit|flush-saves|complete) ;;
*) exit 1 ;;
esac
EOF
    chmod +x "$BLOOM_SESSION_BIN"
    mkdir -p "$SDCARD/Emu/GB"
    cat >"$SDCARD/Emu/GB/launch.sh" <<'EOF'
#!/bin/sh
mkdir -p "$BLOOM_PROC_ROOT/321"
printf 'retroarch\n' >"$BLOOM_PROC_ROOT/321/comm"
printf '%s\n' "$1" >"$BLOOM_TEST_ROOT/launched-rom"
EOF
    chmod +x "$SDCARD/Emu/GB/launch.sh"
}

teardown() { teardown_bloom_fixture; }

@test "session runner applies the private config and removes its temporary launcher" {
    run "$RUNNER" "$REQUEST"
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/launched-rom")" = "$SDCARD/Roms/GB/Test.gb" ]
    [ ! -e "$BLOOM_TEST_ROOT/temporary-launcher" ]
    grep -Fx 'attach 321' "$MOCK_LOG"
    grep -Fx observe-exit "$MOCK_LOG"
    grep -Fx flush-saves "$MOCK_LOG"
    grep -Fx complete "$MOCK_LOG"
}

@test "session runner supports direct sessions without an append config" {
    printf '%s\n' '{"append_configs":[]}' >"$REQUEST"
    run "$RUNNER" "$REQUEST"
    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/launched-rom")" = "$SDCARD/Roms/GB/Test.gb" ]
}

@test "session runner rejects requests and configs outside its private root" {
    run "$RUNNER" "$BLOOM_TEST_ROOT/outside.json"
    [ "$status" -eq 1 ]

    printf '%s\n' '{"append_configs":["/tmp/arbitrary.cfg"]}' >"$REQUEST"
    run "$RUNNER" "$REQUEST"
    [ "$status" -eq 1 ]
}
