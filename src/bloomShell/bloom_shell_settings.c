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

typedef struct {
    BloomShellSettingsRow row;
    int requires_wifi;
    int requires_developer;
} BloomShellSettingsDefinition;

static const BloomShellSettingsDefinition settings_rows[] = {
    {{BLOOM_SHELL_SETTINGS_DISPLAY_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "DISPLAY"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_BRIGHTNESS, BLOOM_SHELL_SETTINGS_ROW_SLIDER, "Brightness"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_THEME, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Theme"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_AUDIO_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "AUDIO"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_VOLUME, BLOOM_SHELL_SETTINGS_ROW_SLIDER, "Volume"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_MUTE, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "Mute"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_CONTROLS_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION,
      "CONTROLS & GAMEPLAY"},
     0,
     0},
    {{BLOOM_SHELL_SETTINGS_BUTTON_GRAMMAR, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Buttons"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_LAUNCH_BEHAVIOR, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Game launch"},
     0,
     0},
    {{BLOOM_SHELL_SETTINGS_NETWORK_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "NETWORK"}, 1, 0},
    {{BLOOM_SHELL_SETTINGS_WIFI, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "Wi-Fi"}, 1, 0},
    {{BLOOM_SHELL_SETTINGS_SSH, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "SSH"}, 1, 0},
    {{BLOOM_SHELL_SETTINGS_SFTP, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "SFTP"}, 1, 0},
    {{BLOOM_SHELL_SETTINGS_SAMBA, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "Samba"}, 1, 0},
    {{BLOOM_SHELL_SETTINGS_RA_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "RETROACHIEVEMENTS"},
     0,
     0},
    {{BLOOM_SHELL_SETTINGS_RA_ENABLED, BLOOM_SHELL_SETTINGS_ROW_TOGGLE, "Achievements"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_RA_ACCOUNT, BLOOM_SHELL_SETTINGS_ROW_DETAIL, "Account"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_RA_MODE, BLOOM_SHELL_SETTINGS_ROW_ENUM, "Mode"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_RA_OFFLINE, BLOOM_SHELL_SETTINGS_ROW_ENUM, "Offline awards"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_RA_CONNECTION, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Connection"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_SYSTEM_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "SYSTEM"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_UPDATE, BLOOM_SHELL_SETTINGS_ROW_ACTION, "Update"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_STORAGE, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Storage"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_HEALTH, BLOOM_SHELL_SETTINGS_ROW_ACTION, "Health"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_ABOUT, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "About"}, 0, 0},
    {{BLOOM_SHELL_SETTINGS_DEVELOPER_SECTION, BLOOM_SHELL_SETTINGS_ROW_SECTION, "DEVELOPER"}, 0, 1},
    {{BLOOM_SHELL_SETTINGS_DEVELOPER_MODE, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Developer Mode"},
     0,
     1},
    {{BLOOM_SHELL_SETTINGS_DIAGNOSTICS, BLOOM_SHELL_SETTINGS_ROW_READ_ONLY, "Diagnostics"}, 0, 1},
};

static int settings_definition_visible(const BloomShellCapabilities *capabilities,
                                       const BloomShellSettingsDefinition *definition)
{
    return capabilities != NULL && definition != NULL &&
           (!definition->requires_wifi || capabilities->wifi) &&
           (!definition->requires_developer || capabilities->developer_mode);
}

size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities)
{
    size_t count = 0;
    for (size_t index = 0; capabilities != NULL && index < sizeof(settings_rows) / sizeof(settings_rows[0]);
         ++index)
        if (settings_definition_visible(capabilities, &settings_rows[index]))
            count++;
    return count;
}

int bloom_shell_settings_row(const BloomShellCapabilities *capabilities, size_t row,
                             BloomShellSettingsRow *settings_row)
{
    if (capabilities == NULL || settings_row == NULL)
        return -1;
    size_t visible = 0;
    for (size_t index = 0; index < sizeof(settings_rows) / sizeof(settings_rows[0]); ++index) {
        if (!settings_definition_visible(capabilities, &settings_rows[index]))
            continue;
        if (visible++ == row) {
            *settings_row = settings_rows[index].row;
            return 0;
        }
    }
    return -1;
}

