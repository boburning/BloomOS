#include "bloom_shell_settings.h"

#include <stddef.h>

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
