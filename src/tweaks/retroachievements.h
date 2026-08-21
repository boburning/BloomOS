#ifndef TWEAKS_RETROACHIEVEMENTS_H__
#define TWEAKS_RETROACHIEVEMENTS_H__

#include <cjson/cJSON.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "components/list.h"

#include "./appstate.h"
#include "./text_entry.h"
#include "./tools.h"

#define BLOOMCTL_RA "/mnt/SDCARD/.tmp_update/bin/bloomctl achievements"

typedef struct ra_settings_state_s {
    bool available;
    bool configured;
    bool enabled;
    bool authenticated;
    bool offline_casual;
    int mode;
    int pending_awards;
} RaSettingsState;

static RaSettingsState ra_settings;

static cJSON *ra_read_json_result(const char *command, bool require_success)
{
    char output[1024] = {0};
    FILE *pipe = popen(command, "r");
    if (pipe == NULL)
        return NULL;
    bool read_ok = fgets(output, sizeof(output), pipe) != NULL;
    int status = pclose(pipe);
    if (!read_ok || !WIFEXITED(status) || (require_success && WEXITSTATUS(status) != 0))
        return NULL;
    return cJSON_Parse(output);
}

static cJSON *ra_read_json(const char *command)
{
    return ra_read_json_result(command, true);
}

static bool ra_json_bool(cJSON *root, const char *name, bool *value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsBool(item))
        return false;
    *value = cJSON_IsTrue(item);
    return true;
}

static void ra_load_state(void)
{
    memset(&ra_settings, 0, sizeof(ra_settings));
    ra_settings.pending_awards = -1;
    cJSON *account = ra_read_json(BLOOMCTL_RA " account status 2>/dev/null");
    if (account != NULL) {
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(account, "mode");
        if (ra_json_bool(account, "configured", &ra_settings.configured) &&
            ra_json_bool(account, "enabled", &ra_settings.enabled) &&
            ra_json_bool(account, "authenticated", &ra_settings.authenticated) &&
            ra_json_bool(account, "offline_casual", &ra_settings.offline_casual) &&
            cJSON_IsString(mode) &&
            (strcmp(mode->valuestring, "softcore") == 0 || strcmp(mode->valuestring, "hardcore") == 0)) {
            ra_settings.available = true;
            ra_settings.mode = strcmp(mode->valuestring, "hardcore") == 0;
        }
        cJSON_Delete(account);
    }

    cJSON *pending = ra_read_json(BLOOMCTL_RA " proxy pending 2>/dev/null");
    if (pending != NULL) {
        cJSON *count = cJSON_GetObjectItemCaseSensitive(pending, "pending_awards");
        if (cJSON_IsNumber(count) && count->valueint >= 0)
            ra_settings.pending_awards = count->valueint;
        cJSON_Delete(pending);
    }
}

static void ra_run_setting(const char *command)
{
    int status = system(command);
    _toolDialog("RetroAchievements",
                status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0
                    ? "Setting saved"
                    : "Setting failed",
                false);
    if (video != NULL)
        msleep(900);
    reset_menus = true;
    all_changed = true;
}

static void action_ra_enabled(void *pt)
{
    if (((ListItem *)pt)->value)
        ra_run_setting(BLOOMCTL_RA " account set enabled true >/dev/null 2>&1");
    else
        ra_run_setting(BLOOMCTL_RA " account set enabled false >/dev/null 2>&1");
}

static void action_ra_mode(void *pt)
{
    if (((ListItem *)pt)->value)
        ra_run_setting(BLOOMCTL_RA " account set mode hardcore >/dev/null 2>&1");
    else
        ra_run_setting(BLOOMCTL_RA " account set mode softcore >/dev/null 2>&1");
}

static void action_ra_offline(void *pt)
{
    if (((ListItem *)pt)->value)
        ra_run_setting(BLOOMCTL_RA " account set offline-casual automatic >/dev/null 2>&1");
    else
        ra_run_setting(BLOOMCTL_RA " account set offline-casual disabled >/dev/null 2>&1");
}

static bool ra_write_all(int descriptor, const char *value, size_t length)
{
    size_t written = 0;
    while (written < length) {
        ssize_t result = write(descriptor, value + written, length - written);
        if (result <= 0)
            return false;
        written += (size_t)result;
    }
    return true;
}