int bloom_shell_settings_row_selectable(const BloomShellCapabilities *capabilities, size_t row)
{
    BloomShellSettingsRow settings_row;
    return bloom_shell_settings_row(capabilities, row, &settings_row) == 0 &&
           settings_row.kind != BLOOM_SHELL_SETTINGS_ROW_SECTION;
}

size_t bloom_shell_settings_first_selectable(const BloomShellCapabilities *capabilities)
{
    size_t count = bloom_shell_settings_count(capabilities);
    for (size_t row = 0; row < count; ++row)
        if (bloom_shell_settings_row_selectable(capabilities, row))
            return row;
    return 0;
}

size_t bloom_shell_settings_next_selectable(const BloomShellCapabilities *capabilities, size_t row,
                                            int direction)
{
    size_t count = bloom_shell_settings_count(capabilities);
    if (count == 0 || row >= count || (direction != -1 && direction != 1))
        return row;
    size_t next = row;
    while ((direction < 0 && next > 0) || (direction > 0 && next + 1 < count)) {
        next = direction < 0 ? next - 1 : next + 1;
        if (bloom_shell_settings_row_selectable(capabilities, next))
            return next;
    }
    return row;
}

int bloom_shell_settings_row_format(const BloomShellCapabilities *capabilities,
                                    const BloomShellQuickValues *values, size_t row, char *label,
                                    size_t label_size)
{
    BloomShellSettingsRow settings_row;
    if (values == NULL || label == NULL || label_size == 0 ||
        bloom_shell_settings_row(capabilities, row, &settings_row) != 0)
        return -1;
    int length = -1;
    switch (settings_row.id) {
    case BLOOM_SHELL_SETTINGS_BRIGHTNESS:
        length = values->ready ? snprintf(label, label_size, "Brightness                 %d", values->brightness)
                               : snprintf(label, label_size, "Brightness       unavailable");
        break;
    case BLOOM_SHELL_SETTINGS_VOLUME:
        length = values->ready ? snprintf(label, label_size, "Volume                    %d", values->volume)
                               : snprintf(label, label_size, "Volume           unavailable");
        break;
    case BLOOM_SHELL_SETTINGS_MUTE:
        length = values->ready ? snprintf(label, label_size, "Mute                    %s", values->mute ? "On" : "Off")
                               : snprintf(label, label_size, "Mute             unavailable");
        break;
    case BLOOM_SHELL_SETTINGS_WIFI:
        length = values->ready ? snprintf(label, label_size, "Wi-Fi                  %s", values->wifi_enabled ? "On" : "Off")
                               : snprintf(label, label_size, "Wi-Fi            unavailable");
        break;
    case BLOOM_SHELL_SETTINGS_SSH:
        length = !values->network_services_ready || !values->ssh_available
                     ? snprintf(label, label_size, "SSH              needs public key")
                     : snprintf(label, label_size, "SSH                       %s",
                                values->ssh_enabled ? "On" : "Off");
        break;
    case BLOOM_SHELL_SETTINGS_SFTP:
        length = !values->network_services_ready || !values->sftp_available
                     ? snprintf(label, label_size, "SFTP               unavailable")
                 : !values->ssh_enabled
                     ? snprintf(label, label_size, "SFTP              requires SSH")
                     : snprintf(label, label_size, "SFTP                      %s",
                                values->sftp_enabled ? "On" : "Off");
        break;
    case BLOOM_SHELL_SETTINGS_SAMBA:
        length = !values->network_services_ready || !values->samba_available
                     ? snprintf(label, label_size, "Samba              unavailable")
                     : snprintf(label, label_size, "Samba                     %s",
                                values->samba_enabled ? "On" : "Off");
        break;
    case BLOOM_SHELL_SETTINGS_THEME:
        length = snprintf(label, label_size, "Theme                 Bloom");
        break;
    case BLOOM_SHELL_SETTINGS_BUTTON_GRAMMAR:
        length = snprintf(label, label_size, "Buttons       A Confirm / B Back");
        break;
    case BLOOM_SHELL_SETTINGS_LAUNCH_BEHAVIOR:
        length = snprintf(label, label_size, "Game launch       Supervised");
        break;
    case BLOOM_SHELL_SETTINGS_DEVELOPER_MODE:
        length = snprintf(label, label_size, "Developer Mode           On");
        break;
    default:
        length = snprintf(label, label_size, "%s%s", settings_row.label,
                          settings_row.kind == BLOOM_SHELL_SETTINGS_ROW_DETAIL ? "                 >" : "");
        break;
    }
    return length >= 0 && (size_t)length < label_size ? 0 : -1;
}

