#!/usr/bin/env bats

setup() {
    export SERVICE=/workspace/static/build/.tmp_update/bin/bloom-controls
    export BLOOM_CONTROLS_ROOT="$BATS_TEST_TMPDIR/root"
    export BLOOM_PLATFORM_BIN="$BATS_TEST_TMPDIR/bloom-platform"
    export BLOOM_SETTINGS_BIN="$BATS_TEST_TMPDIR/bloom-settings"
    export BLOOM_VOLUME_BIN="$BATS_TEST_TMPDIR/bloom-volume"
    export SETTINGS_LOG="$BATS_TEST_TMPDIR/settings.log"
    export VOLUME_LOG="$BATS_TEST_TMPDIR/volume.log"
    duty="$BLOOM_CONTROLS_ROOT/sys/class/pwm/pwmchip0/pwm0/duty_cycle"
    mkdir -p "$(dirname "$duty")"
    printf 35 >"$duty"
    cat >"$BLOOM_PLATFORM_BIN" <<'SH'
#!/bin/sh
printf '%s\n' mini_plus
SH
    cat >"$BLOOM_SETTINGS_BIN" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >>"$SETTINGS_LOG"
SH
    cat >"$BLOOM_VOLUME_BIN" <<'SH'
#!/bin/sh
if [ "$1" = status ]; then
    printf '%s\n' '{"schema":1,"service":"bloom-volume","available":true,"backend":"audio_server_fifo"}'
else
    printf '%s\n' "$*" >>"$VOLUME_LOG"
fi
SH
    chmod +x "$BLOOM_PLATFORM_BIN" "$BLOOM_SETTINGS_BIN" "$BLOOM_VOLUME_BIN"
}

@test "status reports bounded raw brightness without changing it" {
    run "$SERVICE" status
    [ "$status" -eq 0 ]
    [ "$output" = '{"schema":1,"service":"bloom-controls","available":true,"model":"mini_plus","brightness":{"available":true,"raw":35},"volume":{"schema":1,"service":"bloom-volume","available":true,"backend":"audio_server_fifo"}}' ]
    [ "$(cat "$duty")" = 35 ]
    [ ! -e "$SETTINGS_LOG" ]
}

@test "internal volume apply uses the firmware audio-server curve without persisting" {
    run "$SERVICE" apply volume 0
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":0,"raw":-60,"persisted":false,"applied":true'* ]]
    [ "$(cat "$VOLUME_LOG")" = 'apply -60' ]
    [ ! -e "$SETTINGS_LOG" ]

    run "$SERVICE" apply volume 20
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":20,"raw":3'* ]]
    [ "$(tail -n 1 "$VOLUME_LOG")" = 'apply 3' ]
}

@test "public volume request persists the level and clears mute before applying" {
    run "$SERVICE" request volume 10
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":10,"raw":-10,"persisted":true,"applied":true'* ]]
    [ "$(cat "$SETTINGS_LOG")" = $'set device.volume 10\nset device.mute false' ]
    [ "$(cat "$VOLUME_LOG")" = 'apply -10' ]
}

@test "public mute request persists mute and applies silence" {
    run "$SERVICE" request mute true
    [ "$status" -eq 0 ]
    [[ "$output" == *'"control":"mute","value":true,"persisted":true,"applied":true'* ]]
    [ "$(cat "$SETTINGS_LOG")" = 'set device.mute true' ]
    [ "$(cat "$VOLUME_LOG")" = 'apply -60' ]

    run "$SERVICE" request mute false
    [ "$status" -eq 2 ]
}

@test "volume rejects invalid values and separates persistence from apply failure" {
    run "$SERVICE" request volume 21
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_value"'* ]]
    [ ! -e "$SETTINGS_LOG" ]

    cat >"$BLOOM_VOLUME_BIN" <<'SH'
#!/bin/sh
if [ "$1" = status ]; then
    printf '%s\n' '{"schema":1,"service":"bloom-volume","available":true,"backend":"audio_server_fifo"}'
    exit 0
fi
exit 1
SH
    chmod +x "$BLOOM_VOLUME_BIN"
    run "$SERVICE" request volume 5
    [ "$status" -eq 1 ]
    [[ "$output" == *'"persisted":true,"applied":false,"state":"apply_failed"'* ]]
    [ "$(cat "$SETTINGS_LOG")" = $'set device.volume 5\nset device.mute false' ]
}

@test "volume rejects an unavailable backend before changing settings" {
    cat >"$BLOOM_VOLUME_BIN" <<'SH'
#!/bin/sh
printf '%s\n' '{"schema":1,"service":"bloom-volume","available":false,"backend":"audio_server_fifo"}'
SH
    chmod +x "$BLOOM_VOLUME_BIN"
    run "$SERVICE" request volume 5
    [ "$status" -eq 1 ]
    [[ "$output" == *'"persisted":false,"applied":false,"state":"control_unavailable"'* ]]
    [ ! -e "$SETTINGS_LOG" ]
}

@test "internal apply owns the canonical brightness curve without persisting" {
    run "$SERVICE" apply brightness 0
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":0,"raw":3,"persisted":false,"applied":true'* ]]
    [ "$(cat "$duty")" = 3 ]
    [ ! -e "$SETTINGS_LOG" ]

    run "$SERVICE" apply brightness 10
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":10,"raw":100'* ]]
    [ "$(cat "$duty")" = 100 ]
}

@test "public brightness request persists the canonical field before applying" {
    run "$SERVICE" request brightness 7
    [ "$status" -eq 0 ]
    [[ "$output" == *'"value":7,"raw":35,"persisted":true,"applied":true'* ]]
    [ "$(cat "$SETTINGS_LOG")" = 'set device.brightness 7' ]
    [ "$(cat "$duty")" = 35 ]
}

@test "brightness rejects invalid values and unknown hardware before mutation" {
    run "$SERVICE" request brightness 11
    [ "$status" -eq 2 ]
    [[ "$output" == *'"state":"invalid_value"'* ]]
    [ ! -e "$SETTINGS_LOG" ]

    printf '#!/bin/sh\nprintf unknown\n' >"$BLOOM_PLATFORM_BIN"
    chmod +x "$BLOOM_PLATFORM_BIN"
    run "$SERVICE" request brightness 7
    [ "$status" -eq 1 ]
    [[ "$output" == *'"state":"platform_unavailable"'* ]]
    [ ! -e "$SETTINGS_LOG" ]
}

@test "brightness distinguishes settings rejection from unavailable control" {
    printf '#!/bin/sh\nexit 1\n' >"$BLOOM_SETTINGS_BIN"
    chmod +x "$BLOOM_SETTINGS_BIN"
    run "$SERVICE" request brightness 6
    [ "$status" -eq 1 ]
    [[ "$output" == *'"persisted":false,"applied":false,"state":"settings_rejected"'* ]]

    printf '#!/bin/sh\nexit 0\n' >"$BLOOM_SETTINGS_BIN"
    chmod +x "$BLOOM_SETTINGS_BIN"
    rm "$duty"
    run "$SERVICE" request brightness 6
    [ "$status" -eq 1 ]
    [[ "$output" == *'"persisted":true,"applied":false,"state":"control_unavailable"'* ]]
}

@test "boot applies brightness only through the Bloom control adapter" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F '$sysdir/bin/bloom-controls apply brightness "$brightness"' "$runtime"
    ! grep -F 'brightness_raw=$(awk' "$runtime"
    ! grep -F 'echo $brightness_raw > /sys/class/pwm/pwmchip0/pwm0/duty_cycle' "$runtime"
}
