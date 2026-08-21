#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-time
    export BLOOM_TIME_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_DATE_BIN="$BATS_TEST_TMPDIR/date"
    export BLOOM_PLAY_ACTIVITY_BIN="$BATS_TEST_TMPDIR/playActivity"
    export DATE_LOG="$BATS_TEST_TMPDIR/date.log"
    export ACTIVITY_LOG="$BATS_TEST_TMPDIR/activity.log"
    export MOCK_EPOCH=1800000000
    mkdir -p "$BLOOM_TIME_ROOT/tmp" "$BLOOM_TIME_ROOT/mnt/SDCARD/Saves/CurrentProfile/saves" \
        "$BLOOM_TIME_ROOT/mnt/SDCARD/.tmp_update/config/startup"
    cat >"$BLOOM_PLATFORM_BIN" <<'SH'
#!/bin/sh
printf '%s\n' false
SH
    cat >"$BLOOM_DATE_BIN" <<'SH'
#!/bin/sh
if [ "$#" -eq 1 ] && [ "$1" = +%s ]; then printf '%s\n' "$MOCK_EPOCH"; exit 0; fi
printf '%s\n' "$*" >>"$DATE_LOG"
SH
    cat >"$BLOOM_PLAY_ACTIVITY_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >>"$ACTIVITY_LOG"
SH
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_DATE_BIN" "$BLOOM_PLAY_ACTIVITY_BIN"
}

@test "status separates RTC capability from usable wall clock" {
    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-time","available":true,"rtc":true,"clock_usable":true,"state":"ready"}' ]

    export MOCK_EPOCH=5
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [[ "$output" == *'"rtc":true,"clock_usable":false,"state":"clock_invalid"'* ]]
}

@test "reconcile preserves a valid RTC clock and publishes capability state" {
    printf '#!/bin/sh\nprintf true\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"rtc":true,"applied":false,"state":"rtc_preserved"'* ]]
    [ -f "$BLOOM_TIME_ROOT/tmp/rtc_available" ]
    [ ! -e "$DATE_LOG" ]
    [ ! -e "$ACTIVITY_LOG" ]
}

@test "reconcile preserves an already usable system clock without RTC" {
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"state":"system_clock_preserved"'* ]]
    [ ! -e "$DATE_LOG" ]
}

@test "reconcile restores validated saved time and closes legacy activity" {
    export MOCK_EPOCH=5
    printf 1000 >"$BLOOM_TIME_ROOT/mnt/SDCARD/Saves/CurrentProfile/saves/currentTime.txt"
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [[ "$output" == *'"applied":true,"state":"saved_time_restored"'* ]]
    [ "$(sed -n '1p' "$DATE_LOG")" = '+%s -s @1000' ]
    [ "$(sed -n '2p' "$DATE_LOG")" = '+%s -s @15400' ]
    [ "$(cat "$ACTIVITY_LOG")" = stop_all ]
}

@test "enabled network time suppresses the compatibility offset" {
    export MOCK_EPOCH=5
    printf 1000 >"$BLOOM_TIME_ROOT/mnt/SDCARD/Saves/CurrentProfile/saves/currentTime.txt"
    touch "$BLOOM_TIME_ROOT/mnt/SDCARD/.tmp_update/config/.ntpState"
    run "$SERVICE" reconcile
    [ "$status" -eq 0 ]
    [ "$(sed -n '2p' "$DATE_LOG")" = '+%s -s @1000' ]
}

@test "invalid fallback data fails before clock or activity mutation" {
    export MOCK_EPOCH=5
    saved="$BLOOM_TIME_ROOT/mnt/SDCARD/Saves/CurrentProfile/saves/currentTime.txt"
    printf invalid >"$saved"
    run "$SERVICE" reconcile
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"saved_time_invalid"'* ]]
    [ ! -e "$DATE_LOG" ]
    [ ! -e "$ACTIVITY_LOG" ]

    outside="$BATS_TEST_TMPDIR/outside-time"
    printf 1000 >"$outside"
    rm "$saved"
    ln -s "$outside" "$saved"
    run "$SERVICE" reconcile
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"saved_time_unavailable"'* ]]
    [ ! -e "$DATE_LOG" ]

    rm "$saved"
    printf 1000 >"$saved"
    printf 99 >"$BLOOM_TIME_ROOT/mnt/SDCARD/.tmp_update/config/startup/addHours"
    run "$SERVICE" reconcile
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"offset_invalid"'* ]]
    [ ! -e "$DATE_LOG" ]
}

@test "boot delegates wall-clock reconciliation only to Bloom" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F '$sysdir/bin/bloom-time reconcile' "$runtime"
    ! grep -F 'date +%s -s @$currentTime' "$runtime"
    ! grep -F 'playActivity stop_all' "$runtime"
}