size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities)
{
    return capabilities == NULL ? 0 : 4 + (size_t)capabilities->wifi;
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
    if (row == 0)
        return "Battery";
    return row == 1 ? "Open Settings" : NULL;
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

int bloom_shell_network_services_parse(const char *json, BloomShellQuickValues *values)
{
    if (json == NULL || values == NULL)
        return -1;
    cJSON *root = cJSON_Parse(json);
    const cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *service = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *ssh = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "ssh");
    const cJSON *sftp = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "sftp");
    const cJSON *samba = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "samba");
    const cJSON *ssh_available = cJSON_GetObjectItemCaseSensitive(ssh, "available");
    const cJSON *ssh_enabled = cJSON_GetObjectItemCaseSensitive(ssh, "enabled");
    const cJSON *sftp_available = cJSON_GetObjectItemCaseSensitive(sftp, "available");
    const cJSON *sftp_enabled = cJSON_GetObjectItemCaseSensitive(sftp, "enabled");
    const cJSON *samba_available = cJSON_GetObjectItemCaseSensitive(samba, "available");
    const cJSON *samba_enabled = cJSON_GetObjectItemCaseSensitive(samba, "enabled");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valuedouble == 1.0 && cJSON_IsString(service) &&
        strcmp(service->valuestring, "bloom-network-services") == 0 && cJSON_IsObject(ssh) &&
        cJSON_IsObject(sftp) && cJSON_IsObject(samba) && cJSON_IsBool(ssh_available) &&
        cJSON_IsBool(ssh_enabled) && cJSON_IsBool(sftp_available) && cJSON_IsBool(sftp_enabled) &&
        cJSON_IsBool(samba_available) && cJSON_IsBool(samba_enabled)) {
        values->ssh_available = cJSON_IsTrue(ssh_available);
        values->ssh_enabled = cJSON_IsTrue(ssh_enabled);
        values->sftp_available = cJSON_IsTrue(sftp_available);
        values->sftp_enabled = cJSON_IsTrue(sftp_enabled);
        values->samba_available = cJSON_IsTrue(samba_available);
        values->samba_enabled = cJSON_IsTrue(samba_enabled);
        values->network_services_ready = 1;
        result = 0;
    }
    cJSON_Delete(root);
    return result;
}

int bloom_shell_network_services_load(const char *services_path, BloomShellQuickValues *values)
{
    char json[QUICK_OUTPUT_MAX + 1];
    return load_output(services_path, "status", NULL, json, sizeof(json)) == 0
               ? bloom_shell_network_services_parse(json, values)
               : -1;
}

int bloom_shell_first_run_parse(const char *json, BloomShellFirstRun *first_run)
{
    if (json == NULL || first_run == NULL)
        return -1;
    memset(first_run, 0, sizeof(*first_run));
    cJSON *root = cJSON_Parse(json);
    const cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *service =
        root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *complete =
        root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "first_run_complete");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valuedouble == 1.0 && cJSON_IsString(service) &&
        strcmp(service->valuestring, "bloom-settings") == 0 && cJSON_IsBool(complete)) {
        first_run->ready = 1;
        first_run->complete = cJSON_IsTrue(complete);
        result = 0;
    }
    cJSON_Delete(root);
    return result;
}

