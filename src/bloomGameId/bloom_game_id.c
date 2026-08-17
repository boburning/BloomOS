#include "bloom_game_id.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_ROOT "/mnt/SDCARD/Roms/"

struct sha256 {
    uint32_t state[8];
    uint64_t bits;
    unsigned char block[64];
    size_t used;
};

static const uint32_t constants[64] = {
    0x428a2f98,
    0x71374491,
    0xb5c0fbcf,
    0xe9b5dba5,
    0x3956c25b,
    0x59f111f1,
    0x923f82a4,
    0xab1c5ed5,
    0xd807aa98,
    0x12835b01,
    0x243185be,
    0x550c7dc3,
    0x72be5d74,
    0x80deb1fe,
    0x9bdc06a7,
    0xc19bf174,
    0xe49b69c1,
    0xefbe4786,
    0x0fc19dc6,
    0x240ca1cc,
    0x2de92c6f,
    0x4a7484aa,
    0x5cb0a9dc,
    0x76f988da,
    0x983e5152,
    0xa831c66d,
    0xb00327c8,
    0xbf597fc7,
    0xc6e00bf3,
    0xd5a79147,
    0x06ca6351,
    0x14292967,
    0x27b70a85,
    0x2e1b2138,
    0x4d2c6dfc,
    0x53380d13,
    0x650a7354,
    0x766a0abb,
    0x81c2c92e,
    0x92722c85,
    0xa2bfe8a1,
    0xa81a664b,
    0xc24b8b70,
    0xc76c51a3,
    0xd192e819,
    0xd6990624,
    0xf40e3585,
    0x106aa070,
    0x19a4c116,
    0x1e376c08,
    0x2748774c,
    0x34b0bcb5,
    0x391c0cb3,
    0x4ed8aa4a,
    0x5b9cca4f,
    0x682e6ff3,
    0x748f82ee,
    0x78a5636f,
    0x84c87814,
    0x8cc70208,
    0x90befffa,
    0xa4506ceb,
    0xbef9a3f7,
    0xc67178f2,
};

static uint32_t rotate(uint32_t value, unsigned int bits) { return (value >> bits) | (value << (32 - bits)); }

