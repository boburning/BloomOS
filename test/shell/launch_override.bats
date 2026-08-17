#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export OVERRIDE=/workspace/static/build/.tmp_update/bin/bloom-launch-override
    export BLOOM_SD_ROOT="$SDCARD"
    export LAUNCHER="$SDCARD/Emu/GB/launch.sh"
    mkdir -p "$(dirname "$LAUNCHER")"
    cat >"$LAUNCHER" <<'EOF'
#!/bin/sh
progdir=$(dirname "$0")
cd /mnt/SDCARD/RetroArch
HOME=/mnt/SDCARD/RetroArch "$progdir/../../RetroArch/retroarch" -v -L core.so "$1"
EOF
    chmod 755 "$LAUNCHER"
}

teardown() { teardown_bloom_fixture; }

@test "creates an adjacent override without changing the permanent launcher" {
    before="$(sha256sum "$LAUNCHER" | awk '{print $1}')"

    run "$OVERRIDE" create "$LAUNCHER" /tmp/reset.cfg

    [ "$status" -eq 0 ]
    temporary="$output"
    [ "$(dirname "$temporary")" = "$(dirname "$LAUNCHER")" ]
    [ "$(sha256sum "$LAUNCHER" | awk '{print $1}')" = "$before" ]
    grep -F -- '-v --appendconfig "/tmp/reset.cfg"' "$temporary"
    [ "$(stat -c '%a' "$temporary")" = 700 ]

    run "$OVERRIDE" remove "$temporary"
    [ "$status" -eq 0 ]
    [ ! -e "$temporary" ]
}

@test "supports auto-load state without modifying launcher bytes" {
    before="$(sha256sum "$LAUNCHER" | awk '{print $1}')"

    run "$OVERRIDE" create "$LAUNCHER" /tmp/auto_load_state.cfg

    [ "$status" -eq 0 ]
    grep -F -- '-v --appendconfig "/tmp/auto_load_state.cfg"' "$output"
    [ "$(sha256sum "$LAUNCHER" | awk '{print $1}')" = "$before" ]
}

@test "rejects external launchers links and arbitrary configs" {
    outside="$BLOOM_TEST_ROOT/launch.sh"
    cp "$LAUNCHER" "$outside"
    run "$OVERRIDE" create "$outside" /tmp/reset.cfg
    [ "$status" -eq 1 ]

    linked="$SDCARD/Emu/GB-link/launch.sh"
    mkdir -p "$(dirname "$linked")"
    ln -s "$LAUNCHER" "$linked"
    run "$OVERRIDE" create "$linked" /tmp/reset.cfg
    [ "$status" -eq 1 ]

    run "$OVERRIDE" create "$LAUNCHER" /tmp/arbitrary.cfg
    [ "$status" -eq 1 ]
}

@test "runtime never edits permanent launchers for temporary state" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    run grep -F 'sed -i' "$runtime"
    [[ "$output" != *appendconfig* ]]
    grep -F 'bloom-launch-override create' "$runtime"
    grep -F 'bloom-launch-override remove' "$runtime"
}

@test "runtime command restoration treats ROM punctuation as data" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    functions="$BLOOM_TEST_ROOT/functions.sh"
    sed -n '/^shell_quote()/,/^launch_game_postprocess()/p' "$runtime" | sed '$d' >"$functions"
    export sysdir="$SDCARD/.tmp_update"
    export miyoodir="$SDCARD/miyoo"
    mkdir -p "$miyoodir/lib"
    recorder="$BLOOM_TEST_ROOT/record launcher.sh"
    cat >"$recorder" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >"$BLOOM_TEST_ROOT/recorded-rom"
EOF
    chmod 755 "$recorder"
    rom="$SDCARD/Roms/GB/Bob's \$pecial \`Game\`.gb"

    . "$functions"
    write_launch_command "$recorder" "$rom"
    sh -n "$sysdir/cmd_to_run.sh"
    run sh "$sysdir/cmd_to_run.sh"

    [ "$status" -eq 0 ]
    [ "$(cat "$BLOOM_TEST_ROOT/recorded-rom")" = "$rom" ]
}