int bloom_shell_first_run_load(const char *settings_path, BloomShellFirstRun *first_run)
{
    char json[QUICK_OUTPUT_MAX + 1];
    return load_output(settings_path, "first-run-status", NULL, json, sizeof(json)) == 0
               ? bloom_shell_first_run_parse(json, first_run)
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

int bloom_shell_ra_values_parse(const char *json, BloomShellRaValues *values)
{
    if (json == NULL || values == NULL)
        return -1;
    memset(values, 0, sizeof(*values));
    cJSON *root = cJSON_Parse(json);
    const cJSON *schema = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *configured =
        root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "configured");
    const cJSON *enabled = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "enabled");
    const cJSON *authenticated =
        root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "authenticated");
    const cJSON *mode = root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "mode");
    const cJSON *offline =
        root == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(root, "offline_casual");
    int result = -1;
    if (cJSON_IsNumber(schema) && schema->valuedouble == 1.0 && cJSON_IsBool(configured) &&
        cJSON_IsBool(enabled) && cJSON_IsBool(authenticated) && cJSON_IsString(mode) &&
        (strcmp(mode->valuestring, "softcore") == 0 || strcmp(mode->valuestring, "hardcore") == 0) &&
        cJSON_IsBool(offline)) {
        values->ready = 1;
        values->configured = cJSON_IsTrue(configured);
        values->enabled = cJSON_IsTrue(enabled);
        values->authenticated = cJSON_IsTrue(authenticated);
        values->hardcore = strcmp(mode->valuestring, "hardcore") == 0;
        values->offline_casual = cJSON_IsTrue(offline);
        result = 0;
    }
    cJSON_Delete(root);
    return result;
}

int bloom_shell_ra_values_load(const char *ra_path, BloomShellRaValues *values)
{
    char json[QUICK_OUTPUT_MAX + 1];
    return load_output(ra_path, "account", "status", json, sizeof(json)) == 0
               ? bloom_shell_ra_values_parse(json, values)
               : -1;
}

int bloom_shell_ra_settings_format(const BloomShellRaValues *values,
                                   BloomShellSettingsRowId id, char *label,
                                   size_t label_size)
{
    if (values == NULL || label == NULL || label_size == 0)
        return -1;
    int length = -1;
    if (!values->ready) {
        const char *name = id == BLOOM_SHELL_SETTINGS_RA_ENABLED   ? "Achievements"
                           : id == BLOOM_SHELL_SETTINGS_RA_MODE    ? "Mode"
                           : id == BLOOM_SHELL_SETTINGS_RA_OFFLINE ? "Offline awards"
                                                                   : NULL;
        if (name != NULL)
            length = snprintf(label, label_size, "%s       unavailable", name);
    }
    else if (id == BLOOM_SHELL_SETTINGS_RA_ENABLED)
        length = snprintf(label, label_size, "Achievements              %s",
                          values->enabled ? "On" : "Off");
    else if (id == BLOOM_SHELL_SETTINGS_RA_MODE)
        length = snprintf(label, label_size, "Mode                 %s",
                          values->hardcore ? "Hardcore" : "Softcore");
    else if (id == BLOOM_SHELL_SETTINGS_RA_OFFLINE)
        length = snprintf(label, label_size, "Offline awards       %s",
                          values->offline_casual ? "Automatic" : "Off");
    return length >= 0 && (size_t)length < label_size ? 0 : -1;
}

int bloom_shell_ra_settings_change(BloomShellRaValues *values,
                                   BloomShellSettingsRowId id, int direction,
                                   const char *ra_path)
{
    if (values == NULL || !values->ready || !values->configured ||
        (direction != -1 && direction != 1) || ra_path == NULL || ra_path[0] != '/')
        return -1;
    const char *field = NULL;
    const char *value = NULL;
    int *current = NULL;
    if (id == BLOOM_SHELL_SETTINGS_RA_ENABLED) {
        field = "enabled";
        value = direction > 0 ? "true" : "false";
        current = &values->enabled;
    }
    else if (id == BLOOM_SHELL_SETTINGS_RA_MODE) {
        field = "mode";
        value = direction > 0 ? "hardcore" : "softcore";
        current = &values->hardcore;
    }
    else if (id == BLOOM_SHELL_SETTINGS_RA_OFFLINE) {
        field = "offline-casual";
        value = direction > 0 ? "automatic" : "disabled";
        current = &values->offline_casual;
    }
    if (field == NULL || *current == (direction > 0))
        return field == NULL ? -1 : 0;
    pid_t child = fork();
    if (child < 0)
        return -1;
    if (child == 0) {
        execl(ra_path, ra_path, "account", "set", field, value, (char *)NULL);
        _exit(127);
    }
    if (wait_child(child) != 0)
        return -1;
    *current = direction > 0;
    return 0;
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
    size_t open_settings_row = battery_row + 1;
    if (row == open_settings_row)
        length = snprintf(label, label_size, "Open Settings                 >");
    else if (row == battery_row && !values->battery_available)
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
        else if (second != NULL)
            execl(path, path, first, second, (char *)NULL);
        else
            execl(path, path, first, (char *)NULL);
        _exit(127);
    }
    return wait_child(child);
}

