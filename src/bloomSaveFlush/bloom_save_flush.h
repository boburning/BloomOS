#ifndef BLOOM_SAVE_FLUSH_H
#define BLOOM_SAVE_FLUSH_H

#include <stddef.h>

struct bloom_save_flush_result {
    char corename[128];
    size_t files_flushed;
    size_t directories_flushed;
};

int bloom_save_flush(const char *sd_root, const char *core, struct bloom_save_flush_result *result, char *error,
                     size_t error_size);

#endif
