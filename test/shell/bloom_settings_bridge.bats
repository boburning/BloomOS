#!/usr/bin/env bats

bats_require_minimum_version 1.5.0

SETTINGS_HEADER=/workspace/src/common/system/settings.h

@test "legacy settings saves invoke the fixed Bloom compatibility service without a shell" {
    run grep -F '#define BLOOM_SETTINGS_SERVICE "/mnt/SDCARD/.tmp_update/bin/bloom-settings"' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]

    run grep -F 'execl(BLOOM_SETTINGS_SERVICE, BLOOM_SETTINGS_SERVICE, "sync-onion", (char *)NULL);' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]

    run grep -c '_settings_sync_bloom_compatibility();' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]
    [ "$output" -eq 2 ]

    run grep -E 'system\(|popen\(' "$SETTINGS_HEADER"
    [ "$status" -eq 1 ]
}
