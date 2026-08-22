#include "bloom_shell_achievements.h"
#include "bloom_shell_games.h"
#include "bloom_shell_launch.h"
#include "bloom_shell_ra_form.h"
#include "bloom_shell_root.h"
#include "bloom_shell_safe_mode.h"
#include "bloom_shell_search.h"
#include "bloom_shell_settings.h"
#include "bloom_shell_status.h"

#include "../bloomLibrary/bloom_library_mutation.h"
#include "../bloomUi/bloom_ui_core.h"
#include "../bloomUi/bloom_ui_input.h"
#include "../bloomUi/bloom_ui_renderer.h"
#include "../gameSwitcher/gameSwitcherLibrary.h"

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DATABASE_PATH "/mnt/SDCARD/.bloom/library/catalog.sqlite3"
#define RA_DATABASE_PATH "/mnt/SDCARD/.bloom/achievements/catalog.sqlite3"
#define REQUEST_PATH "/mnt/SDCARD/.tmp_update/bloom-shell-launch.json"
#define COMMAND_PATH "/mnt/SDCARD/.tmp_update/cmd_to_run.sh"
#define SESSION_REQUEST_PATH "/tmp/bloom-session/request.json"
#define SESSION_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-session"
#define BLOOM_STATUS_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-shell-status"
#define BLOOMCTL_BINARY "/mnt/SDCARD/.tmp_update/bin/bloomctl"
#define BLOOM_SETTINGS_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-settings"
#define BLOOM_CONTROLS_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-controls"
#define BLOOM_NETWORK_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-network"
#define BLOOM_PLATFORM_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-platform"
#define BLOOM_RA_BINARY "/mnt/SDCARD/.tmp_update/bin/bloom-ra"
#define GAME_SWITCHER_BINARY "/mnt/SDCARD/.tmp_update/bin/gameSwitcher"
#define DEVICE_MODEL_PATH "/tmp/deviceModel"
#define DEVELOPER_MODE_PATH "/mnt/SDCARD/.bloom-dev"
#define GAME_PAGE_SIZE 100
#define GAME_CAPACITY_MAX 4096
#define FAVORITES_CAPACITY_MAX 100
#define RECENTS_CAPACITY_MAX 100
#define APPS_CAPACITY_MAX 100
#define LAUNCH_READY_EXIT 20
#define RESTART_NORMAL_EXIT 21

static const char *screen_labels[BLOOM_UI_DESTINATION_COUNT] = {
    "BloomOS",
    "Games",
    "Favorites",
    "Recent",
    "Apps",
    "Settings",
};

static void fill_rect(SDL_Surface *screen, int x, int y, int width, int height, uint32_t rgb);

static int developer_mode_enabled(void)
{
    struct stat status;
    return lstat(DEVELOPER_MODE_PATH, &status) == 0 && S_ISREG(status.st_mode);
}

static int stage_game_with_core(const BloomLibraryGame *game, const char *core)
{
    char error[256] = {0};
    return bloom_shell_stage_game(game, core, REQUEST_PATH, COMMAND_PATH, SESSION_REQUEST_PATH,
                                  SESSION_BINARY, error, sizeof(error));
}

static int stage_game(const BloomLibraryGame *game)
{
    char core[128] = {0};
    if (game == NULL)
        return -1;
    if (bloom_shell_detect_core("/mnt/SDCARD", game->launch_path, core, sizeof(core)) != 0) {
        const char *default_core = bloom_shell_games_default_core(game->system_id);
        if (default_core == NULL)
            return -1;
        snprintf(core, sizeof(core), "%s", default_core);
    }
    return stage_game_with_core(game, core);
}

static int favorite_index(const BloomLibraryGame *favorites, size_t count, const char *game_id)
{
    for (size_t index = 0; index < count; ++index)
        if (strcmp(favorites[index].bloom_game_id, game_id) == 0)
            return (int)index;
    return -1;
}

static int favorite_set(const BloomLibraryGame *game, int favorite)
{
    sqlite3 *database = NULL;
    int changed = 0;
    int result = sqlite3_open_v2(DATABASE_PATH, &database, SQLITE_OPEN_READWRITE, NULL);
    if (result == SQLITE_OK)
        result = bloom_library_favorite_set(database, game->bloom_game_id, favorite, &changed);
    if (database != NULL)
        sqlite3_close(database);
    return result == SQLITE_OK ? 0 : -1;
}

static int favorite_toggle(const BloomLibraryGame *game, BloomLibraryGame *favorites,
                           size_t *favorite_count, BloomUiFocus *favorites_focus,
                           size_t visible_rows)
{
    int index = favorite_index(favorites, *favorite_count, game->bloom_game_id);
    int favorite = index < 0;
    if (favorite && *favorite_count >= FAVORITES_CAPACITY_MAX)
        return -1;
    if (favorite_set(game, favorite) != 0)
        return -1;
    if (favorite && *favorite_count < FAVORITES_CAPACITY_MAX)
        favorites[(*favorite_count)++] = *game;
    else if (!favorite) {
        if ((size_t)index + 1 < *favorite_count)
            memmove(&favorites[index], &favorites[index + 1],
                    (*favorite_count - (size_t)index - 1) * sizeof(favorites[0]));
        memset(&favorites[*favorite_count - 1], 0, sizeof(favorites[0]));
        (*favorite_count)--;
    }
    bloom_ui_focus_set_count(favorites_focus, *favorite_count, visible_rows);
    return 0;
}

static const BloomLibraryGame *game_source(BloomUiDestination destination,
                                           const BloomShellGamesBrowser *browser,
                                           const BloomLibraryGame *games,
                                           const BloomLibraryGame *favorites, size_t favorite_count,
                                           const BloomLibraryGame *recents, size_t recent_count,
                                           size_t *count)
{
    *count = 0;
    if (destination == BLOOM_UI_DESTINATION_GAMES) {
        const BloomShellGamesSystem *system = bloom_shell_games_current(browser);
        if (system != NULL) {
            *count = system->focus.item_count;
            return games + system->game_offset;
        }
    }
    else if (destination == BLOOM_UI_DESTINATION_FAVORITES) {
        *count = favorite_count;
        return favorites;
    }
    else if (destination == BLOOM_UI_DESTINATION_RECENT) {
        *count = recent_count;
        return recents;
    }
    return NULL;
}

static const BloomLibraryGame *selected_game(BloomUiDestination destination,
                                             const BloomShellGamesBrowser *browser,
                                             const BloomLibraryGame *games,
                                             const BloomLibraryGame *favorites,
                                             const BloomUiFocus *favorites_focus,
                                             const BloomLibraryGame *recents,
                                             const BloomUiFocus *recent_focus,
                                             const BloomShellSearch *search)
{
    if (search->active)
        return search->focus.item_count > 0 ? search->results[search->focus.selected] : NULL;
    if (destination == BLOOM_UI_DESTINATION_GAMES) {
        const BloomShellGamesSystem *system = bloom_shell_games_current(browser);
        return system != NULL && system->focus.item_count > 0
                   ? &games[system->game_offset + system->focus.selected]
                   : NULL;
    }
    if (destination == BLOOM_UI_DESTINATION_FAVORITES)
        return favorites_focus->item_count > 0 ? &favorites[favorites_focus->selected] : NULL;
    if (destination == BLOOM_UI_DESTINATION_RECENT)
        return recent_focus->item_count > 0 ? &recents[recent_focus->selected] : NULL;
    return NULL;
}

static void settings_focus_step(BloomUiFocus *focus,
                                const BloomShellCapabilities *capabilities, int direction,
                                size_t visible_rows, int *held_repeats)
{
    int steps = held_repeats != NULL && *held_repeats >= 6 ? 2 : 1;
    for (int step = 0; step < steps; ++step)
        focus->selected =
            bloom_shell_settings_next_selectable(capabilities, focus->selected, direction);
    if (held_repeats != NULL && *held_repeats < 12)
        (*held_repeats)++;
    if (focus->selected < focus->window_start)
        focus->window_start = focus->selected;
    else if (visible_rows > 0 && focus->selected >= focus->window_start + visible_rows)
        focus->window_start = focus->selected - visible_rows + 1;
}

