setup_bloom_fixture() {
    export BLOOM_TEST_ROOT="$(mktemp -d)"
    export SDCARD="$BLOOM_TEST_ROOT/mnt/SDCARD"
    export MOCK_BIN="$BLOOM_TEST_ROOT/mock-bin"
    export MOCK_LOG="$BLOOM_TEST_ROOT/mock-calls.log"

    mkdir -p \
        "$MOCK_BIN" \
        "$SDCARD/.tmp_update/bin" \
        "$SDCARD/.tmp_update/config" \
        "$SDCARD/.tmp_update/onionVersion" \
        "$SDCARD/App/PackageManager/data" \
        "$SDCARD/Emu" \
        "$SDCARD/RApp" \
        "$SDCARD/RetroArch/.retroarch" \
        "$SDCARD/Saves/CurrentProfile"

    PATH="$MOCK_BIN:$PATH"
    export PATH
}

teardown_bloom_fixture() {
    if [ -n "${BLOOM_TEST_ROOT:-}" ] && [ -d "$BLOOM_TEST_ROOT" ]; then
        rm -rf -- "$BLOOM_TEST_ROOT"
    fi
}

mock_command() {
    command_name="$1"
    cat >"$MOCK_BIN/$command_name" <<'EOF'
#!/bin/sh
printf '%s\n' "$0 $*" >>"$MOCK_LOG"
EOF
    chmod +x "$MOCK_BIN/$command_name"
}
