#ifndef TOPGEAR2_SHA256_H
#define TOPGEAR2_SHA256_H

#include "topgear2.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOPGEAR2_USA_ROM_SHA256 "76b2702c4be8b668c1017f2817c280283c275eaa41535bf6ffa2b8d2220b68c6"

void tg2_sha256_compute(const uint8_t *data, size_t len, uint8_t out[32]);
void tg2_sha256_hex(const uint8_t digest[32], char out[65]);
Tg2RomStatus tg2_verify_rom_buffer(const uint8_t *data, size_t size, char *out_sha256_hex);

#ifdef __cplusplus
}
#endif

#endif /* TOPGEAR2_SHA256_H */