static int load_catalog(BloomLibraryGame **games, size_t *game_count, BloomLibraryGame *recents,
                        size_t *recent_count, BloomLibraryGame *favorites, size_t *favorite_count,
                        BloomLibraryApp *apps, size_t *app_count, int include_development,
                        BloomShellGamesBrowser *browser)
{
    sqlite3 *database = NULL;
    *games = calloc(GAME_CAPACITY_MAX, sizeof(**games));
    *game_count = 0;
    *recent_count = 0;
    *favorite_count = 0;
    *app_count = 0;
    if (*games == NULL ||
        sqlite3_open_v2(DATABASE_PATH, &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_exec(database, "PRAGMA query_only=ON", NULL, NULL, NULL) != SQLITE_OK) {
        if (database != NULL)
            sqlite3_close(database);
        free(*games);
        *games = NULL;
        return -1;
    }
    bloom_shell_games_init(browser);
    int result = bloom_library_query_recents(database, NULL, RECENTS_CAPACITY_MAX, recents,
                                             RECENTS_CAPACITY_MAX, recent_count);
    if (result == SQLITE_OK)
        result = bloom_library_query_favorites(database, NULL, FAVORITES_CAPACITY_MAX, favorites,
                                               FAVORITES_CAPACITY_MAX, favorite_count);
    if (result == SQLITE_OK)
        result = bloom_library_query_apps(database, include_development, APPS_CAPACITY_MAX, apps,
                                          APPS_CAPACITY_MAX, app_count);
    BloomLibrarySystem systems[BLOOM_SHELL_SYSTEM_CAPACITY] = {0};
    size_t system_count = 0;
    if (result == SQLITE_OK)
        result = bloom_library_query_systems(database, BLOOM_SHELL_SYSTEM_CAPACITY, systems,
                                             BLOOM_SHELL_SYSTEM_CAPACITY, &system_count);
    for (size_t system_index = 0;
         result == SQLITE_OK && system_index < system_count && *game_count < GAME_CAPACITY_MAX;
         ++system_index) {
        size_t offset = *game_count;
        char cursor[79] = {0};
        while (result == SQLITE_OK && *game_count < GAME_CAPACITY_MAX) {
            size_t remaining = GAME_CAPACITY_MAX - *game_count;
            size_t limit = remaining < GAME_PAGE_SIZE ? remaining : GAME_PAGE_SIZE;
            BloomLibraryGamePage page = {0};
            result = bloom_library_query_games(
                database, systems[system_index].system_id, cursor[0] == '\0' ? NULL : cursor,
                limit, *games + *game_count, remaining, &page);
            if (result != SQLITE_OK)
                break;
            *game_count += page.count;
            if (!page.has_more)
                break;
            snprintf(cursor, sizeof(cursor), "%s", page.next_cursor);
        }
        size_t loaded = *game_count - offset;
        char core[128] = {0};
        const char *default_core = bloom_shell_games_default_core(systems[system_index].system_id);
        if (loaded > 0 &&
            bloom_shell_detect_core("/mnt/SDCARD", (*games)[offset].launch_path, core,
                                    sizeof(core)) != 0 &&
            default_core != NULL)
            snprintf(core, sizeof(core), "%s", default_core);
        if (loaded == 0 || core[0] == '\0' ||
            bloom_shell_games_add(browser, &systems[system_index], offset, loaded, core) != 0)
            *game_count = offset;
    }
    sqlite3_close(database);
    if (result != SQLITE_OK) {
        free(*games);
        *games = NULL;
        *game_count = 0;
        return -1;
    }
    return 0;
}

static void render_label(SDL_Surface *screen, TTF_Font *font, const char *label, int x, int y,
                         int maximum_width, SDL_Color color)
{
    char bounded[96];
    snprintf(bounded, sizeof(bounded), "%s", label);
    int width = 0;
    int height = 0;
    while (bounded[0] != '\0' &&
           (TTF_SizeUTF8(font, bounded, &width, &height) != 0 || width > maximum_width))
        bounded[strlen(bounded) - 1] = '\0';
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, bounded, color);
    if (text != NULL) {
        SDL_Rect target = {(Sint16)x, (Sint16)y, 0, 0};
        SDL_BlitSurface(text, NULL, screen, &target);
        SDL_FreeSurface(text);
    }
}

static int settings_flat_label(const BloomShellCapabilities *capabilities,
                               const BloomShellQuickValues *values,
                               const BloomShellStatus *status, int support_export_result,
                               int update_confirm_result, int ra_form_result, size_t row,
                               char *label, size_t label_size)
{
    BloomShellSettingsRow settings_row;
    if (bloom_shell_settings_row(capabilities, row, &settings_row) != 0)
        return -1;
    if (settings_row.id == BLOOM_SHELL_SETTINGS_UPDATE) {
        int length = snprintf(
            label, label_size, "Update       %s",
            update_confirm_result > 0   ? "Confirmed"
            : update_confirm_result < 0 ? "Confirmation failed"
            : status->ready && status->update_healthy &&
                    strcmp(status->update_phase, "testing") == 0
                ? "Confirm tested update"
                : "Up to date");
        return length >= 0 && (size_t)length < label_size ? 0 : -1;
    }
    if (settings_row.id == BLOOM_SHELL_SETTINGS_STORAGE)
        return bloom_shell_storage_label(status, label, label_size);
    if (settings_row.id == BLOOM_SHELL_SETTINGS_HEALTH) {
        if (support_export_result != 0) {
            int length = snprintf(label, label_size, "Health: Support export %s",
                                  support_export_result > 0 ? "complete" : "failed");
            return length >= 0 && (size_t)length < label_size ? 0 : -1;
        }
        return bloom_shell_status_label(status, 0, label, label_size);
    }
    if (settings_row.id == BLOOM_SHELL_SETTINGS_ABOUT) {
        int length = snprintf(label, label_size, "About       BloomOS %s", BLOOM_VERSION);
        return length >= 0 && (size_t)length < label_size ? 0 : -1;
    }
    if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_ACCOUNT)
        return bloom_shell_status_label(status, 2, label, label_size);
    if (settings_row.id == BLOOM_SHELL_SETTINGS_RA_CONNECTION) {
        const char *connection = ra_form_result == 1                   ? "Sign-in complete"
                                 : ra_form_result == -1                ? "Sign-in failed"
                                 : ra_form_result == 2                 ? "Signed out"
                                 : ra_form_result == -2                ? "Sign-out failed"
                                 : status->ready && status->ra_healthy ? "Ready"
                                                                       : "Needs attention";
        int length = snprintf(label, label_size, "Connection       %s", connection);
        return length >= 0 && (size_t)length < label_size ? 0 : -1;
    }
    if (settings_row.id == BLOOM_SHELL_SETTINGS_DIAGNOSTICS) {
        int length = snprintf(label, label_size, "Diagnostics");
        return length >= 0 && (size_t)length < label_size ? 0 : -1;
    }
    return bloom_shell_settings_row_format(capabilities, values, row, label, label_size);
}

