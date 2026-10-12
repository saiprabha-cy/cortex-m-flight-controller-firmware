#include "image_verify.h"
#include "crc.h"
#include "sha256.h"
#include "../third_party/monocypher/monocypher-ed25519.h"
#include <string.h>

const char *image_status_str(img_status_t s)
{
    switch (s) {
    case IMG_OK:              return "OK";
    case IMG_ERR_MAGIC:       return "BAD_MAGIC";
    case IMG_ERR_HDR_CRC:     return "BAD_HEADER_CRC";
    case IMG_ERR_SLOT:        return "WRONG_SLOT";
    case IMG_ERR_SIZE:        return "BAD_SIZE";
    case IMG_ERR_LOADADDR:    return "BAD_LOAD_ADDR";
    case IMG_ERR_PAYLOAD_CRC: return "BAD_PAYLOAD_CRC";
    case IMG_ERR_SHA256:      return "BAD_SHA256";
    case IMG_ERR_SIGNATURE:   return "BAD_SIGNATURE";
    case IMG_ERR_ROLLBACK:    return "ROLLBACK_BLOCKED";
    case IMG_ERR_VECTOR:      return "BAD_VECTOR_TABLE";
    default:                  return "?";
    }
}

img_status_t image_verify(const uint8_t *image, uint32_t slot_addr, uint32_t slot_id,
                          uint32_t slot_size, uint32_t min_security_counter,
                          const uint8_t pubkey[32], image_header_t *hdr_out)
{
    image_header_t h;
    memcpy(&h, image, sizeof h);                         /* copy: flash may be unaligned-unsafe for packed access */

    if (h.magic != IMG_MAGIC) { return IMG_ERR_MAGIC; }
    if (crc32_calc((const uint8_t *)&h, IMG_HDR_CRC_OFFSET) != h.header_crc32) { return IMG_ERR_HDR_CRC; }
    if ((h.slot_id != slot_id) || (h.header_version != IMG_HEADER_VERSION)) { return IMG_ERR_SLOT; }
    if ((h.image_size < 64u) || (h.image_size > (slot_size - IMG_HEADER_SIZE))) { return IMG_ERR_SIZE; }
    if (h.load_addr != (slot_addr + IMG_HEADER_SIZE)) { return IMG_ERR_LOADADDR; }

    const uint8_t *payload = image + IMG_HEADER_SIZE;
    if (crc32_calc(payload, h.image_size) != h.payload_crc32) { return IMG_ERR_PAYLOAD_CRC; }

    uint8_t digest[32];
    sha256_calc(payload, h.image_size, digest);
    if (memcmp(digest, h.payload_sha256, 32u) != 0) { return IMG_ERR_SHA256; }

    if (crypto_ed25519_check(h.signature, pubkey, (const uint8_t *)&h, IMG_SIG_OFFSET) != 0) {
        return IMG_ERR_SIGNATURE;
    }
    if (h.security_counter < min_security_counter) { return IMG_ERR_ROLLBACK; }

    /* Vector table sanity: initial SP must point into SRAM, reset vector must point into this slot (Thumb bit set). */
    uint32_t sp, rv;
    memcpy(&sp, payload, 4u); memcpy(&rv, payload + 4u, 4u);
    if ((sp < SRAM_BASE_ADDR) || (sp > (SRAM_BASE_ADDR + SRAM_SIZE)) || ((rv & 1u) == 0u) ||
        ((rv & ~1u) < h.load_addr) || ((rv & ~1u) >= (h.load_addr + h.image_size))) {
        return IMG_ERR_VECTOR;
    }
    if (hdr_out != NULL) { *hdr_out = h; }
    return IMG_OK;
}
