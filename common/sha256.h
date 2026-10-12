#ifndef SHA256_H
#define SHA256_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t h[8]; uint8_t buf[64]; uint32_t buflen; uint64_t total; } sha256_ctx_t;
void sha256_init(sha256_ctx_t *c);
void sha256_update(sha256_ctx_t *c, const uint8_t *d, size_t n);
void sha256_final(sha256_ctx_t *c, uint8_t out[32]);
void sha256_calc(const uint8_t *d, size_t n, uint8_t out[32]);
#endif
