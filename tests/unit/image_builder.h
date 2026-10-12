/* image_builder.h - builds signed test images in a buffer (host tests only). */
#ifndef IMAGE_BUILDER_H
#define IMAGE_BUILDER_H
#include "../../common/image_header.h"
typedef struct { uint8_t sk[64]; uint8_t pk[32]; } test_keys_t;
void test_keys_make(test_keys_t *k, uint8_t seed_byte);
/* Writes header + payload to buf; payload gets a valid vector table. Returns total size. */
uint32_t build_image(uint8_t *buf, const test_keys_t *k, uint32_t slot_id, uint32_t slot_addr,
                     uint32_t payload_size, uint32_t version, uint32_t sec_counter);
void resign(uint8_t *buf, const test_keys_t *k);       /* recompute header CRC and signature (keeps payload hashes) */
void rehash_payload(uint8_t *buf);                     /* recompute payload CRC32 + SHA-256 (not signature) */
void fix_hdr_crc(uint8_t *buf);
#endif