int bloom_shell_network_service_change(BloomShellQuickValues *values,
                                       BloomShellSettingsRowId id, int enabled,
                                       const char *services_path)
{
    if (values == NULL || !values->network_services_ready ||
        (enabled != 0 && enabled != 1))
        return -1;
    const char *service = NULL;
    int *current = NULL;
    int available = 0;
    if (id == BLOOM_SHELL_SETTINGS_SSH) {
        service = "ssh";
        current = &values->ssh_enabled;
        available = values->ssh_available;
    }
    else if (id == BLOOM_SHELL_SETTINGS_SFTP) {
        service = "sftp";
        current = &values->sftp_enabled;
        available = values->sftp_available && values->ssh_enabled;
    }
    else if (id == BLOOM_SHELL_SETTINGS_SAMBA) {
        service = "samba";
        current = &values->samba_enabled;
        available = values->samba_available;
    }
    if (service == NULL || current == NULL || !available)
        return -1;
    if (*current == enabled)
        return 0;
    if (run_request(services_path, "request", service, enabled ? "enable" : "disable") != 0)
        return -1;
    *current = enabled;
    if (id == BLOOM_SHELL_SETTINGS_SSH && !enabled) {
        values->sftp_enabled = 0;
    }
    return 0;
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

int bloom_shell_mute_toggle(BloomShellQuickValues *values, const char *controls_path)
{
    if (values == NULL || !values->ready)
        return -1;
    if (values->mute) {
        char volume[4];
        snprintf(volume, sizeof(volume), "%d", values->volume);
        if (run_request(controls_path, "request", "volume", volume) != 0)
            return -1;
        values->mute = 0;
        return 0;
    }
    if (run_request(controls_path, "request", "mute", "true") != 0)
        return -1;
    values->mute = 1;
    return 0;
}

int bloom_shell_quick_settings_activate(const BloomShellCapabilities *capabilities,
                                        BloomShellQuickValues *values, size_t row,
                                        const char *controls_path, const char *network_path,
                                        int *open_settings)
{
    if (capabilities == NULL || values == NULL || open_settings == NULL)
        return -1;
    *open_settings = 0;
    if (row == 1)
        return bloom_shell_mute_toggle(values, controls_path);
    if (capabilities->wifi && row == 2)
        return bloom_shell_quick_settings_adjust(capabilities, values, row,
                                                 values->wifi_enabled ? -1 : 1, controls_path,
                                                 network_path);
    size_t battery_row = capabilities->wifi ? 3 : 2;
    if (row == battery_row + 1) {
        *open_settings = 1;
        return 0;
    }
    return row == 0 || row == battery_row ? 0 : -1;
}

int bloom_shell_first_run_finish(const char *settings_path, BloomShellFirstRun *first_run,
                                 BloomShellQuickValues *values)
{
    if (first_run == NULL || values == NULL || !first_run->ready || first_run->complete)
        return -1;
    if (run_request(settings_path, "activate-bloom", NULL, NULL) != 0 ||
        run_request(settings_path, "complete-first-run", NULL, NULL) != 0 ||
        bloom_shell_quick_values_load(settings_path, values) != 0)
        return -1;
    first_run->complete = 1;
    return 0;
}
