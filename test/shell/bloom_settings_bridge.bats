#!/usr/bin/env bats

bats_require_minimum_version 1.5.0

SETTINGS_HEADER=/workspace/src/common/system/settings.h
SETTINGS_SYNC_HEADER=/workspace/src/common/system/settings_sync.h
RUNTIME=/workspace/static/build/.tmp_update/runtime.sh
WPS_CLIENT=/workspace/static/build/.tmp_update/script/network/wpsclient.sh
TWEAKS_ACTIONS=/workspace/src/tweaks/actions.h
TWEAKS_VALUES=/workspace/src/tweaks/values.h

@test "legacy settings saves invoke the fixed Bloom compatibility service without a shell" {
    run grep -F '#define BLOOM_SETTINGS_SERVICE "/mnt/SDCARD/.tmp_update/bin/bloom-settings"' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]

    run grep -F 'execl(BLOOM_SETTINGS_SERVICE, BLOOM_SETTINGS_SERVICE, "reconcile-onion", (char *)NULL);' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]

    run grep -c '_settings_sync_bloom_compatibility();' "$SETTINGS_HEADER"
    [ "$status" -eq 0 ]
    [ "$output" -eq 2 ]

    run grep -F '_settings_sync_bloom_compatibility();' "$SETTINGS_SYNC_HEADER"
    [ "$status" -eq 0 ]

    run grep -E 'system\(|popen\(' "$SETTINGS_HEADER"
    [ "$status" -eq 1 ]
}

@test "Tweaks persists PWM selection through the canonical legacy key" {
    run grep -F 'settings.pwmfrequency = item_value;' "$TWEAKS_ACTIONS"
    [ "$status" -eq 0 ]

    run grep -F 'config_setNumber("pwmfrequency", item_value);' "$TWEAKS_ACTIONS"
    [ "$status" -eq 0 ]

    run grep -F 'config_get("pwmfrequency", CONFIG_INT, &pwmfrequency);' "$TWEAKS_VALUES"
    [ "$status" -eq 0 ]

    run grep -F '".pwmfrequency"' "$TWEAKS_ACTIONS" "$TWEAKS_VALUES"
    [ "$status" -eq 1 ]
}

@test "boot and WPS changes reconcile the inactive canonical settings boundary" {
    run grep -F 'if [ -x "$sysdir/bin/bloom-settings" ] && [ -x "$sysdir/bin/bloomctl" ]; then' "$RUNTIME"
    [ "$status" -eq 0 ]

    run grep -F '"$sysdir/bin/bloomctl" settings import-onion' "$RUNTIME"
    [ "$status" -eq 0 ]

    run grep -F '"$sysdir/bin/bloomctl" settings reconcile-onion' "$RUNTIME"
    [ "$status" -eq 0 ]

    run grep -F '"$sysdir/bin/bloomctl" settings reconcile-onion > /dev/null 2>&1 || true' "$WPS_CLIENT"
    [ "$status" -eq 0 ]
}