static void transform(struct sha256 *hash, const unsigned char *block)
{
    uint32_t words[64];
    for (size_t i = 0; i < 16; i++)
        words[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    for (size_t i = 16; i < 64; i++) {
        uint32_t s0 = rotate(words[i - 15], 7) ^ rotate(words[i - 15], 18) ^ (words[i - 15] >> 3);
        uint32_t s1 = rotate(words[i - 2], 17) ^ rotate(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    uint32_t a = hash->state[0], b = hash->state[1], c = hash->state[2], d = hash->state[3];
    uint32_t e = hash->state[4], f = hash->state[5], g = hash->state[6], h = hash->state[7];
    for (size_t i = 0; i < 64; i++) {
        uint32_t sum1 = rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choice + constants[i] + words[i];
        uint32_t sum0 = rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    hash->state[0] += a;
    hash->state[1] += b;
    hash->state[2] += c;
    hash->state[3] += d;
    hash->state[4] += e;
    hash->state[5] += f;
    hash->state[6] += g;
    hash->state[7] += h;
}

static void sha256_init(struct sha256 *hash)
{
    static const uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(hash->state, initial, sizeof(initial));
    hash->bits = 0;
    hash->used = 0;
}

static void sha256_update(struct sha256 *hash, const void *input, size_t length)
{
    const unsigned char *bytes = input;
    hash->bits += (uint64_t)length * 8;
    while (length > 0) {
        size_t available = sizeof(hash->block) - hash->used;
        size_t take = length < available ? length : available;
        memcpy(hash->block + hash->used, bytes, take);
        hash->used += take;
        bytes += take;
        length -= take;
        if (hash->used == sizeof(hash->block)) {
            transform(hash, hash->block);
            hash->used = 0;
        }
    }
}

static void sha256_final(struct sha256 *hash, unsigned char digest[32])
{
    uint64_t bits = hash->bits;
    hash->block[hash->used++] = 0x80;
    if (hash->used > 56) {
        memset(hash->block + hash->used, 0, 64 - hash->used);
        transform(hash, hash->block);
        hash->used = 0;
    }
    memset(hash->block + hash->used, 0, 56 - hash->used);
    for (size_t i = 0; i < 8; i++)
        hash->block[63 - i] = (unsigned char)(bits >> (i * 8));
    transform(hash, hash->block);
    for (size_t i = 0; i < 8; i++) {
        digest[i * 4] = (unsigned char)(hash->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(hash->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(hash->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)hash->state[i];
    }
}

static void set_error(char *error, size_t size, const char *format, ...)
{
    if (error == NULL || size == 0)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, size, format, args);
    va_end(args);
}

static bool valid_system(const char *system)
{
    if (system == NULL || system[0] == '\0')
        return false;
    for (const unsigned char *p = (const unsigned char *)system; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
            return false;
    return true;
}

static int normalize_path(const char *rom_path, char *relative, size_t size, char *error, size_t error_size)
{
    if (rom_path == NULL || strncmp(rom_path, ROM_ROOT, strlen(ROM_ROOT)) != 0) {
        set_error(error, error_size, "ROM path is outside the ROM root");
        return -1;
    }
    const unsigned char *input = (const unsigned char *)rom_path + strlen(ROM_ROOT);
    size_t used = 0;
    bool need_separator = false;
    while (*input) {
        while (*input == '/')
            input++;
        const unsigned char *segment = input;
        while (*input && *input != '/') {
            if (*input < 0x20 || *input == 0x7f || *input == '\\') {
                set_error(error, error_size, "ROM path contains an unsafe character");
                return -1;
            }
            input++;
        }
        size_t length = (size_t)(input - segment);
        if (length == 0)
            break;
        if (length == 1 && segment[0] == '.')
            continue;
        if (length == 2 && segment[0] == '.' && segment[1] == '.') {
            set_error(error, error_size, "ROM path traversal is not allowed");
            return -1;
        }
        if (used + (need_separator ? 1 : 0) + length >= size) {
            set_error(error, error_size, "normalized ROM path is too long");
            return -1;
        }
        if (need_separator)
            relative[used++] = '/';
        memcpy(relative + used, segment, length);
        used += length;
        need_separator = true;
    }
    if (used == 0 || rom_path[strlen(rom_path) - 1] == '/') {
        set_error(error, error_size, "ROM path does not identify a file");
        return -1;
    }
    relative[used] = '\0';
    return 0;
}

int bloom_game_id_create(const char *system_id, const char *rom_path, char *game_id, size_t game_id_size,
                         char *relative_path, size_t relative_path_size, char *error, size_t error_size)
{
    if (!valid_system(system_id) || game_id == NULL || game_id_size < BLOOM_GAME_ID_LENGTH + 1 ||
        relative_path == NULL || relative_path_size == 0) {
        set_error(error, error_size, "GameID arguments are invalid");
        return -1;
    }
    if (normalize_path(rom_path, relative_path, relative_path_size, error, error_size) != 0)
        return -1;
    struct sha256 hash;
    unsigned char digest[32];
    const char version[] = "bloom-game-v1";
    const char separator = '\0';
    sha256_init(&hash);
    sha256_update(&hash, version, sizeof(version) - 1);
    sha256_update(&hash, &separator, 1);
    sha256_update(&hash, system_id, strlen(system_id));
    sha256_update(&hash, &separator, 1);
    sha256_update(&hash, relative_path, strlen(relative_path));
    sha256_final(&hash, digest);
    strcpy(game_id, "bloom-game-v1:");
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(digest); i++) {
        game_id[14 + i * 2] = hex[digest[i] >> 4];
        game_id[15 + i * 2] = hex[digest[i] & 0x0f];
    }
    game_id[BLOOM_GAME_ID_LENGTH] = '\0';
    return 0;
}

int bloom_game_id_valid(const char *game_id)
{
    if (game_id == NULL || strlen(game_id) != BLOOM_GAME_ID_LENGTH || strncmp(game_id, "bloom-game-v1:", 14) != 0)
        return 0;
    for (const char *p = game_id + 14; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
            return 0;
    return 1;
}
