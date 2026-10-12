#include "image_builder.h"
#include "../../common/crc.h"
#include "../../common/sha256.h"
#include "../../third_party/monocypher/monocypher-ed25519.h"
#include <string.h>

void test_keys_make(test_keys_t *k, uint8_t seed_byte)
{ uint8_t seed[32]; memset(seed, seed_byte, 32); crypto_ed25519_key_pair(k->sk, k->pk, seed); }

void fix_hdr_crc(uint8_t *buf) { image_header_t *h = (image_header_t *)buf; h->header_crc32 = crc32_calc(buf, IMG_HDR_CRC_OFFSET); }
void rehash_payload(uint8_t *buf)
{
    image_header_t *h = (image_header_t *)buf;
    h->payload_crc32 = crc32_calc(buf + IMG_HEADER_SIZE, h->image_size);
    sha256_calc(buf + IMG_HEADER_SIZE, h->image_size, h->payload_sha256);
}
void resign(uint8_t *buf, const test_keys_t *k)
{ image_header_t *h = (image_header_t *)buf; fix_hdr_crc(buf); crypto_ed25519_sign(h->signature, k->sk, buf, IMG_SIG_OFFSET); }

uint32_t build_image(uint8_t *buf, const test_keys_t *k, uint32_t slot_id, uint32_t slot_addr,
                     uint32_t psz, uint32_t version, uint32_t sec)
{
    image_header_t *h = (image_header_t *)buf; uint8_t *pl = buf + IMG_HEADER_SIZE;
    memset(buf, 0, IMG_HEADER_SIZE);
    for (uint32_t i = 0; i < psz; i++) { pl[i] = (uint8_t)(i * 31u + 7u); }
    uint32_t sp = SRAM_BASE_ADDR + SRAM_SIZE, rv = (slot_addr + IMG_HEADER_SIZE + 0x40u) | 1u;
    memcpy(pl, &sp, 4); memcpy(pl + 4, &rv, 4);
    h->magic = IMG_MAGIC; h->header_version = IMG_HEADER_VERSION; h->slot_id = (uint16_t)slot_id;
    h->image_size = psz; h->load_addr = slot_addr + IMG_HEADER_SIZE; h->version = version;
    h->security_counter = sec; h->build_time = 1700000000u;
    rehash_payload(buf); resign(buf, k);
    return IMG_HEADER_SIZE + psz;
}
