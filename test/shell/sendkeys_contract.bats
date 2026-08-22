#!/usr/bin/env bats

@test "hardware input helper emits complete checked evdev frames" {
    source_file=/workspace/src/sendkeys/sendkeys.c

    grep -F 'memset(events, 0, sizeof(events));' "$source_file"
    grep -F 'struct input_event sync_event = {.type = EV_SYN, .code = SYN_REPORT, .value = 0};' "$source_file"
    grep -F 'write(input_fd, &sync_event, sizeof(sync_event)) != sizeof(sync_event)' "$source_file"
    grep -F 'if (input_fd < 0)' "$source_file"
}
