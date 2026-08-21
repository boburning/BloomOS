#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIN_RAW_VOLUME (-60)
#define MAX_RAW_VOLUME 30

static const char *fifo_path(void)
{
    const char *override = getenv("BLOOM_AUDIO_IOCTL_FIFO");
    return override != NULL && override[0] != '\0' ? override : "/tmp/audio_fifo_ioctl_req";
}

static int fifo_available(void)
{
    struct stat info;
    return lstat(fifo_path(), &info) == 0 && S_ISFIFO(info.st_mode);
}

static int parse_raw(const char *text, int32_t *value)
{
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < MIN_RAW_VOLUME ||
        parsed > MAX_RAW_VOLUME)
        return 0;
    *value = (int32_t)parsed;
    return 1;
}

static int apply_raw(int32_t raw)
{
    int32_t request[6] = {-1, raw, 0, 0, 0, 0};
    int descriptor = open(fifo_path(), O_WRONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0)
        return 0;
    ssize_t written;
    do {
        written = write(descriptor, request, sizeof(request));
    } while (written < 0 && errno == EINTR);
    int saved_errno = errno;
    close(descriptor);
    errno = saved_errno;
    return written == (ssize_t)sizeof(request);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        printf("{\"schema\":1,\"service\":\"bloom-volume\",\"available\":%s,\"backend\":\"audio_server_fifo\"}\n",
               fifo_available() ? "true" : "false");
        return 0;
    }
    int32_t raw = 0;
    if (argc != 3 || strcmp(argv[1], "apply") != 0 || !parse_raw(argv[2], &raw)) {
        fputs("{\"schema\":1,\"service\":\"bloom-volume\",\"applied\":false,\"state\":\"invalid_arguments\"}\n",
              stdout);
        return 2;
    }
    if (!apply_raw(raw)) {
        fputs("{\"schema\":1,\"service\":\"bloom-volume\",\"applied\":false,\"state\":\"backend_unavailable\"}\n",
              stdout);
        return 1;
    }
    printf("{\"schema\":1,\"service\":\"bloom-volume\",\"applied\":true,\"state\":\"applied\",\"raw\":%d}\n",
           raw);
    return 0;
}
