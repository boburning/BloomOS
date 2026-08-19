#ifndef BLOOM_RA_ACCOUNT_H
#define BLOOM_RA_ACCOUNT_H

#include <stddef.h>

#define BLOOM_RA_ACCOUNT_SCHEMA 1

typedef struct {
    int schema;
    int enabled;
    int authenticated;
    int offline_casual;
    char username[64];
    char mode[16];
} BloomRaAccountStatus;

int bloom_ra_account_load(const char *settings_path, const char *credentials_path, BloomRaAccountStatus *status,
                          char *error, size_t error_size);
int bloom_ra_account_store(const char *settings_path, const char *credentials_path, const char *username,
                           const char *token, int enabled, const char *mode, int offline_casual, char *error,
                           size_t error_size);
int bloom_ra_account_read_token(const char *credentials_path, char *token, size_t token_size);
int bloom_ra_account_sign_out(const char *settings_path, const char *credentials_path, char *error,
                              size_t error_size);
int bloom_ra_account_update(const char *settings_path, const char *credentials_path, int enabled,
                            const char *mode, int offline_casual, char *error, size_t error_size);

#endif
