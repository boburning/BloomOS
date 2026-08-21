#ifndef BLOOM_SHELL_SETTINGS_H
#define BLOOM_SHELL_SETTINGS_H

#include <stddef.h>

typedef struct {
    int wifi;
    int flip;
    int developer_mode;
} BloomShellCapabilities;

typedef struct {
    int ready;
    int generation;
    int brightness;
    int volume;
    int mute;
    int wifi_enabled;
} BloomShellQuickValues;

int bloom_shell_capabilities_from_model(int model, int developer_mode,
                                        BloomShellCapabilities *capabilities);
size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_settings_label(const BloomShellCapabilities *capabilities, size_t row);
size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_quick_settings_label(const BloomShellCapabilities *capabilities, size_t row);
int bloom_shell_quick_values_parse(const char *json, BloomShellQuickValues *values);
int bloom_shell_quick_values_load(const char *settings_path, BloomShellQuickValues *values);
int bloom_shell_quick_settings_format(const BloomShellCapabilities *capabilities,
                                      const BloomShellQuickValues *values, size_t row,
                                      char *label, size_t label_size);
int bloom_shell_quick_settings_adjust(const BloomShellCapabilities *capabilities,
                                      BloomShellQuickValues *values, size_t row, int direction,
                                      const char *controls_path, const char *network_path);

#endif