static bool ra_submit_login(const char *username, const char *password)
{
    int input_pipe[2];
    if (pipe(input_pipe) != 0)
        return false;
    pid_t child = fork();
    if (child == 0) {
        int null_output = open("/dev/null", O_WRONLY);
        dup2(input_pipe[0], STDIN_FILENO);
        if (null_output >= 0) {
            dup2(null_output, STDOUT_FILENO);
            dup2(null_output, STDERR_FILENO);
        }
        close(input_pipe[0]);
        close(input_pipe[1]);
        execl("/mnt/SDCARD/.tmp_update/bin/bloomctl", "bloomctl", "achievements", "account", "login", (char *)NULL);
        _exit(127);
    }
    close(input_pipe[0]);
    if (child < 0) {
        close(input_pipe[1]);
        return false;
    }
    bool sent = ra_write_all(input_pipe[1], username, strlen(username)) &&
                ra_write_all(input_pipe[1], "\n", 1) &&
                ra_write_all(input_pipe[1], password, strlen(password)) &&
                ra_write_all(input_pipe[1], "\n", 1);
    close(input_pipe[1]);
    int status = 0;
    return sent && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void action_ra_login(void *_)
{
    (void)_;
    char username[64] = {0};
    char password[128] = {0};
    if (!text_entry_dialog("RA username", username, sizeof(username), false) ||
        !text_entry_dialog("RA password", password, sizeof(password), true)) {
        memset(password, 0, sizeof(password));
        return;
    }
    _toolDialog("RetroAchievements", "Signing in securely...", false);
    bool authenticated = ra_submit_login(username, password);
    memset(password, 0, sizeof(password));
    memset(username, 0, sizeof(username));
    _toolDialog("RetroAchievements", authenticated ? "Signed in" : "Sign-in failed", false);
    if (video != NULL)
        msleep(authenticated ? 1000 : 1800);
    reset_menus = true;
    all_changed = true;
}

static const char *ra_network_message(const char *state)
{
    if (strcmp(state, "ready") == 0)
        return "RetroAchievements is reachable";
    if (strcmp(state, "no_network_hardware") == 0)
        return "This device has no network hardware";
    if (strcmp(state, "wifi_disabled") == 0)
        return "Wi-Fi is disabled";
    if (strcmp(state, "not_associated") == 0)
        return "Wi-Fi is not connected";
    if (strcmp(state, "clock_invalid") == 0)
        return "System time must be synchronized";
    if (strcmp(state, "dns_failure") == 0)
        return "DNS lookup failed";
    if (strcmp(state, "tls_failure") == 0)
        return "Secure connection failed";
    if (strcmp(state, "timeout") == 0)
        return "Connection timed out";
    if (strcmp(state, "network_unreachable") == 0)
        return "Network is unreachable";
    return "RetroAchievements is unavailable";
}

static void action_ra_connection(void *_)
{
    (void)_;
    const char *message = "Connection check failed";
    cJSON *result = ra_read_json_result(BLOOMCTL_RA " network status 2>/dev/null", false);
    if (result != NULL) {
        cJSON *state = cJSON_GetObjectItemCaseSensitive(result, "state");
        if (cJSON_IsString(state))
            message = ra_network_message(state->valuestring);
    }
    _toolDialog("RetroAchievements", message, false);
    if (result != NULL)
        cJSON_Delete(result);
    if (video != NULL)
        msleep(1400);
    all_changed = true;
}

static void action_ra_scan(void *_)
{
    (void)_;
    _runCommandPopup("RetroAchievements", BLOOMCTL_RA " scan --changed >/dev/null 2>&1");
}

static void action_ra_cache_favorites(void *_)
{
    (void)_;
    _runCommandPopup("RetroAchievements", BLOOMCTL_RA " proxy cache-favorites >/dev/null 2>&1");
}

static void action_ra_cache_recent(void *_)
{
    (void)_;
    _runCommandPopup("RetroAchievements", BLOOMCTL_RA " proxy cache-recent >/dev/null 2>&1");
}

static void action_ra_cache_all(void *_)
{
    (void)_;
    _runCommandPopup("RetroAchievements", BLOOMCTL_RA " proxy cache-all >/dev/null 2>&1");
}

static void action_ra_sign_out(void *_)
{
    (void)_;
    _runCommandPopup("RetroAchievements", BLOOMCTL_RA " account sign-out >/dev/null 2>&1");
    reset_menus = true;
}

static void menu_ra_offline_cache(void *_)
{
    (void)_;
    if (!_menu_ra_offline_cache._created) {
        _menu_ra_offline_cache = list_createWithTitle(3, LIST_SMALL, "RA offline cache");
        list_addItem(&_menu_ra_offline_cache, (ListItem){.label = "Cache Favorites", .action = action_ra_cache_favorites});
        list_addItem(&_menu_ra_offline_cache, (ListItem){.label = "Cache Recent Games", .action = action_ra_cache_recent});
        list_addItem(&_menu_ra_offline_cache, (ListItem){.label = "Cache all RA systems", .action = action_ra_cache_all});
    }
    menu_stack[++menu_level] = &_menu_ra_offline_cache;
    header_changed = true;
}

static void menu_retroachievements(void *_)
{
    (void)_;
    if (!_menu_retroachievements._created) {
        ra_load_state();
        _menu_retroachievements = list_createWithTitle(10, LIST_SMALL, "RetroAchievements");
        char account_label[STR_MAX];
        if (!ra_settings.available)
            strcpy(account_label, "Account: Service unavailable");
        else if (ra_settings.authenticated)
            strcpy(account_label, "Account: Signed in");
        else if (ra_settings.configured)
            strcpy(account_label, "Account: Attention required");
        else
            strcpy(account_label, "Account: Not signed in");
        list_addItem(&_menu_retroachievements,
                     (ListItem){.label = "", .disabled = true});
        strncpy(_menu_retroachievements.items[0].label, account_label, STR_MAX - 1);
        list_addItemWithInfoNote(&_menu_retroachievements,
                                 (ListItem){.label = "", .action = action_ra_login},
                                 "Enter your RetroAchievements username and\npassword on-device. Bloom stores only the\nreturned token, never the password.");
        strncpy(_menu_retroachievements.items[1].label,
                ra_settings.configured ? "Sign in again" : "Sign in", STR_MAX - 1);
        list_addItemWithInfoNote(&_menu_retroachievements,
                                 (ListItem){.label = "Enable achievements", .item_type = TOGGLE, .disabled = !ra_settings.configured, .value = ra_settings.enabled, .action = action_ra_enabled},
                                 "Enable RetroAchievements for supported\ngames and certified or best-effort cores.");
        list_addItemWithInfoNote(&_menu_retroachievements,
                                 (ListItem){.label = "Mode", .item_type = MULTIVALUE, .value_max = 1, .value_labels = {"Softcore", "Hardcore"}, .disabled = !ra_settings.configured, .value = ra_settings.mode, .action = action_ra_mode},
                                 "Hardcore disables states, rewind, cheats,\nand other prohibited features. It is\nnever silently downgraded to Softcore.");
        list_addItemWithInfoNote(&_menu_retroachievements,
                                 (ListItem){.label = "Offline Casual", .item_type = TOGGLE, .disabled = !ra_settings.configured, .value = ra_settings.offline_casual, .action = action_ra_offline},
                                 "Route Softcore sessions through the optional\noffline proxy. Hardcore always stays direct.");
        list_addItem(&_menu_retroachievements,
                     (ListItem){.label = "Offline cache...", .action = menu_ra_offline_cache});
        list_addItem(&_menu_retroachievements,
                     (ListItem){.label = "Scan changed games", .action = action_ra_scan});
        list_addItem(&_menu_retroachievements,
                     (ListItem){.label = "Check connection", .action = action_ra_connection});
        char pending_label[STR_MAX];
        if (ra_settings.pending_awards >= 0)
            snprintf(pending_label, sizeof(pending_label), "Offline awards pending: %d", ra_settings.pending_awards);
        else
            strcpy(pending_label, "Offline awards: Unavailable");
        list_addItem(&_menu_retroachievements, (ListItem){.label = "", .disabled = true});
        strncpy(_menu_retroachievements.items[8].label, pending_label, STR_MAX - 1);
        list_addItem(&_menu_retroachievements,
                     (ListItem){.label = "Sign out", .disabled = !ra_settings.configured, .action = action_ra_sign_out});
    }
    menu_stack[++menu_level] = &_menu_retroachievements;
    header_changed = true;
}

#endif
