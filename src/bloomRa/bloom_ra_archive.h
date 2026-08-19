#ifndef BLOOM_RA_ARCHIVE_H
#define BLOOM_RA_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

int bloom_ra_archive_read_rom(const char *system_id, const char *archive_path, char *entry_name,
                              size_t entry_name_size, uint8_t **content, size_t *content_size,
                              char *error, size_t error_size);

#endif
