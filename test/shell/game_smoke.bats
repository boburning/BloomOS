#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export GAME_SMOKE=/workspace/static/build/.tmp_update/bin/bloom-game-smoke
    export BLOOM_SD_ROOT="$SDCARD"
    mkdir -p "$SDCARD/.tmp_update/config" "$SDCARD/Roms/GB" "$SDCARD/Roms/GBC" "$SDCARD/Emu/GB"
}

teardown() { teardown_bloom_fixture; }

encode() { printf '%s' "$1" | base64 | tr -d '\r\n'; }

@test "game smoke requires explicit developer mode" {
    rom="$SDCARD/Roms/GB/Test.gb"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"developer_mode_required"'
}

@test "game smoke rejects unsupported systems before launch" {
    : >"$SDCARD/.bloom-dev"

    run "$GAME_SMOKE" N64 "$(encode "$SDCARD/Roms/N64/Test.z64")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"unsupported_system"'
}

@test "game smoke rejects durations beyond the bounded soak limit" {
    : >"$SDCARD/.bloom-dev"
    rom="$SDCARD/Roms/GB/Test.gb"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 901

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"invalid_duration"'
}

@test "game smoke confines ROMs to the selected system directory" {
    : >"$SDCARD/.bloom-dev"
    rom="$SDCARD/Roms/GBC/Test.gbc"
    : >"$rom"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"rom_outside_system_directory"'
}

@test "game smoke maps PICO to Fake-08 through structured launch data" {
    : >"$SDCARD/.bloom-dev"
    mkdir -p "$SDCARD/Roms/PICO" "$SDCARD/Emu/PICO"
    rom="$SDCARD/Roms/PICO/Bloom Probe.p8"
    : >"$rom"
    printf '#!/bin/sh\n' >"$SDCARD/Emu/PICO/launch.sh"
    chmod +x "$SDCARD/Emu/PICO/launch.sh"
    cat >"$SDCARD/.tmp_update/bin/bloom-launch" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$MOCK_LOG"
exit 1
EOF
    chmod +x "$SDCARD/.tmp_update/bin/bloom-launch"
    cat >"$SDCARD/.tmp_update/bin/bloom-game-id" <<'EOF'
#!/bin/sh
printf '%s\n' bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
EOF
    chmod +x "$SDCARD/.tmp_update/bin/bloom-game-id"
    printf '#!/bin/sh\nexit 0\n' >"$SDCARD/.tmp_update/bin/bloom-session"
    chmod +x "$SDCARD/.tmp_update/bin/bloom-session"
    cat >"$MOCK_BIN/pidof" <<'EOF'
#!/bin/sh
exit 0
EOF
    chmod +x "$MOCK_BIN/pidof"

    run "$GAME_SMOKE" PICO "$(encode "$rom")" 60

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"structured_request_failed"'
    grep -F "pico8 $rom $SDCARD/Emu/PICO/launch.sh retroarch fake08_libretro.so false" "$MOCK_LOG"
}

@test "game smoke routes command-sensitive ROM paths through structured data" {
    : >"$SDCARD/.bloom-dev"
    rom="$SDCARD/Roms/GB/Unsafe\$Name.gb"
    : >"$rom"
    printf '#!/bin/sh\n' >"$SDCARD/Emu/GB/launch.sh"
    chmod +x "$SDCARD/Emu/GB/launch.sh"
    cat >"$SDCARD/.tmp_update/bin/bloom-launch" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$MOCK_LOG"
exit 1
EOF
    chmod +x "$SDCARD/.tmp_update/bin/bloom-launch"
    cat >"$SDCARD/.tmp_update/bin/bloom-game-id" <<'EOF'
#!/bin/sh
printf '%s\n' bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
EOF
    chmod +x "$SDCARD/.tmp_update/bin/bloom-game-id"
    printf '#!/bin/sh\nexit 0\n' >"$SDCARD/.tmp_update/bin/bloom-session"
    chmod +x "$SDCARD/.tmp_update/bin/bloom-session"
    cat >"$MOCK_BIN/pidof" <<'EOF'
#!/bin/sh
exit 0
EOF
    chmod +x "$MOCK_BIN/pidof"

    run "$GAME_SMOKE" GB "$(encode "$rom")" 10

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F '"reason":"structured_request_failed"'
    grep -F "create $SDCARD/.tmp_update/bloom-launch-request.json bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef gb $rom" "$MOCK_LOG"
    [ ! -e "$SDCARD/.tmp_update/bloom-launch-request.json" ]
}