static void draw_ra_form(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                         const BloomShellRaForm *form)
{
    if (bloom_ui_render_keyboard(screen, layout, &form->keyboard) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    char label[192];
    if (bloom_shell_ra_form_label(form, label, sizeof(label)) == 0)
        render_label(screen, font, label, layout->content.x + 16, layout->content.y + 8,
                     layout->content.width - 32, cream);
    render_label(screen, font, "A Type  B Delete  X Mode  Y Field  START Save",
                 layout->content.x + 16, layout->content.y + 40, layout->content.width - 32,
                 cream);

    int height = layout->content.height * 3 / 4;
    int keyboard_y = layout->content.y + layout->content.height - height;
    int gap = layout->viewport_width >= 720 ? 6 : 4;
    int row_height = height / 4;
    for (size_t row = 0; row < 4; ++row) {
        size_t count = bloom_ui_keyboard_row_length(form->keyboard.mode, row);
        int key_width = (layout->content.width - gap * ((int)count + 1)) / (int)count;
        int row_width = (int)count * key_width + ((int)count - 1) * gap;
        int row_x = layout->content.x + (layout->content.width - row_width) / 2;
        for (size_t column = 0; column < count; ++column) {
            BloomUiKeyboardFocus key = {.mode = form->keyboard.mode, .row = row, .column = column};
            char glyph[2] = {bloom_ui_keyboard_character(&key), '\0'};
            int selected = form->keyboard.row == row && form->keyboard.column == column;
            render_label(screen, font, glyph,
                         row_x + (int)column * (key_width + gap) + key_width / 3,
                         keyboard_y + (int)row * row_height + gap + row_height / 5,
                         key_width / 2, selected ? canvas : cream);
        }
    }
}

static void draw_search(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                        const BloomShellSearch *search)
{
    if (bloom_ui_render_keyboard(screen, layout, &search->keyboard) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    char label[128];
    snprintf(label, sizeof(label), "Search: %s", search->query);
    render_label(screen, font, label, layout->content.x + 16, layout->content.y + 8,
                 layout->content.width - 32, cream);
    render_label(screen, font, "A Type   Y Delete   X Mode   SELECT Apply   B Cancel",
                 layout->content.x + 16, layout->content.y + 40, layout->content.width - 32,
                 cream);
}

static void draw_ra_sign_out(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                             const BloomUiDialogFocus *dialog)
{
    if (bloom_ui_render_dialog(screen, layout, dialog) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 4 / 5;
    int height = layout->viewport_height / 3;
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    int gap = 12;
    int button_width = (width - 48 - gap) / 2;
    int button_height = layout->row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    render_label(screen, font, "Sign out of RetroAchievements?", x + 24, y + 18, width - 48,
                 cream);
    render_label(screen, font, "Cancel", x + 24 + button_width / 3,
                 button_y + button_height / 4, button_width * 2 / 3,
                 dialog->selected == 0 ? canvas : cream);
    render_label(screen, font, "Sign out", x + 24 + button_width + gap + button_width / 4,
                 button_y + button_height / 4, button_width * 3 / 4,
                 dialog->selected == 1 ? canvas : cream);
}

static void draw_update_confirm(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                                const BloomUiDialogFocus *dialog)
{
    if (bloom_ui_render_dialog(screen, layout, dialog) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 4 / 5;
    int height = layout->viewport_height / 3;
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    int gap = 12;
    int button_width = (width - 48 - gap) / 2;
    int button_height = layout->row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    render_label(screen, font, "Keep this tested update?", x + 24, y + 18, width - 48, cream);
    render_label(screen, font, "Cancel", x + 24 + button_width / 3,
                 button_y + button_height / 4, button_width * 2 / 3,
                 dialog->selected == 0 ? canvas : cream);
    render_label(screen, font, "Confirm", x + 24 + button_width + gap + button_width / 4,
                 button_y + button_height / 4, button_width * 3 / 4,
                 dialog->selected == 1 ? canvas : cream);
}

static void draw_recent_remove_confirm(SDL_Surface *screen, const BloomUiLayout *layout,
                                       TTF_Font *font, const BloomUiDialogFocus *dialog)
{
    if (bloom_ui_render_dialog(screen, layout, dialog) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 4 / 5;
    int height = layout->viewport_height / 3;
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    int gap = 12;
    int button_width = (width - 48 - gap) / 2;
    int button_height = layout->row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    render_label(screen, font, "Remove this game from Recent?", x + 24, y + 18, width - 48,
                 cream);
    render_label(screen, font, "Cancel", x + 24 + button_width / 3,
                 button_y + button_height / 4, button_width * 2 / 3,
                 dialog->selected == 0 ? canvas : cream);
    render_label(screen, font, "Remove", x + 24 + button_width + gap + button_width / 4,
                 button_y + button_height / 4, button_width * 3 / 4,
                 dialog->selected == 1 ? canvas : cream);
}

static void draw_rollback_confirm(SDL_Surface *screen, const BloomUiLayout *layout,
                                  TTF_Font *font, const BloomUiDialogFocus *dialog)
{
    if (bloom_ui_render_dialog(screen, layout, dialog) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 4 / 5;
    int height = layout->viewport_height / 3;
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    int gap = 12;
    int button_width = (width - 48 - gap) / 2;
    int button_height = layout->row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    render_label(screen, font, "Restore the previous signed version?", x + 24, y + 18,
                 width - 48, cream);
    render_label(screen, font, "Cancel", x + 24 + button_width / 3,
                 button_y + button_height / 4, button_width * 2 / 3,
                 dialog->selected == 0 ? canvas : cream);
    render_label(screen, font, "Restore", x + 24 + button_width + gap + button_width / 4,
                 button_y + button_height / 4, button_width * 3 / 4,
                 dialog->selected == 1 ? canvas : cream);
}

static void draw_reset_confirm(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                               const BloomUiDialogFocus *dialog)
{
    if (bloom_ui_render_dialog(screen, layout, dialog) != 0)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 4 / 5;
    int height = layout->viewport_height / 3;
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    int gap = 12;
    int button_width = (width - 48 - gap) / 2;
    int button_height = layout->row_height * 2 / 3;
    int button_y = y + height - button_height - 20;
    render_label(screen, font, "Reset BloomOS settings to defaults?", x + 24, y + 18,
                 width - 48, cream);
    render_label(screen, font, "Cancel", x + 24 + button_width / 3,
                 button_y + button_height / 4, button_width * 2 / 3,
                 dialog->selected == 0 ? canvas : cream);
    render_label(screen, font, "Reset", x + 24 + button_width + gap + button_width / 4,
                 button_y + button_height / 4, button_width * 3 / 4,
                 dialog->selected == 1 ? canvas : cream);
}

static void draw_game_actions(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                              const BloomUiFocus *focus, int favorite, int recent)
{
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color canvas = {33, 23, 17, 0};
    int width = layout->content.width * 3 / 4;
    int height = layout->row_height * (recent ? 4 : 3);
    int x = (layout->viewport_width - width) / 2;
    int y = (layout->viewport_height - height) / 2;
    fill_rect(screen, x - 4, y - 4, width + 8, height + 8, 0xD86A2C);
    fill_rect(screen, x, y, width, height, 0x493025);
    const char *labels[] = {"Play / Resume", favorite ? "Unfavorite" : "Favorite",
                            "Remove from Recent"};
    size_t action_count = recent ? 3 : 2;
    for (size_t row = 0; row < action_count; ++row) {
        int row_y = y + 16 + (int)row * layout->row_height;
        int selected = focus->selected == row;
        fill_rect(screen, x + 16, row_y, width - 32, layout->row_height - 6,
                  selected ? 0xF3E2BD : 0x352319);
        render_label(screen, font, labels[row], x + 32, row_y + layout->row_height / 3,
                     width - 64, selected ? canvas : cream);
    }
}

static void fill_rect(SDL_Surface *screen, int x, int y, int width, int height, uint32_t rgb)
{
    SDL_Rect rectangle = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
    SDL_FillRect(screen, &rectangle,
                 SDL_MapRGB(screen->format, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb));
}

static void draw_root_icon(SDL_Surface *screen, BloomShellRootDestination destination, int x, int y,
                           int size, int selected)
{
    uint32_t foreground = selected ? 0x211711 : 0xF3E2BD;
    uint32_t accent = 0xD86A2C;
    int unit = size / 7;
    if (destination == BLOOM_SHELL_ROOT_GAMES) {
        fill_rect(screen, x + unit, y + unit * 2, unit * 5, unit * 3, foreground);
        fill_rect(screen, x, y + unit * 3, unit, unit, foreground);
        fill_rect(screen, x + unit * 6, y + unit * 3, unit, unit, foreground);
        fill_rect(screen, x + unit * 2, y + unit * 3, unit, unit, accent);
        fill_rect(screen, x + unit * 4, y + unit * 2, unit, unit, accent);
    }
    else if (destination == BLOOM_SHELL_ROOT_FAVORITES) {
        fill_rect(screen, x + unit * 3, y, unit, unit * 7, foreground);
        fill_rect(screen, x, y + unit * 3, unit * 7, unit, foreground);
        fill_rect(screen, x + unit, y + unit, unit * 5, unit * 5, foreground);
        fill_rect(screen, x + unit * 2, y + unit * 2, unit * 3, unit * 3, accent);
    }
    else if (destination == BLOOM_SHELL_ROOT_RECENT) {
        fill_rect(screen, x + unit, y + unit, unit * 5, unit, foreground);
        fill_rect(screen, x + unit, y + unit * 5, unit * 5, unit, foreground);
        fill_rect(screen, x, y + unit * 2, unit, unit * 3, foreground);
        fill_rect(screen, x + unit * 6, y + unit * 2, unit, unit * 3, foreground);
        fill_rect(screen, x + unit * 3, y + unit * 2, unit, unit * 3, accent);
        fill_rect(screen, x + unit * 3, y + unit * 4, unit * 2, unit, accent);
    }
    else if (destination == BLOOM_SHELL_ROOT_APPS) {
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column)
                fill_rect(screen, x + column * unit * 4, y + row * unit * 4, unit * 3, unit * 3,
                          row == 1 && column == 1 ? accent : foreground);
    }
    else {
        for (int row = 0; row < 3; ++row) {
            fill_rect(screen, x, y + row * unit * 3, unit * 7, unit, foreground);
            fill_rect(screen, x + (row == 1 ? unit * 4 : unit * 2), y + row * unit * 3 - unit,
                      unit, unit * 3, accent);
        }
    }
}

static void draw_game_preview(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                              const BloomLibraryGame *game,
                              const BloomShellAchievementIndex *achievement_index)
{
    if (game == NULL)
        return;
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color sand = {205, 175, 123, 0};
    int list_width = layout->content.width * 58 / 100;
    int x = layout->content.x + list_width + 16;
    int width = layout->content.width - list_width - 16;
    int cover_height = layout->content.height * 2 / 3;
    fill_rect(screen, x, layout->content.y + 4, width, cover_height, 0x352319);
    fill_rect(screen, x + 5, layout->content.y + 9, width - 10, cover_height - 10, 0x493025);
    int mark = layout->viewport_height >= 540 ? 10 : 8;
    int center_x = x + width / 2;
    int center_y = layout->content.y + cover_height / 2;
    fill_rect(screen, center_x - mark / 2, center_y - mark * 2, mark, mark, 0xD86A2C);
    fill_rect(screen, center_x - mark / 2, center_y + mark, mark, mark, 0xD86A2C);
    fill_rect(screen, center_x - mark * 2, center_y - mark / 2, mark, mark, 0xE2A93B);
    fill_rect(screen, center_x + mark, center_y - mark / 2, mark, mark, 0xE2A93B);
    fill_rect(screen, center_x - mark / 2, center_y - mark / 2, mark, mark, 0xF3E2BD);
    render_label(screen, font, game->display_title, x, layout->content.y + cover_height + 14,
                 width, cream);
    render_label(screen, font, game->system_id, x,
                 layout->content.y + cover_height + layout->row_height, width, sand);
    if (bloom_shell_achievements_contains(achievement_index, game->bloom_game_id))
        render_label(screen, font, "RetroAchievements", x,
                     layout->content.y + cover_height + layout->row_height * 3 / 2, width,
                     (SDL_Color){226, 169, 59, 0});
}

static void draw_root(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                      const BloomShellRootState *root, const BloomLibraryGame *recent)
{
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color sand = {205, 175, 123, 0};
    int content_x = layout->content.x + 16;
    int content_width = layout->content.width - 32;
    int hero_height = root->has_continue ? layout->content.height * 3 / 5 : 0;
    if (root->has_continue) {
        render_label(screen, font, "CONTINUE PLAYING", content_x, layout->content.y + 8,
                     content_width, sand);
        int hero_y = layout->content.y + layout->row_height;
        fill_rect(screen, content_x, hero_y, content_width, hero_height - layout->row_height,
                  root->continue_focused ? 0x493025 : 0x352319);
        if (root->continue_focused)
            fill_rect(screen, content_x, hero_y, 6, hero_height - layout->row_height, 0xD86A2C);
        int preview_width = content_width / 3;
        int preview_height = hero_height - layout->row_height - 24;
        fill_rect(screen, content_x + 20, hero_y + 12, preview_width, preview_height, 0x211711);
        fill_rect(screen, content_x + 25, hero_y + 17, preview_width - 10, preview_height - 10,
                  0x352319);
        int icon_size = preview_height > 42 ? 35 : 28;
        draw_root_icon(screen, BLOOM_SHELL_ROOT_GAMES,
                       content_x + 20 + (preview_width - icon_size) / 2,
                       hero_y + 12 + (preview_height - icon_size) / 2, icon_size, 0);
        int text_x = content_x + preview_width + 44;
        render_label(screen, font, recent->display_title, text_x,
                     hero_y + layout->row_height / 2, content_width - 48, cream);
        render_label(screen, font, recent->system_id, text_x, hero_y + layout->row_height * 3 / 2,
                     content_width - preview_width - 64, sand);
        render_label(screen, font, "A Resume", text_x, hero_y + layout->row_height * 2,
                     content_width - preview_width - 64, sand);
    }
    int rail_y = layout->content.y + hero_height + (root->has_continue ? 8 : layout->row_height);
    int gap = 6;
    int rail_width = content_width - gap * (BLOOM_SHELL_ROOT_COUNT - 1);
    int item_width = rail_width / BLOOM_SHELL_ROOT_COUNT;
    int item_height = layout->row_height * 3 / 2;
    for (int item = 0; item < BLOOM_SHELL_ROOT_COUNT; ++item) {
        int x = content_x + item * (item_width + gap);
        int selected = !root->continue_focused && item == root->selected;
        fill_rect(screen, x, rail_y, item_width, item_height, selected ? 0xF3E2BD : 0x352319);
        if (selected)
            fill_rect(screen, x, rail_y + item_height - 5, item_width, 5, 0xD86A2C);
        int icon_size = layout->viewport_height >= 540 ? 35 : 28;
        draw_root_icon(screen, (BloomShellRootDestination)item, x + (item_width - icon_size) / 2,
                       rail_y + 8, icon_size, selected);
        render_label(screen, font, bloom_shell_root_label((BloomShellRootDestination)item), x + 8,
                     rail_y + item_height - layout->row_height / 2, item_width - 16,
                     selected ? (SDL_Color){33, 23, 17, 0} : cream);
    }
}

static void draw_first_run(SDL_Surface *screen, const BloomUiLayout *layout, TTF_Font *font,
                           size_t game_count, int result)
{
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color sand = {205, 175, 123, 0};
    char games_ready[80];
    snprintf(games_ready, sizeof(games_ready), "%zu game%s ready", game_count,
             game_count == 1 ? "" : "s");
    int x = layout->content.x + layout->margin * 2;
    int width = layout->content.width - layout->margin * 4;
    int y = layout->content.y + layout->row_height;
    render_label(screen, font, "Your library is ready.", x, y, width, cream);
    render_label(screen, font, games_ready, x, y + layout->row_height, width, sand);
    render_label(screen, font, "Games, saves, and settings stay in place.", x,
                 y + layout->row_height * 2, width, cream);
    render_label(screen, font,
                 result < 0 ? "Setup needs attention. Press A to try again."
                            : "Press A to finish setup.",
                 x, y + layout->row_height * 4, width, result < 0 ? sand : cream);
}

static void draw(SDL_Surface *screen, SDL_Surface *video, const BloomUiLayout *layout, TTF_Font *font,
                 TTF_Font *compact_font,
                 BloomUiDestination destination, const BloomShellRootState *root,
                 const BloomShellGamesBrowser *games_browser, const BloomUiFocus *favorites_focus,
                 const BloomUiFocus *recent_focus, const BloomLibraryGame *games,
                 const BloomShellAchievementIndex *achievement_index,
                 const BloomLibraryGame *favorites, const BloomLibraryGame *recent, int has_recent,
                 const BloomShellSearch *search,
                 const BloomUiFocus *settings_focus, const BloomUiFocus *apps_focus,
                 const BloomLibraryApp *apps, const BloomShellStatus *status,
                 const BloomShellCapabilities *capabilities,
                 const BloomShellQuickValues *quick_values, int support_export_result,
                 int update_confirm_result, int ra_form_result, int quick_settings,
                 int ra_form_open, const BloomShellRaForm *ra_form,
                 int ra_sign_out_open, const BloomUiDialogFocus *ra_sign_out_dialog,
                 int update_confirm_open, const BloomUiDialogFocus *update_confirm_dialog,
                 const BloomUiFocus *quick_settings_focus, int game_actions_open,
                 const BloomUiFocus *game_actions_focus, int action_game_favorite,
                 int action_game_recent, int recent_remove_confirm_open,
                 const BloomUiDialogFocus *recent_remove_dialog, int safe_mode,
                 const BloomUiFocus *safe_mode_focus, int rollback_result,
                 int rollback_confirm_open, const BloomUiDialogFocus *rollback_dialog,
                 int reset_result, int reset_confirm_open,
                 const BloomUiDialogFocus *reset_dialog, int first_run_open,
                 int first_run_result, size_t game_count)
{
    const BloomShellGamesSystem *games_system = bloom_shell_games_current(games_browser);
    const BloomUiFocus empty_focus = {0};
    const BloomUiFocus *games_focus = games_system == NULL ? &empty_focus : &games_system->focus;
    const BloomUiFocus *focus = destination == BLOOM_UI_DESTINATION_FAVORITES
                                    ? favorites_focus
                                : destination == BLOOM_UI_DESTINATION_RECENT ? recent_focus
                                                                             : games_focus;
    const BloomLibraryGame *rows =
        destination == BLOOM_UI_DESTINATION_FAVORITES
            ? favorites
        : destination == BLOOM_UI_DESTINATION_RECENT
            ? recent
            : games + (games_system == NULL ? 0 : games_system->game_offset);
    if (search->active) {
        focus = &search->focus;
    }
    int game_destination = destination == BLOOM_UI_DESTINATION_GAMES ||
                           destination == BLOOM_UI_DESTINATION_FAVORITES ||
                           destination == BLOOM_UI_DESTINATION_RECENT;
    const BloomLibraryGame *preview_game = NULL;
    if (game_destination && focus->item_count > 0)
        preview_game = search->active ? search->results[focus->selected] : &rows[focus->selected];
    int safe_mode_root = safe_mode && destination == BLOOM_UI_DESTINATION_ROOT;
    size_t item_count = quick_settings
                            ? quick_settings_focus->item_count
                        : safe_mode_root                               ? safe_mode_focus->item_count
                        : destination == BLOOM_UI_DESTINATION_ROOT     ? 0
                        : destination == BLOOM_UI_DESTINATION_APPS     ? apps_focus->item_count
                        : destination == BLOOM_UI_DESTINATION_SETTINGS ? settings_focus->item_count
                                                                       : focus->item_count;
    size_t selected = quick_settings
                          ? quick_settings_focus->selected
                      : safe_mode_root                               ? safe_mode_focus->selected
                      : destination == BLOOM_UI_DESTINATION_ROOT     ? 0
                      : destination == BLOOM_UI_DESTINATION_APPS     ? apps_focus->selected
                      : destination == BLOOM_UI_DESTINATION_SETTINGS ? settings_focus->selected
                                                                     : focus->selected;
    size_t window_start = quick_settings
                              ? quick_settings_focus->window_start
                          : safe_mode_root                           ? safe_mode_focus->window_start
                          : destination == BLOOM_UI_DESTINATION_ROOT ? 0
                          : destination == BLOOM_UI_DESTINATION_APPS ? apps_focus->window_start
                          : destination == BLOOM_UI_DESTINATION_SETTINGS
                              ? settings_focus->window_start
                              : focus->window_start;
    BloomUiScene scene = {
        .destination = quick_settings || safe_mode_root ? BLOOM_UI_DESTINATION_SETTINGS
                                                        : destination,
        .item_count = item_count,
        .selected = selected,
        .window_start = window_start,
        .row_width_percent = game_destination ? 58 : 100,
        .healthy = status->ready && status->healthy,
    };
    bloom_ui_render_shell(screen, layout, &scene);
    SDL_Color cream = {243, 226, 189, 0};
    SDL_Color sand = {205, 175, 123, 0};
    char games_header[640];
    const char *header = quick_settings   ? "Quick Settings"
                         : first_run_open ? "Welcome to BloomOS"
                         : safe_mode_root ? "Safe Mode"
                                          : screen_labels[destination];
    if (!quick_settings && destination == BLOOM_UI_DESTINATION_GAMES && games_system != NULL) {
        snprintf(games_header, sizeof(games_header), "Games   < %s >", games_system->system.label);
        header = games_header;
    }
    render_label(screen, font, header,
                 layout->header.height + layout->margin, layout->header.y + 18,
                 layout->viewport_width * 3 / 4 - layout->header.height - layout->margin * 2,
                 cream);
    char device_status[80] = {0};
    if (quick_values->battery_capacity_available)
        snprintf(device_status, sizeof(device_status), "%s%s%d%%",
                 capabilities->wifi
                     ? (quick_values->wifi_enabled ? "Wi-Fi  " : "Wi-Fi off  ")
                     : "",
                 quick_values->battery_charging ? "+" : "", quick_values->battery_capacity);
    else if (capabilities->wifi)
        snprintf(device_status, sizeof(device_status), "%s",
                 quick_values->wifi_enabled ? "Wi-Fi" : "Wi-Fi off");
    if (device_status[0] != '\0')
        render_label(screen, compact_font, device_status, layout->viewport_width * 3 / 4,
                     layout->header.y + 18, layout->viewport_width / 4 - layout->margin, sand);
    if (quick_settings) {
        for (size_t row = 0; row < quick_settings_focus->item_count; ++row) {
            char label[96];
            if (bloom_shell_quick_settings_format(capabilities, quick_values, row, label,
                                                  sizeof(label)) == 0)
                render_label(screen, font, label, layout->content.x + 20,
                             layout->content.y + (int)row * layout->row_height +
                                 layout->row_height / 3,
                             layout->content.width - 40, cream);
        }
    }
    else if (safe_mode_root) {
        for (size_t row = 0; row < safe_mode_focus->item_count; ++row) {
            char label[96];
            const char *base = bloom_shell_safe_mode_label((BloomShellSafeModeRow)row);
            if (row == BLOOM_SHELL_SAFE_MODE_HEALTH)
                snprintf(label, sizeof(label), "%s       %s", base,
                         status->ready && status->system_healthy ? "Good" : "Needs attention");
            else if (row == BLOOM_SHELL_SAFE_MODE_EXPORT_SUPPORT && support_export_result != 0)
                snprintf(label, sizeof(label), "%s       %s", base,
                         support_export_result > 0 ? "Complete" : "Failed");
            else if (row == BLOOM_SHELL_SAFE_MODE_ROLLBACK && rollback_result != 0)
                snprintf(label, sizeof(label), "%s       %s", base,
                         rollback_result == 2  ? "Restoring..."
                         : rollback_result > 0 ? "Restart required"
                                               : "Unavailable");
            else if (row == BLOOM_SHELL_SAFE_MODE_RESET_SETTINGS && reset_result != 0)
                snprintf(label, sizeof(label), "%s       %s", base,
                         reset_result == 2  ? "Resetting..."
                         : reset_result > 0 ? "Complete"
                                            : "Failed");
            else
                snprintf(label, sizeof(label), "%s", base == NULL ? "" : base);
            render_label(screen, font, label, layout->content.x + 20,
                         layout->content.y + (int)row * layout->row_height +
                             layout->row_height / 3,
                         layout->content.width - 40, cream);
        }
    }
    else if (first_run_open)
        draw_first_run(screen, layout, font, game_count, first_run_result);
    else if (destination == BLOOM_UI_DESTINATION_ROOT)
        draw_root(screen, layout, font, root, recent);
    else if (destination == BLOOM_UI_DESTINATION_APPS)
        for (size_t row = 0;
             row < layout->visible_rows && apps_focus->window_start + row < apps_focus->item_count;
             ++row)
            render_label(screen, font, apps[apps_focus->window_start + row].label,
                         layout->content.x + 20,
                         layout->content.y + (int)row * layout->row_height + layout->row_height / 3,
                         layout->content.width - 40, cream);
    else if (destination == BLOOM_UI_DESTINATION_SETTINGS) {
        for (size_t row = 0;
             row < layout->visible_rows &&
             settings_focus->window_start + row < settings_focus->item_count;
             ++row) {
            size_t item = settings_focus->window_start + row;
            char status_label[96];
            BloomShellSettingsRow settings_row;
            const char *label = status_label;
            if (settings_flat_label(capabilities, quick_values, status, support_export_result,
                                    update_confirm_result, ra_form_result, item, status_label,
                                    sizeof(status_label)) != 0 ||
                bloom_shell_settings_row(capabilities, item, &settings_row) != 0)
                label = NULL;
            if (label != NULL)
                render_label(screen, font, label, layout->content.x + 20,
                             layout->content.y + (int)row * layout->row_height +
                                 layout->row_height / 3,
                             layout->content.width - 40,
                             settings_row.kind == BLOOM_SHELL_SETTINGS_ROW_SECTION ? sand
                                                                                   : cream);
            if (label != NULL && settings_row.kind == BLOOM_SHELL_SETTINGS_ROW_SECTION)
                fill_rect(screen, layout->content.x + layout->content.width * 45 / 100,
                          layout->content.y + (int)row * layout->row_height +
                              layout->row_height / 2,
                          layout->content.width * 55 / 100 - 20, 2, 0xE2A93B);
        }
    }
    else if (search->active && search->focus.item_count == 0) {
        render_label(screen, font, "No matching games", layout->content.x + 20,
                     layout->content.y + layout->row_height, layout->content.width - 40, cream);
        render_label(screen, font, "Press SELECT to change the search.", layout->content.x + 20,
                     layout->content.y + layout->row_height * 2, layout->content.width - 40, sand);
    }
    else if (destination == BLOOM_UI_DESTINATION_GAMES && games_focus->item_count == 0) {
        render_label(screen, font, "No supported games found", layout->content.x + 20,
                     layout->content.y + layout->row_height, layout->content.width - 40, cream);
        render_label(screen, font, "Add games to a Roms folder.", layout->content.x + 20,
                     layout->content.y + layout->row_height * 2, layout->content.width - 40, sand);
    }
    else if (destination == BLOOM_UI_DESTINATION_FAVORITES &&
             favorites_focus->item_count == 0) {
        render_label(screen, font, "No favorites yet", layout->content.x + 20,
                     layout->content.y + layout->row_height, layout->content.width - 40, cream);
        render_label(screen, font, "Press Y on a game to add it here.", layout->content.x + 20,
                     layout->content.y + layout->row_height * 2, layout->content.width - 40, sand);
    }
    else if (destination == BLOOM_UI_DESTINATION_RECENT && !has_recent) {
        render_label(screen, font, "Nothing played yet", layout->content.x + 20,
                     layout->content.y + layout->row_height, layout->content.width - 40, cream);
        render_label(screen, font, "Launch a game and it will appear here.", layout->content.x + 20,
                     layout->content.y + layout->row_height * 2, layout->content.width - 40, sand);
    }
    else {
        for (size_t row = 0; row < layout->visible_rows && focus->window_start + row < focus->item_count;
             ++row) {
            const BloomLibraryGame *game = search->active
                                               ? search->results[focus->window_start + row]
                                               : &rows[focus->window_start + row];
            int badge = bloom_shell_achievements_contains(achievement_index,
                                                          game->bloom_game_id);
            if (badge)
                render_label(screen, compact_font, "RA", layout->content.x + 20,
                             layout->content.y + (int)row * layout->row_height +
                                 layout->row_height / 3,
                             30, (SDL_Color){226, 169, 59, 0});
            render_label(screen, font, game->display_title,
                         layout->content.x + (badge ? 58 : 20),
                         layout->content.y + (int)row * layout->row_height + layout->row_height / 3,
                         (game_destination ? layout->content.width * 58 / 100
                                           : layout->content.width) -
                             (badge ? 78 : 40),
                         cream);
        }
    }
    if (game_destination)
        draw_game_preview(screen, layout, font, preview_game, achievement_index);
    const char *footer = quick_settings
                             ? "Left/Right Change   A Toggle   B/START Close"
                         : first_run_open
                             ? "A Finish Setup   MENU Switcher   START Quick"
                         : safe_mode_root
                             ? "A Open   MENU Switcher   START Quick"
                         : safe_mode && destination == BLOOM_UI_DESTINATION_GAMES
                             ? "A Play   X Actions   Y Favorite   SELECT Search   B Safe Mode"
                         : destination == BLOOM_UI_DESTINATION_ROOT
                             ? "Left/Right Choose   A Open   MENU Switcher   START Quick"
                         : destination == BLOOM_UI_DESTINATION_SETTINGS
                             ? "Left/Right Change   A Open/Toggle   B Home   START Quick"
                         : destination == BLOOM_UI_DESTINATION_APPS
                             ? "A Open   B Home   START Quick"
                             : "A Play   X Actions   Y Favorite   SELECT Search   B Home";
    render_label(screen, compact_font, footer, layout->footer.x + 56,
                 layout->footer.y + layout->footer.height / 3, layout->footer.width - 76, sand);
    if (ra_form_open)
        draw_ra_form(screen, layout, font, ra_form);
    if (search->open)
        draw_search(screen, layout, font, search);
    if (ra_sign_out_open)
        draw_ra_sign_out(screen, layout, font, ra_sign_out_dialog);
    if (update_confirm_open)
        draw_update_confirm(screen, layout, font, update_confirm_dialog);
    if (game_actions_open)
        draw_game_actions(screen, layout, font, game_actions_focus, action_game_favorite,
                          action_game_recent);
    if (recent_remove_confirm_open)
        draw_recent_remove_confirm(screen, layout, font, recent_remove_dialog);
    if (rollback_confirm_open)
        draw_rollback_confirm(screen, layout, font, rollback_dialog);
    if (reset_confirm_open)
        draw_reset_confirm(screen, layout, font, reset_dialog);
#ifdef PLATFORM_MIYOOMINI
    bloom_ui_rotate_180(screen);
    SDL_BlitSurface(screen, NULL, video, NULL);
    if (bloom_ui_publish_framebuffer_pages(video, "/dev/fb0", 3) != 0)
        SDL_Flip(video);
#else
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
#endif
}

int main(int argc, char **argv)
{
    int model = 0;
    FILE *model_file = fopen(DEVICE_MODEL_PATH, "r");
    if (model_file == NULL || fscanf(model_file, "%d", &model) != 1) {
        if (model_file != NULL)
            fclose(model_file);
        return 1;
    }
    fclose(model_file);
    int developer_mode = developer_mode_enabled();
    int safe_mode = bloom_shell_safe_mode_enabled(getenv("BLOOM_SAFE_MODE"));
    BloomShellCapabilities capabilities = {0};
    if (bloom_shell_capabilities_from_model(model, developer_mode, &capabilities) != 0)
        return 1;
    BloomShellStatus status = {0};
    bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
    BloomShellFirstRun first_run = {0};
    bloom_shell_first_run_load(BLOOM_SETTINGS_BINARY, &first_run);
    BloomShellQuickValues quick_values = {0};
    bloom_shell_quick_values_load(BLOOM_SETTINGS_BINARY, &quick_values);
    bloom_shell_quick_battery_load(BLOOM_PLATFORM_BINARY, &quick_values);
    BloomLibraryGame *games = NULL;
    BloomLibraryGame recents[RECENTS_CAPACITY_MAX] = {0};
    BloomLibraryGame favorites[FAVORITES_CAPACITY_MAX] = {0};
    BloomLibraryApp apps[APPS_CAPACITY_MAX] = {0};
    BloomShellGamesBrowser games_browser;
    size_t game_count = 0;
    size_t recent_count = 0;
    size_t favorite_count = 0;
    size_t app_count = 0;
    if (load_catalog(&games, &game_count, recents, &recent_count, favorites, &favorite_count, apps,
                     &app_count, developer_mode, &games_browser) != 0)
        return 1;
    BloomShellAchievementIndex achievement_index = {0};
    if (game_count > 0 &&
        bloom_shell_achievements_load(&achievement_index, RA_DATABASE_PATH, game_count) != 0) {
        free(games);
        return 1;
    }
    int has_recent = recent_count > 0;
    if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
        size_t gb_games = 0;
        for (size_t index = 0; index < games_browser.system_count; ++index)
            if (strcmp(games_browser.systems[index].system.system_id, "gb") == 0)
                gb_games = games_browser.systems[index].system.game_count;
        printf("{\"schema\":1,\"service\":\"bloom-shell\",\"ready\":true,\"gb_games\":%zu,"
               "\"gb_recent\":%s,\"gb_favorites\":%zu,\"apps\":%zu,\"health_ready\":%s,"
               "\"healthy\":%s,\"systems\":%zu,\"games\":%zu,\"ra_supported\":%zu}\n",
               gb_games, has_recent ? "true" : "false", favorite_count, app_count,
               status.ready ? "true" : "false", status.healthy ? "true" : "false",
               games_browser.system_count, game_count, achievement_index.count);
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 0;
    }
    if (argc != 1) {
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 2;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) {
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 1;
    }
    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    int width = info != NULL && info->current_w >= 640 ? info->current_w : 640;
    int height = info != NULL && info->current_h >= 480 ? info->current_h : 480;
    BloomUiLayout layout;
    if (bloom_ui_layout_init(width, height, 0, &layout) != 0) {
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 1;
    }
    SDL_Surface *video = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE);
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0, 0, 0, 0);
    TTF_Font *font = TTF_OpenFont("/customer/app/wqy-microhei.ttc", height >= 540 ? 26 : 22);
    TTF_Font *compact_font =
        TTF_OpenFont("/customer/app/wqy-microhei.ttc", height >= 540 ? 20 : 17);
    if (video == NULL || screen == NULL || font == NULL || compact_font == NULL) {
        if (compact_font != NULL)
            TTF_CloseFont(compact_font);
        if (font != NULL)
            TTF_CloseFont(font);
        if (screen != NULL)
            SDL_FreeSurface(screen);
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 70);

    BloomUiDestination destination = BLOOM_UI_DESTINATION_ROOT;
    BloomShellRootState root;
    bloom_shell_root_init(&root, has_recent && !safe_mode);
    BloomUiFocus favorites_focus;
    BloomUiFocus recent_focus;
    BloomUiFocus settings_focus;
    BloomUiFocus apps_focus;
    BloomUiFocus quick_settings_focus;
    BloomUiFocus safe_mode_focus;
    bloom_ui_focus_init(&favorites_focus, favorite_count);
    bloom_ui_focus_init(&recent_focus, recent_count);
    bloom_ui_focus_init(&settings_focus, bloom_shell_settings_count(&capabilities));
    settings_focus.selected = bloom_shell_settings_first_selectable(&capabilities);
    bloom_ui_focus_init(&apps_focus, app_count);
    bloom_ui_focus_init(&quick_settings_focus,
                        bloom_shell_quick_settings_count(&capabilities));
    bloom_ui_focus_init(&safe_mode_focus, BLOOM_SHELL_SAFE_MODE_ROW_COUNT);
    BloomShellSearch search;
    if (bloom_shell_search_init(&search, GAME_CAPACITY_MAX) != 0) {
        TTF_CloseFont(font);
        TTF_CloseFont(compact_font);
        SDL_FreeSurface(screen);
        TTF_Quit();
        SDL_Quit();
        bloom_shell_achievements_destroy(&achievement_index);
        free(games);
        return 1;
    }
    int quick_settings = 0;
    int settings_held_repeats = 0;
    int support_export_result = 0;
    int update_confirm_result = 0;
    int ra_form_result = 0;
    int ra_form_open = 0;
    BloomShellRaForm ra_form;
    bloom_shell_ra_form_init(&ra_form);
    int ra_sign_out_open = 0;
    BloomUiDialogFocus ra_sign_out_dialog = {0};
    int update_confirm_open = 0;
    BloomUiDialogFocus update_confirm_dialog = {0};
    int game_actions_open = 0;
    int action_game_recent = 0;
    BloomLibraryGame action_game = {0};
    BloomUiFocus game_actions_focus;
    bloom_ui_focus_init(&game_actions_focus, 2);
    int recent_remove_confirm_open = 0;
    BloomUiDialogFocus recent_remove_dialog = {0};
    int rollback_result = 0;
    int rollback_confirm_open = 0;
    BloomUiDialogFocus rollback_dialog = {0};
    int reset_result = 0;
    int reset_confirm_open = 0;
    BloomUiDialogFocus reset_dialog = {0};
    int first_run_open = !safe_mode && !first_run.complete;
    int first_run_result = first_run.ready ? 0 : -1;
    draw(screen, video, &layout, font, compact_font, destination, &root, &games_browser, &favorites_focus,
         &recent_focus, games, &achievement_index, favorites, recents, has_recent, &search, &settings_focus, &apps_focus, apps, &status,
         &capabilities, &quick_values, support_export_result, update_confirm_result, ra_form_result,
         quick_settings, ra_form_open, &ra_form, ra_sign_out_open, &ra_sign_out_dialog,
         update_confirm_open, &update_confirm_dialog, &quick_settings_focus, game_actions_open,
         &game_actions_focus,
         favorite_index(favorites, favorite_count, action_game.bloom_game_id) >= 0,
         action_game_recent, recent_remove_confirm_open, &recent_remove_dialog, safe_mode,
         &safe_mode_focus, rollback_result, rollback_confirm_open, &rollback_dialog, reset_result,
         reset_confirm_open, &reset_dialog, first_run_open, first_run_result, game_count);
    int running = 1;
    int exit_code = 0;
    while (running) {
        SDL_Event event;
        if (!SDL_WaitEvent(&event))
            continue;
        if (event.type == SDL_QUIT) {
            running = 0;
            continue;
        }
        if (event.type == SDL_KEYUP) {
            BloomUiInput released = bloom_ui_input_from_sdl_key(event.key.keysym.sym);
            if (released == BLOOM_UI_INPUT_UP || released == BLOOM_UI_INPUT_DOWN) {
                bloom_shell_games_release(&games_browser);
                settings_held_repeats = 0;
            }
            continue;
        }
        if (event.type != SDL_KEYDOWN)
            continue;
        BloomUiAction action = bloom_ui_normalize_input(bloom_ui_input_from_sdl_key(event.key.keysym.sym));
        BloomShellSettingsRow selected_settings = {0};
        int selected_settings_valid =
            destination == BLOOM_UI_DESTINATION_SETTINGS &&
            bloom_shell_settings_row(&capabilities, settings_focus.selected,
                                     &selected_settings) == 0;
        if (reset_confirm_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_dialog_step(&reset_dialog, -1);
        else if (reset_confirm_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_dialog_step(&reset_dialog, 1);
        else if (reset_confirm_open && action == BLOOM_UI_ACTION_BACK)
            reset_confirm_open = 0;
        else if (reset_confirm_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (reset_dialog.selected == 1)
                reset_result = 2;
            reset_confirm_open = 0;
        }
        else if (reset_confirm_open)
            continue;
        else if (rollback_confirm_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_dialog_step(&rollback_dialog, -1);
        else if (rollback_confirm_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_dialog_step(&rollback_dialog, 1);
        else if (rollback_confirm_open && action == BLOOM_UI_ACTION_BACK)
            rollback_confirm_open = 0;
        else if (rollback_confirm_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (rollback_dialog.selected == 1)
                rollback_result = 2;
            rollback_confirm_open = 0;
        }
        else if (rollback_confirm_open)
            continue;
        else if (recent_remove_confirm_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_dialog_step(&recent_remove_dialog, -1);
        else if (recent_remove_confirm_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_dialog_step(&recent_remove_dialog, 1);
        else if (recent_remove_confirm_open && action == BLOOM_UI_ACTION_BACK)
            recent_remove_confirm_open = 0;
        else if (recent_remove_confirm_open && action == BLOOM_UI_ACTION_CONFIRM) {
            int recent_index = -1;
            for (size_t index = 0; index < recent_count; ++index)
                if (strcmp(recents[index].bloom_game_id, action_game.bloom_game_id) == 0)
                    recent_index = (int)index;
            if (recent_remove_dialog.selected == 1 && recent_index >= 0 &&
                gameswitcher_library_remove_recent(DATABASE_PATH, action_game.bloom_game_id) == 0) {
                size_t removed = (size_t)recent_index;
                if (removed + 1 < recent_count)
                    memmove(&recents[removed], &recents[removed + 1],
                            (recent_count - removed - 1) * sizeof(recents[0]));
                memset(&recents[recent_count - 1], 0, sizeof(recents[0]));
                recent_count--;
                bloom_ui_focus_set_count(&recent_focus, recent_count, layout.visible_rows);
                has_recent = recent_count > 0;
                root.has_continue = has_recent;
                if (!has_recent)
                    root.continue_focused = 0;
                game_actions_open = 0;
                bloom_shell_search_clear(&search);
            }
            recent_remove_confirm_open = 0;
        }
        else if (recent_remove_confirm_open)
            continue;
        else if (game_actions_open && action == BLOOM_UI_ACTION_FOCUS_UP)
            bloom_ui_focus_step(&game_actions_focus, -1, 3);
        else if (game_actions_open && action == BLOOM_UI_ACTION_FOCUS_DOWN)
            bloom_ui_focus_step(&game_actions_focus, 1, 3);
        else if (game_actions_open && action == BLOOM_UI_ACTION_BACK)
            game_actions_open = 0;
        else if (game_actions_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (game_actions_focus.selected == 0) {
                if (stage_game(&action_game) == 0) {
                    exit_code = LAUNCH_READY_EXIT;
                    running = 0;
                }
            }
            else if (game_actions_focus.selected == 1) {
                favorite_toggle(&action_game, favorites, &favorite_count, &favorites_focus,
                                layout.visible_rows);
                game_actions_open = 0;
                bloom_shell_search_clear(&search);
            }
            else if (game_actions_focus.selected == 2 && action_game_recent &&
                     bloom_ui_dialog_init(&recent_remove_dialog, 2, 0, 1) == 0)
                recent_remove_confirm_open = 1;
        }
        else if (game_actions_open)
            continue;
        else if (update_confirm_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_dialog_step(&update_confirm_dialog, -1);
        else if (update_confirm_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_dialog_step(&update_confirm_dialog, 1);
        else if (update_confirm_open && action == BLOOM_UI_ACTION_BACK)
            update_confirm_open = 0;
        else if (update_confirm_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (update_confirm_dialog.selected == 1) {
                update_confirm_result =
                    bloom_shell_update_confirm(BLOOMCTL_BINARY) == 0 ? 1 : -1;
                bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
            }
            update_confirm_open = 0;
        }
        else if (update_confirm_open)
            continue;
        else if (ra_sign_out_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_dialog_step(&ra_sign_out_dialog, -1);
        else if (ra_sign_out_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_dialog_step(&ra_sign_out_dialog, 1);
        else if (ra_sign_out_open && action == BLOOM_UI_ACTION_BACK)
            ra_sign_out_open = 0;
        else if (ra_sign_out_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (ra_sign_out_dialog.selected == 1) {
                ra_form_result = bloom_shell_ra_sign_out(BLOOM_RA_BINARY) == 0 ? 2 : -2;
                bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
            }
            ra_sign_out_open = 0;
        }
        else if (ra_sign_out_open)
            continue;
        else if (ra_form_open && action == BLOOM_UI_ACTION_FOCUS_UP)
            bloom_shell_ra_form_move(&ra_form, 0, -1);
        else if (ra_form_open && action == BLOOM_UI_ACTION_FOCUS_DOWN)
            bloom_shell_ra_form_move(&ra_form, 0, 1);
        else if (ra_form_open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_shell_ra_form_move(&ra_form, -1, 0);
        else if (ra_form_open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_shell_ra_form_move(&ra_form, 1, 0);
        else if (ra_form_open && action == BLOOM_UI_ACTION_CONFIRM)
            bloom_shell_ra_form_append(&ra_form);
        else if (ra_form_open && action == BLOOM_UI_ACTION_CONTEXT)
            bloom_shell_ra_form_cycle_mode(&ra_form);
        else if (ra_form_open && action == BLOOM_UI_ACTION_TOGGLE_FAVORITE)
            bloom_shell_ra_form_toggle_field(&ra_form);
        else if (ra_form_open && action == BLOOM_UI_ACTION_BACK) {
            size_t length = ra_form.field == BLOOM_SHELL_RA_FIELD_USERNAME
                                ? strlen(ra_form.username)
                                : strlen(ra_form.token);
            if (length > 0)
                bloom_shell_ra_form_backspace(&ra_form);
            else {
                bloom_shell_ra_form_clear(&ra_form);
                ra_form_open = 0;
            }
        }
        else if (ra_form_open && action == BLOOM_UI_ACTION_QUICK_SETTINGS) {
            ra_form_result = bloom_shell_ra_form_submit(BLOOM_RA_BINARY, &ra_form) == 0 ? 1 : -1;
            if (ra_form_result > 0) {
                bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
                bloom_shell_ra_form_clear(&ra_form);
                ra_form_open = 0;
            }
        }
        else if (ra_form_open)
            continue;
        else if (search.open && action == BLOOM_UI_ACTION_GAME_SWITCHER) {
            char error[256] = {0};
            if (bloom_shell_stage_executable(GAME_SWITCHER_BINARY, COMMAND_PATH, error,
                                             sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (search.open && action == BLOOM_UI_ACTION_QUICK_SETTINGS) {
            search.open = 0;
            quick_settings = 1;
        }
        else if (search.open && action == BLOOM_UI_ACTION_FOCUS_UP)
            bloom_ui_keyboard_move(&search.keyboard, 0, -1);
        else if (search.open && action == BLOOM_UI_ACTION_FOCUS_DOWN)
            bloom_ui_keyboard_move(&search.keyboard, 0, 1);
        else if (search.open && action == BLOOM_UI_ACTION_FOCUS_LEFT)
            bloom_ui_keyboard_move(&search.keyboard, -1, 0);
        else if (search.open && action == BLOOM_UI_ACTION_FOCUS_RIGHT)
            bloom_ui_keyboard_move(&search.keyboard, 1, 0);
        else if (search.open && (action == BLOOM_UI_ACTION_CONFIRM ||
                                 action == BLOOM_UI_ACTION_TOGGLE_FAVORITE)) {
            size_t source_count = 0;
            const BloomLibraryGame *source = game_source(
                destination, &games_browser, games, favorites, favorite_count, recents,
                recent_count, &source_count);
            if (action == BLOOM_UI_ACTION_CONFIRM)
                bloom_shell_search_append(&search, source, source_count, layout.visible_rows);
            else
                bloom_shell_search_backspace(&search, source, source_count, layout.visible_rows);
        }
        else if (search.open && action == BLOOM_UI_ACTION_CONTEXT)
            bloom_ui_keyboard_cycle_mode(&search.keyboard);
        else if (search.open && action == BLOOM_UI_ACTION_SEARCH)
            search.open = 0;
        else if (search.open && action == BLOOM_UI_ACTION_BACK)
            bloom_shell_search_clear(&search);
        else if (search.open)
            continue;
        else if (action == BLOOM_UI_ACTION_GAME_SWITCHER) {
            quick_settings = 0;
            char error[256] = {0};
            if (bloom_shell_stage_executable(GAME_SWITCHER_BINARY, COMMAND_PATH, error,
                                             sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_QUICK_SETTINGS) {
            quick_settings = !quick_settings;
        }
        else if (action == BLOOM_UI_ACTION_BACK && quick_settings) {
            quick_settings = 0;
        }
        else if (quick_settings && action == BLOOM_UI_ACTION_FOCUS_UP) {
            bloom_ui_focus_step(&quick_settings_focus, -1, layout.visible_rows);
        }
        else if (quick_settings && action == BLOOM_UI_ACTION_FOCUS_DOWN) {
            bloom_ui_focus_step(&quick_settings_focus, 1, layout.visible_rows);
        }
        else if (quick_settings && (action == BLOOM_UI_ACTION_FOCUS_LEFT ||
                                    action == BLOOM_UI_ACTION_FOCUS_RIGHT)) {
            bloom_shell_quick_settings_adjust(
                &capabilities, &quick_values, quick_settings_focus.selected,
                action == BLOOM_UI_ACTION_FOCUS_RIGHT ? 1 : -1, BLOOM_CONTROLS_BINARY,
                BLOOM_NETWORK_BINARY);
        }
        else if (quick_settings) {
            continue;
        }
        else if (first_run_open && action == BLOOM_UI_ACTION_CONFIRM) {
            if (!first_run.ready)
                bloom_shell_first_run_load(BLOOM_SETTINGS_BINARY, &first_run);
            first_run_result =
                first_run.ready &&
                        bloom_shell_first_run_finish(BLOOM_SETTINGS_BINARY, &first_run,
                                                     &quick_values) == 0
                    ? 1
                    : -1;
            if (first_run_result > 0)
                first_run_open = 0;
        }
        else if (first_run_open)
            continue;
        else if (action == BLOOM_UI_ACTION_BACK) {
            bloom_shell_search_clear(&search);
            if (destination != BLOOM_UI_DESTINATION_ROOT)
                destination = BLOOM_UI_DESTINATION_ROOT;
        }
        else if (safe_mode && destination == BLOOM_UI_DESTINATION_ROOT &&
                 action == BLOOM_UI_ACTION_FOCUS_UP)
            bloom_ui_focus_step(&safe_mode_focus, -1, layout.visible_rows);
        else if (safe_mode && destination == BLOOM_UI_DESTINATION_ROOT &&
                 action == BLOOM_UI_ACTION_FOCUS_DOWN)
            bloom_ui_focus_step(&safe_mode_focus, 1, layout.visible_rows);
        else if (safe_mode && destination == BLOOM_UI_DESTINATION_ROOT &&
                 action == BLOOM_UI_ACTION_CONFIRM) {
            if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_GAMES)
                destination = BLOOM_UI_DESTINATION_GAMES;
            else if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_HEALTH)
                bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
            else if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_EXPORT_SUPPORT)
                support_export_result = bloom_shell_support_export(BLOOMCTL_BINARY) == 0 ? 1 : -1;
            else if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_ROLLBACK &&
                     rollback_result <= 0 &&
                     bloom_ui_dialog_init(&rollback_dialog, 2, 0, 1) == 0)
                rollback_confirm_open = 1;
            else if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_RESET_SETTINGS &&
                     reset_result <= 0 && bloom_ui_dialog_init(&reset_dialog, 2, 0, 1) == 0)
                reset_confirm_open = 1;
            else if (safe_mode_focus.selected == BLOOM_SHELL_SAFE_MODE_RESTART_NORMAL) {
                exit_code = RESTART_NORMAL_EXIT;
                running = 0;
            }
        }
        else if (!safe_mode && destination == BLOOM_UI_DESTINATION_ROOT &&
                 action != BLOOM_UI_ACTION_CONFIRM)
            bloom_shell_root_handle(&root, action);
        else if (!safe_mode && action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_ROOT) {
            if (root.continue_focused && has_recent) {
                if (stage_game(&recents[0]) == 0) {
                    exit_code = LAUNCH_READY_EXIT;
                    running = 0;
                }
            }
            else
                destination = bloom_shell_root_open(&root);
        }
        else if (action == BLOOM_UI_ACTION_FOCUS_LEFT &&
                 destination == BLOOM_UI_DESTINATION_GAMES) {
            bloom_shell_search_clear(&search);
            bloom_shell_games_switch(&games_browser, -1);
        }
        else if (action == BLOOM_UI_ACTION_FOCUS_RIGHT &&
                 destination == BLOOM_UI_DESTINATION_GAMES) {
            bloom_shell_search_clear(&search);
            bloom_shell_games_switch(&games_browser, 1);
        }
        else if ((action == BLOOM_UI_ACTION_FOCUS_UP || action == BLOOM_UI_ACTION_FOCUS_DOWN) &&
                 search.active)
            bloom_ui_focus_step(&search.focus, action == BLOOM_UI_ACTION_FOCUS_UP ? -1 : 1,
                                layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP && destination == BLOOM_UI_DESTINATION_GAMES)
            bloom_shell_games_step(&games_browser, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN && destination == BLOOM_UI_DESTINATION_GAMES)
            bloom_shell_games_step(&games_browser, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP &&
                 destination == BLOOM_UI_DESTINATION_FAVORITES)
            bloom_ui_focus_step(&favorites_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN &&
                 destination == BLOOM_UI_DESTINATION_FAVORITES)
            bloom_ui_focus_step(&favorites_focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP &&
                 destination == BLOOM_UI_DESTINATION_RECENT)
            bloom_ui_focus_step(&recent_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN &&
                 destination == BLOOM_UI_DESTINATION_RECENT)
            bloom_ui_focus_step(&recent_focus, 1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_SEARCH &&
                 (destination == BLOOM_UI_DESTINATION_GAMES ||
                  destination == BLOOM_UI_DESTINATION_FAVORITES ||
                  destination == BLOOM_UI_DESTINATION_RECENT)) {
            size_t source_count = 0;
            const BloomLibraryGame *source = game_source(
                destination, &games_browser, games, favorites, favorite_count, recents,
                recent_count, &source_count);
            search.open = 1;
            bloom_shell_search_rebuild(&search, source, source_count, layout.visible_rows);
        }
        else if (action == BLOOM_UI_ACTION_CONTEXT &&
                 (destination == BLOOM_UI_DESTINATION_GAMES ||
                  destination == BLOOM_UI_DESTINATION_FAVORITES ||
                  destination == BLOOM_UI_DESTINATION_RECENT)) {
            const BloomLibraryGame *game = selected_game(
                destination, &games_browser, games, favorites, &favorites_focus, recents,
                &recent_focus, &search);
            if (game != NULL) {
                action_game = *game;
                action_game_recent = destination == BLOOM_UI_DESTINATION_RECENT;
                bloom_ui_focus_init(&game_actions_focus, action_game_recent ? 3 : 2);
                game_actions_open = 1;
            }
        }
        else if (action == BLOOM_UI_ACTION_TOGGLE_FAVORITE &&
                 (destination == BLOOM_UI_DESTINATION_GAMES ||
                  destination == BLOOM_UI_DESTINATION_FAVORITES ||
                  destination == BLOOM_UI_DESTINATION_RECENT)) {
            const BloomLibraryGame *game = selected_game(
                destination, &games_browser, games, favorites, &favorites_focus, recents,
                &recent_focus, &search);
            if (game != NULL) {
                BloomLibraryGame selected_copy = *game;
                favorite_toggle(&selected_copy, favorites, &favorite_count, &favorites_focus,
                                layout.visible_rows);
                if (search.active) {
                    size_t source_count = 0;
                    const BloomLibraryGame *source = game_source(
                        destination, &games_browser, games, favorites, favorite_count, recents,
                        recent_count, &source_count);
                    bloom_shell_search_rebuild(&search, source, source_count, layout.visible_rows);
                }
            }
        }
        else if (action == BLOOM_UI_ACTION_FOCUS_UP &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS)
            settings_focus_step(&settings_focus, &capabilities, -1, layout.visible_rows,
                                &settings_held_repeats);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS)
            settings_focus_step(&settings_focus, &capabilities, 1, layout.visible_rows,
                                &settings_held_repeats);
        else if (action == BLOOM_UI_ACTION_FOCUS_UP && destination == BLOOM_UI_DESTINATION_APPS)
            bloom_ui_focus_step(&apps_focus, -1, layout.visible_rows);
        else if (action == BLOOM_UI_ACTION_FOCUS_DOWN && destination == BLOOM_UI_DESTINATION_APPS)
            bloom_ui_focus_step(&apps_focus, 1, layout.visible_rows);
        else if (selected_settings_valid && destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 (action == BLOOM_UI_ACTION_FOCUS_LEFT || action == BLOOM_UI_ACTION_FOCUS_RIGHT)) {
            size_t quick_row = (size_t)-1;
            if (selected_settings.id == BLOOM_SHELL_SETTINGS_BRIGHTNESS)
                quick_row = 0;
            else if (selected_settings.id == BLOOM_SHELL_SETTINGS_VOLUME)
                quick_row = 1;
            else if (selected_settings.id == BLOOM_SHELL_SETTINGS_WIFI)
                quick_row = 2;
            if (quick_row != (size_t)-1)
                bloom_shell_quick_settings_adjust(
                    &capabilities, &quick_values, quick_row,
                    action == BLOOM_UI_ACTION_FOCUS_RIGHT ? 1 : -1, BLOOM_CONTROLS_BINARY,
                    BLOOM_NETWORK_BINARY);
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 selected_settings_valid && selected_settings.id == BLOOM_SHELL_SETTINGS_MUTE) {
            bloom_shell_mute_toggle(&quick_values, BLOOM_CONTROLS_BINARY);
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 selected_settings_valid && selected_settings.id == BLOOM_SHELL_SETTINGS_WIFI) {
            bloom_shell_quick_settings_adjust(&capabilities, &quick_values, 2,
                                              quick_values.wifi_enabled ? -1 : 1,
                                              BLOOM_CONTROLS_BINARY, BLOOM_NETWORK_BINARY);
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS && selected_settings_valid &&
                 selected_settings.id == BLOOM_SHELL_SETTINGS_HEALTH) {
            support_export_result = bloom_shell_support_export(BLOOMCTL_BINARY) == 0 ? 1 : -1;
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 selected_settings_valid && selected_settings.id == BLOOM_SHELL_SETTINGS_UPDATE &&
                 update_confirm_result == 0 && status.ready && status.update_healthy &&
                 strcmp(status.update_phase, "testing") == 0 &&
                 bloom_ui_dialog_init(&update_confirm_dialog, 2, 0, 1) == 0) {
            update_confirm_result = 0;
            update_confirm_open = 1;
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 selected_settings_valid &&
                 selected_settings.id == BLOOM_SHELL_SETTINGS_RA_ACCOUNT && !status.ra_enabled) {
            bloom_shell_ra_form_init(&ra_form);
            ra_form_result = 0;
            ra_form_open = 1;
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_SETTINGS &&
                 selected_settings_valid &&
                 selected_settings.id == BLOOM_SHELL_SETTINGS_RA_ACCOUNT && status.ra_enabled &&
                 bloom_ui_dialog_init(&ra_sign_out_dialog, 2, 0, 1) == 0) {
            ra_form_result = 0;
            ra_sign_out_open = 1;
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM && search.active &&
                 search.focus.item_count > 0) {
            if (stage_game(search.results[search.focus.selected]) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_GAMES &&
                 games_browser.system_count > 0) {
            const BloomShellGamesSystem *system = bloom_shell_games_current(&games_browser);
            if (stage_game_with_core(&games[system->game_offset + system->focus.selected],
                                     system->core) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM && destination == BLOOM_UI_DESTINATION_APPS &&
                 apps_focus.item_count > 0) {
            char error[256] = {0};
            if (bloom_shell_stage_app(&apps[apps_focus.selected], developer_mode, "/mnt/SDCARD",
                                      COMMAND_PATH, error, sizeof(error)) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_FAVORITES &&
                 favorites_focus.item_count > 0) {
            if (stage_game(&favorites[favorites_focus.selected]) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        else if (action == BLOOM_UI_ACTION_CONFIRM &&
                 destination == BLOOM_UI_DESTINATION_RECENT && recent_focus.item_count > 0) {
            if (stage_game(&recents[recent_focus.selected]) == 0) {
                exit_code = LAUNCH_READY_EXIT;
                running = 0;
            }
        }
        if (running) {
            draw(screen, video, &layout, font, compact_font, destination, &root, &games_browser, &favorites_focus,
                 &recent_focus, games, &achievement_index, favorites, recents, has_recent, &search, &settings_focus, &apps_focus,
                 apps, &status, &capabilities, &quick_values, support_export_result,
                 update_confirm_result, ra_form_result, quick_settings,
                 ra_form_open, &ra_form,
                 ra_sign_out_open, &ra_sign_out_dialog, update_confirm_open,
                 &update_confirm_dialog, &quick_settings_focus, game_actions_open,
                 &game_actions_focus,
                 favorite_index(favorites, favorite_count, action_game.bloom_game_id) >= 0,
                 action_game_recent, recent_remove_confirm_open, &recent_remove_dialog, safe_mode,
                 &safe_mode_focus, rollback_result, rollback_confirm_open, &rollback_dialog,
                 reset_result, reset_confirm_open, &reset_dialog, first_run_open,
                 first_run_result, game_count);
            if (rollback_result == 2) {
                rollback_result = bloom_shell_update_rollback(BLOOMCTL_BINARY) == 0 ? 1 : -1;
                bloom_shell_status_load(BLOOM_STATUS_BINARY, &status);
                draw(screen, video, &layout, font, compact_font, destination, &root,
                     &games_browser, &favorites_focus, &recent_focus, games, &achievement_index, favorites, recents,
                     has_recent, &search, &settings_focus, &apps_focus, apps, &status,
                     &capabilities, &quick_values, support_export_result, update_confirm_result,
                     ra_form_result, quick_settings, ra_form_open, &ra_form, ra_sign_out_open,
                     &ra_sign_out_dialog, update_confirm_open, &update_confirm_dialog,
                     &quick_settings_focus, game_actions_open, &game_actions_focus,
                     favorite_index(favorites, favorite_count, action_game.bloom_game_id) >= 0,
                     action_game_recent, recent_remove_confirm_open, &recent_remove_dialog,
                     safe_mode, &safe_mode_focus, rollback_result, rollback_confirm_open,
                     &rollback_dialog, reset_result, reset_confirm_open, &reset_dialog,
                     first_run_open, first_run_result, game_count);
            }
            if (reset_result == 2) {
                reset_result = bloom_shell_settings_reset(BLOOMCTL_BINARY) == 0 ? 1 : -1;
                draw(screen, video, &layout, font, compact_font, destination, &root,
                     &games_browser, &favorites_focus, &recent_focus, games, &achievement_index, favorites, recents,
                     has_recent, &search, &settings_focus, &apps_focus, apps, &status,
                     &capabilities, &quick_values, support_export_result, update_confirm_result,
                     ra_form_result, quick_settings, ra_form_open, &ra_form, ra_sign_out_open,
                     &ra_sign_out_dialog, update_confirm_open, &update_confirm_dialog,
                     &quick_settings_focus, game_actions_open, &game_actions_focus,
                     favorite_index(favorites, favorite_count, action_game.bloom_game_id) >= 0,
                     action_game_recent, recent_remove_confirm_open, &recent_remove_dialog,
                     safe_mode, &safe_mode_focus, rollback_result, rollback_confirm_open,
                     &rollback_dialog, reset_result, reset_confirm_open, &reset_dialog,
                     first_run_open, first_run_result, game_count);
            }
        }
    }
    TTF_CloseFont(font);
    TTF_CloseFont(compact_font);
    bloom_shell_ra_form_clear(&ra_form);
    bloom_shell_search_destroy(&search);
    SDL_FreeSurface(screen);
    TTF_Quit();
    SDL_Quit();
    bloom_shell_achievements_destroy(&achievement_index);
    free(games);
    return exit_code;
}
