#include "bloom_shell_settings.h"

#include <cjson/cJSON.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define QUICK_OUTPUT_MAX 2048

#define MIYOO_MINI 283
#define MIYOO_FLIP 285
#define MIYOO_PLUS 354

int bloom_shell_capabilities_from_model(int model, int developer_mode,
                                        BloomShellCapabilities *capabilities)
{
    if (capabilities == NULL || (model != MIYOO_MINI && model != MIYOO_FLIP && model != MIYOO_PLUS))
        return -1;
    capabilities->wifi = model == MIYOO_FLIP || model == MIYOO_PLUS;
    capabilities->flip = model == MIYOO_FLIP;
    capabilities->developer_mode = developer_mode == 1;
    return 0;
}

size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities)
{
    if (capabilities == NULL)
        return 0;
    return 7 + (size_t)capabilities->wifi + (size_t)capabilities->developer_mode;
}

const char *bloom_shell_settings_label(const BloomShellCapabilities *capabilities, size_t row)
{
    if (capabilities == NULL)
        return NULL;
    static const char *before_network[] = {"Display", "Audio", "Controls", "Gameplay"};
    static const char *after_network[] = {"RetroAchievements", "Appearance", "System"};
    if (row < sizeof(before_network) / sizeof(before_network[0]))
        return before_network[row];
    row -= sizeof(before_network) / sizeof(before_network[0]);
    if (capabilities->wifi) {
        if (row == 0)
            return "Network";
        row--;
    }
    if (row < sizeof(after_network) / sizeof(after_network[0]))
        return after_network[row];
    row -= sizeof(after_network) / sizeof(after_network[0]);
    return capabilities->developer_mode && row == 0 ? "Advanced" : NULL;
}

size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities)
{
    return capabilities == NULL ? 0 : 3 + (size_t)capabilities->wifi;
}

const char *bloom_shell_quick_settings_label(const BloomShellCapabilities *capabilities, size_t row)
{
    if (capabilities == NULL)
        return NULL;
    static const char *common[] = {"Brightness", "Volume / Mute"};
    if (row < sizeof(common) / sizeof(common[0]))
        return common[row];
    row -= sizeof(common) / sizeof(common[0]);
    if (capabilities->wifi) {
        if (row == 0)
            return "Wi-Fi";
        row--;
    }
    return row == 0 ? "Battery" : NULL;
}

int bloom_shell_quick_values_parse(const char *json, BloomShellQuickValues *values)
{
    if (json == NULL || values == NULL)
        return -1;
    memset(values, 0, sizeof(*values));
    cJSON *root = cJSON_Parse(json);
    const cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *service = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *generation = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "generation");
    const cJSON *authority = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "authority");
    const cJSON *device = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "device");
    const cJSON *brightness = device == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(device, "brightness");
    const cJSON *volume = device == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(device, "volume");
    const cJSON *mute = device == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(device, "mute");
    const cJSON *wifi = device == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(device, "wifi_enabled");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valuedouble == 1.0 && cJSON_IsString(service) &&
        strcmp(service->valuestring, "bloom-settings") == 0 && cJSON_IsNumber(generation) &&
        generation->valuedouble == (double)generation->valueint && generation->valueint >= 1 &&
        cJSON_IsString(authority) && strcmp(authority->valuestring, "bloom") == 0 &&
        cJSON_IsObject(device) && cJSON_IsNumber(brightness) &&
        brightness->valuedouble == (double)brightness->valueint && brightness->valueint >= 0 &&
        brightness->valueint <= 10 && cJSON_IsNumber(volume) &&
        volume->valuedouble == (double)volume->valueint && volume->valueint >= 0 &&
        volume->valueint <= 20 && cJSON_IsBool(mute) && cJSON_IsBool(wifi)) {
        values->generation = generation->valueint;
        values->brightness = brightness->valueint;
        values->volume = volume->valueint;
        values->mute = cJSON_IsTrue(mute);
        values->wifi_enabled = cJSON_IsTrue(wifi);
        values->ready = 1;
        result = 0;
    }
    cJSON_Delete(root);
    return result;
}

static int wait_child(pid_t child)
{
    int status = 0;
    while (waitpid(child, &status, 0) < 0)
        if (errno != EINTR)
            return -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int load_output(const char *path, const char *first, const char *second, char *json,
                       size_t json_size)
{
    if (path == NULL || path[0] != '/' || first == NULL || json == NULL || json_size < 2)
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
        if (second == NULL)
            execl(path, path, first, (char *)NULL);
        else
            execl(path, path, first, second, (char *)NULL);
        _exit(127);
    }
    close(output[1]);
    size_t length = 0;
    int failed = 0;
    while (!failed && length + 1 < json_size) {
        struct pollfd descriptor = {.fd = output[0], .events = POLLIN | POLLHUP};
        int ready;
        do {
            ready = poll(&descriptor, 1, 5000);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0) {
            failed = 1;
            break;
        }
        ssize_t count = read(output[0], json + length, json_size - length - 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            failed = 1;
            break;
        }
        if (count == 0)
            break;
        length += (size_t)count;
    }
    if (length + 1 == json_size)
        failed = 1;
    close(output[0]);
    if (failed)
        kill(child, SIGKILL);
    int child_result = wait_child(child);
    json[length] = '\0';
    return failed || child_result != 0 ? -1 : 0;
}

