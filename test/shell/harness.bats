#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
}

teardown() {
    teardown_bloom_fixture
}

@test "fixture creates the minimum Onion-compatible SD-card layout" {
    [ -d "$SDCARD/.tmp_update/bin" ]
    [ -d "$SDCARD/App/PackageManager/data" ]
    [ -d "$SDCARD/RetroArch/.retroarch" ]
    [ -d "$SDCARD/Saves/CurrentProfile" ]
}

@test "mock commands are isolated and record calls" {
    mock_command poweroff

    run poweroff --test-only

    [ "$status" -eq 0 ]
    grep -F -- "poweroff --test-only" "$MOCK_LOG"
}

@test "repository is mounted read-only" {
    run sh -c 'printf unsafe > /workspace/.bloom-shell-write-test'

    [ "$status" -ne 0 ]
    [ ! -e /workspace/.bloom-shell-write-test ]
}
