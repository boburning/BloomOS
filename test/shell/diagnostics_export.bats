#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export BLOOM_ROOT="$BLOOM_TEST_ROOT"
    export EXPORTER=/workspace/static/build/.tmp_update/bin/bloom-diagnostics-export
    export BLOOMCTL=/workspace/static/build/.tmp_update/bin/bloomctl
    export BLOOM_DIAGNOSTICS_NOW=20260817T150000Z
    mkdir -p "$BLOOM_ROOT/proc" "$BLOOM_ROOT/appconfigs" "$SDCARD/.tmp_update/logs" "$SDCARD/.bloom/logs"
    printf 'MemTotal: 128 kB\nMemFree: 64 kB\n' >"$BLOOM_ROOT/proc/meminfo"
    printf 'safe update log\n' >"$SDCARD/.tmp_update/logs/bloom-update-boot.log"
    printf 'safe shutdown log\n' >"$BLOOM_ROOT/appconfigs/bloom-shutdown.log"
    printf '%s\n' '{"schema":1,"event":"finish","outcome":"stopped","detail":"60"}' >"$SDCARD/.bloom/logs/retroachievements.log"
    printf 'secret network log\n' >"$SDCARD/.tmp_update/logs/network.log"
    mkdir -p "$SDCARD/Roms" "$SDCARD/Saves" "$SDCARD/.ssh"
    printf 'private rom name\n' >"$SDCARD/Roms/Private Game.zip"
    printf 'private save\n' >"$SDCARD/Saves/private.sav"
    printf 'private key\n' >"$SDCARD/.ssh/id_ed25519"

    cat >"$MOCK_BIN/bloomctl-fixture" <<'EOF'
#!/bin/sh
case "$*" in
  'info --json') printf '%s\n' '{"schema":1,"model":"mini_plus"}' ;;
  'health --json') printf '%s\n' '{"schema":1,"healthy":true}' ;;
  'update status') printf '%s\n' '{"schema":1,"state":"idle"}' ;;
  'saves snapshots') printf '%s\n' '[]' ;;
  *) exit 2 ;;
esac
EOF
    chmod +x "$MOCK_BIN/bloomctl-fixture"
    export BLOOMCTL_BIN="$MOCK_BIN/bloomctl-fixture"
}

teardown() {
    teardown_bloom_fixture
}

@test "export publishes an allowlisted archive without user data or secrets" {
    run sh "$EXPORTER"
    [ "$status" -eq 0 ]
    archive="$SDCARD/BloomDiagnostics/diagnostics-$BLOOM_DIAGNOSTICS_NOW.tar.gz"
    [ "$output" = "$archive" ]
    [ -s "$archive" ]

    listing="$(tar -tzf "$archive")"
    printf '%s' "$listing" | grep -F './manifest.json'
    printf '%s' "$listing" | grep -F './info.json'
    printf '%s' "$listing" | grep -F './logs/bloom-update-boot.log'
    printf '%s' "$listing" | grep -F './logs/bloom-shutdown.log'
    printf '%s' "$listing" | grep -F './logs/retroachievements.log'
    ! printf '%s' "$listing" | grep -E 'network|Private|id_ed25519|Roms|Saves'

    extracted="$BLOOM_TEST_ROOT/extracted"
    mkdir "$extracted"
    tar -xzf "$archive" -C "$extracted"
    grep -F '"privacy": "allowlisted"' "$extracted/manifest.json"
    grep -F '"model":"mini_plus"' "$extracted/info.json"
    ! grep -R -E 'secret network|private rom|private save|private key' "$extracted"
}

@test "bloomctl exposes only the explicit logs export operation" {
    export BLOOM_DIAGNOSTICS_EXPORT_BIN="$EXPORTER"
    run sh "$BLOOMCTL" logs export
    [ "$status" -eq 0 ]
    [ -s "$SDCARD/BloomDiagnostics/diagnostics-$BLOOM_DIAGNOSTICS_NOW.tar.gz" ]

    run sh "$BLOOMCTL" logs collect
    [ "$status" -eq 2 ]
}

@test "export refuses symlinked output and does not overwrite an archive" {
    mkdir "$BLOOM_TEST_ROOT/elsewhere"
    ln -s "$BLOOM_TEST_ROOT/elsewhere" "$SDCARD/BloomDiagnostics"
    run sh "$EXPORTER"
    [ "$status" -eq 1 ]
    [ ! -e "$BLOOM_TEST_ROOT/elsewhere/diagnostics-$BLOOM_DIAGNOSTICS_NOW.tar.gz" ]

    rm "$SDCARD/BloomDiagnostics"
    run sh "$EXPORTER"
    [ "$status" -eq 0 ]
    run sh "$EXPORTER"
    [ "$status" -eq 1 ]
}
