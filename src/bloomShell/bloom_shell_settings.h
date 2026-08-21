#ifndef BLOOM_SHELL_SETTINGS_H
#define BLOOM_SHELL_SETTINGS_H

#include <stddef.h>

typedef struct {
    int wifi;
    int flip;
    int developer_mode;
} BloomShellCapabilities;

int bloom_shell_capabilities_from_model(int model, int developer_mode,
                                        BloomShellCapabilities *capabilities);
size_t bloom_shell_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_settings_label(const BloomShellCapabilities *capabilities, size_t row);
size_t bloom_shell_quick_settings_count(const BloomShellCapabilities *capabilities);
const char *bloom_shell_quick_settings_label(const BloomShellCapabilities *capabilities, size_t row);

#endif
