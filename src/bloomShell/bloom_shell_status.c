#include "bloom_shell_status.h"

#include <cjson/cJSON.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define STATUS_OUTPUT_MAX 16384

static int bounded_word(const char *value)
{
    if (value == NULL || value[0] == '\0' || strlen(value) >= 32)
        return 0;
    for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        if (!((*cursor >= 'a' && *cursor <= 'z') || *cursor == '_'))
            return 0;
    return 1;
}

static cJSON *object(cJSON *parent, const char *name)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsObject(value) ? value : NULL;
}

static int boolean(cJSON *parent, const char *name, int *value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (!cJSON_IsBool(item))
        return -1;
    *value = cJSON_IsTrue(item) ? 1 : 0;
    return 0;
}

int bloom_shell_status_parse(const char *json, BloomShellStatus *status)
{
    if (json == NULL || status == NULL)
        return -1;
    memset(status, 0, sizeof(*status));
    cJSON *root = cJSON_Parse(json);
    cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    cJSON *checks = root == NULL ? NULL : object(root, "checks");
    cJSON *system = checks == NULL ? NULL : object(checks, "system");
    cJSON *update = checks == NULL ? NULL : object(checks, "update_state");
    cJSON *ra = checks == NULL ? NULL : object(checks, "retroachievements");
    cJSON *phase = update == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(update, "phase");
    cJSON *ra_state = ra == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(ra, "state");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valueint == 1 && schema->valuedouble == 1.0 &&
        system != NULL && update != NULL && ra != NULL && cJSON_IsString(phase) &&
        bounded_word(phase->valuestring) && cJSON_IsString(ra_state) &&
        bounded_word(ra_state->valuestring) && boolean(root, "healthy", &status->healthy) == 0 &&
        boolean(system, "healthy", &status->system_healthy) == 0 &&
        boolean(update, "healthy", &status->update_healthy) == 0 &&
        boolean(ra, "healthy", &status->ra_healthy) == 0 &&
        boolean(ra, "enabled", &status->ra_enabled) == 0) {
        snprintf(status->update_phase, sizeof(status->update_phase), "%s", phase->valuestring);
        snprintf(status->ra_state, sizeof(status->ra_state), "%s", ra_state->valuestring);
        status->ready = 1;
        result = 0;
    }
    cJSON_Delete(root);
    return result;
}

int bloom_shell_status_load(const char *bloomctl_path, BloomShellStatus *status)
{
    if (bloomctl_path == NULL || bloomctl_path[0] != '/' || status == NULL)
        return -1;
    int output[2];
    if (pipe(output) != 0)
        return -1;
    pid_t child = fork();
    if (child < 0) {
        close(output[0]);
        close(output[1]);
        return -1;
    }
    if (child == 0) {
        close(output[0]);
        if (dup2(output[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(output[1]);
        execl(bloomctl_path, bloomctl_path, "health", "--json", (char *)NULL);
        _exit(127);
    }
    close(output[1]);
    char json[STATUS_OUTPUT_MAX + 1];
    size_t length = 0;
    int failed = 0;
    int complete = 0;
    while (!failed && !complete && length < STATUS_OUTPUT_MAX) {
        struct pollfd descriptor = {.fd = output[0], .events = POLLIN | POLLHUP};
        int ready;
        do {
            ready = poll(&descriptor, 1, 5000);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0) {
            failed = 1;
            break;
        }
        ssize_t count = read(output[0], json + length, STATUS_OUTPUT_MAX - length);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            failed = 1;
            break;
        }
        if (count == 0) {
            complete = 1;
            break;
        }
        length += (size_t)count;
    }
    if (!failed && length == STATUS_OUTPUT_MAX)
        failed = 1;
    close(output[0]);
    if (failed)
        kill(child, SIGKILL);
    int wait_status = 0;
    while (waitpid(child, &wait_status, 0) < 0)
        if (errno != EINTR) {
            failed = 1;
            break;
        }
    json[length] = '\0';
    return failed ? -1 : bloom_shell_status_parse(json, status);
}

static const char *update_label(const BloomShellStatus *status)
{
    if (!status->ready || !status->update_healthy)
        return "Needs attention";
    if (strcmp(status->update_phase, "known_good") == 0)
        return "Up to date";
    if (strcmp(status->update_phase, "testing") == 0)
        return "Testing an update";
    if (strcmp(status->update_phase, "activation_pending") == 0 ||
        strcmp(status->update_phase, "armed") == 0)
        return "Restart required";
    if (strcmp(status->update_phase, "rollback_pending") == 0)
        return "Recovery pending";
    if (strcmp(status->update_phase, "idle") == 0)
        return "Ready";
    return "Needs attention";
}

int bloom_shell_status_label(const BloomShellStatus *status, size_t row, char *label,
                             size_t label_size)
{
    if (status == NULL || label == NULL || label_size == 0 || row >= 3)
        return -1;
    int length = -1;
    if (row == 0)
        length = snprintf(label, label_size, "System health: %s",
                          status->ready && status->system_healthy ? "Good" : "Needs attention");
    else if (row == 1)
        length = snprintf(label, label_size, "Updates: %s", update_label(status));
    else
        length = snprintf(label, label_size, "RetroAchievements: %s",
                          !status->ready || !status->ra_healthy
                              ? "Needs attention"
                              : (status->ra_enabled ? "On" : "Off"));
    return length >= 0 && (size_t)length < label_size ? 0 : -1;
}
