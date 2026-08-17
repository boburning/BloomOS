#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/udp.h"

static int parse_range(const char *text, long minimum, long maximum, long *value)
{
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        return -1;
    *value = parsed;
    return 0;
}

int main(int argc, char *argv[])
{
    char *ipAddress = "127.0.0.1"; // localhost
    int port = 55355;              // default RetroArch CMD port
    char *message = NULL;
    size_t response_size = 0;

    if (argc < 2 || (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))) {
        fprintf(stderr, "Usage: %s [-h <ipAddress>] [-p <port>] [-r <response_size>] <message>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            if (i + 1 < argc) {
                ipAddress = argv[i + 1];
                i++;
            }
        }
        else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                long requested_port = 0;
                if (parse_range(argv[i + 1], 1, 65535, &requested_port) != 0) {
                    fprintf(stderr, "Port must be between 1 and 65535\n");
                    return EXIT_FAILURE;
                }
                port = (int)requested_port;
                i++;
            }
        }
        else if (strcmp(argv[i], "-r") == 0) {
            if (i + 1 < argc) {
                long requested_size = 0;
                if (parse_range(argv[i + 1], 2, 65536, &requested_size) != 0) {
                    fprintf(stderr, "Response size must be between 2 and 65536 bytes\n");
                    return EXIT_FAILURE;
                }
                response_size = (size_t)requested_size;
                i++;
            }
        }
        else {
            message = argv[i];
        }
    }

    if (message == NULL || port < 1 || port > 65535) {
        fprintf(stderr, "A message and valid UDP port are required\n");
        return EXIT_FAILURE;
    }

    if (response_size > 0) {
        char *response = (char *)malloc(response_size);
        if (response == NULL) {
            perror("Failed to allocate memory for response");
            exit(EXIT_FAILURE);
        }

        if (udp_send_receive(ipAddress, port, message, response, response_size) == -1) {
            exit(EXIT_FAILURE);
        }

        printf("%s\n", response);
        free(response);
    }
    else if (udp_send(ipAddress, port, message) == -1) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
