#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/log.h"

int main(int argc, char *argv[])
{
    struct input_event events[100];

    if (argc < 3 || argc % 2 == 0 || argc > 201) {
        printf("Usage: sendkeys [[CODE] [VALUE], ...]\nValues: 0 - released, 1 "
               "- pressed, 2 - repeating\n");
        return 1;
    }

    int num_events = (argc - 1) / 2;
    memset(events, 0, sizeof(events));

    for (int i = 1; i < argc; i += 2) {
        int ev_idx = (i - 1) / 2;
        events[ev_idx].type = EV_KEY;
        events[ev_idx].code = atoi(argv[i]);
        events[ev_idx].value = atoi(argv[i + 1]);
    }

    int input_fd;
    input_fd = open("/dev/input/event0", O_WRONLY);
    if (input_fd < 0)
        return 1;

    for (int j = 0; j < num_events; j++) {
        struct input_event sync_event = {.type = EV_SYN, .code = SYN_REPORT, .value = 0};
        printf_debug("sendkeys: code = %d, value = %d\n", events[j].code,
                     events[j].value);
        if (write(input_fd, &events[j], sizeof(events[j])) != sizeof(events[j]) ||
            write(input_fd, &sync_event, sizeof(sync_event)) != sizeof(sync_event)) {
            close(input_fd);
            return 1;
        }
    }
    close(input_fd);
    sync();
    return 0;
}