int bloom_shell_quick_values_load(const char *settings_path, BloomShellQuickValues *values)
{
    char json[QUICK_OUTPUT_MAX + 1];
    return load_output(settings_path, "values", NULL, json, sizeof(json)) == 0
               ? bloom_shell_quick_values_parse(json, values)
               : -1;
}

int bloom_shell_quick_battery_parse(const char *json, BloomShellQuickValues *values)
{
    if (json == NULL || values == NULL)
        return -1;
    cJSON *root = cJSON_Parse(json);
    const cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *service = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *battery = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "battery");
    const cJSON *available = battery == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(battery, "available");
    const cJSON *capacity = battery == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(battery, "capacity");
    const cJSON *charging = battery == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(battery, "charging");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valuedouble == 1.0 && cJSON_IsString(service) &&
        strcmp(service->valuestring, "bloom-platform") == 0 && cJSON_IsObject(battery) &&
        cJSON_IsBool(available) && cJSON_IsBool(charging) &&
        (cJSON_IsNull(capacity) ||
         (cJSON_IsNumber(capacity) && capacity->valuedouble == (double)capacity->valueint &&
          capacity->valueint >= 0 && capacity->valueint <= 100))) {
        values->battery_available = cJSON_IsTrue(available);
        values->battery_capacity_available = cJSON_IsNumber(capacity);
        values->battery_capacity = values->battery_capacity_available ? capacity->valueint : 0;
        values->battery_charging = cJSON_IsTrue(charging);
        result = values->battery_available ? 0 : -1;
    }
    cJSON_Delete(root);
    return result;
}

int bloom_shell_quick_battery_load(const char *platform_path, BloomShellQuickValues *values)
{
    char json[QUICK_OUTPUT_MAX + 1];
    return load_output(platform_path, "battery", "--json", json, sizeof(json)) == 0
               ? bloom_shell_quick_battery_parse(json, values)
               : -1;
}

int bloom_shell_quick_settings_format(const BloomShellCapabilities *capabilities,
                                      const BloomShellQuickValues *values, size_t row,
                                      char *label, size_t label_size)
{
    const char *name = bloom_shell_quick_settings_label(capabilities, row);
    if (name == NULL || values == NULL || label == NULL || label_size == 0)
        return -1;
    int length;
    size_t battery_row = capabilities->wifi ? 3 : 2;
    if (row == battery_row && !values->battery_available)
        length = snprintf(label, label_size, "Battery: unavailable");
    else if (row == battery_row && values->battery_charging &&
             !values->battery_capacity_available)
        length = snprintf(label, label_size, "Battery: Charging");
    else if (row == battery_row)
        length = snprintf(label, label_size, "Battery: %d%%%s", values->battery_capacity,
                          values->battery_charging ? " / Charging" : "");
    else if (!values->ready)
        length = snprintf(label, label_size, "%s: unavailable", name);
    else if (row == 0)
        length = snprintf(label, label_size, "Brightness: %d", values->brightness);
    else if (row == 1)
        length = snprintf(label, label_size, "Volume: %s%d", values->mute ? "Muted / " : "", values->volume);
    else if (capabilities->wifi && row == 2)
        length = snprintf(label, label_size, "Wi-Fi: %s", values->wifi_enabled ? "On" : "Off");
    else
        return -1;
    return length >= 0 && (size_t)length < label_size ? 0 : -1;
}

static int run_request(const char *path, const char *first, const char *second, const char *third)
{
    if (path == NULL || path[0] != '/')
        return -1;
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        FILE *null = fopen("/dev/null", "w");
        if (null != NULL) {
            dup2(fileno(null), STDOUT_FILENO);
            dup2(fileno(null), STDERR_FILENO);
        }
        if (third != NULL)
            execl(path, path, first, second, third, (char *)NULL);
        else
            execl(path, path, first, second, (char *)NULL);
        _exit(127);
    }
    return wait_child(child);
}

int bloom_shell_quick_settings_adjust(const BloomShellCapabilities *capabilities,
                                      BloomShellQuickValues *values, size_t row, int direction,
                                      const char *controls_path, const char *network_path)
{
    if (capabilities == NULL || values == NULL || !values->ready ||
        (direction != -1 && direction != 1))
        return -1;
    int *current = NULL;
    int maximum = 0;
    const char *control = NULL;
    if (row == 0) {
        current = &values->brightness;
        maximum = 10;
        control = "brightness";
    }
    else if (row == 1) {
        current = &values->volume;
        maximum = 20;
        control = "volume";
    }
    if (current != NULL) {
        int next = *current + direction;
        if (next < 0)
            next = 0;
        if (next > maximum)
            next = maximum;
        if (next == *current)
            return 0;
        char value[4];
        snprintf(value, sizeof(value), "%d", next);
        if (run_request(controls_path, "request", control, value) != 0)
            return -1;
        *current = next;
        if (row == 1)
            values->mute = 0;
        return 0;
    }
    if (capabilities->wifi && row == 2) {
        int next = direction > 0;
        if (next == values->wifi_enabled)
            return 0;
        if (run_request(network_path, "request", next ? "enable" : "disable", NULL) != 0)
            return -1;
        values->wifi_enabled = next;
        return 0;
    }
    return -1;
}
