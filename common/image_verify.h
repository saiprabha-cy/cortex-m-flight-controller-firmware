#ifndef IMAGE_VERIFY_H
#define IMAGE_VERIFY_H
#include <stdint.h>
#include "image_header.h"

typedef enum {
    IMG_OK = 0, IMG_ERR_MAGIC, IMG_ERR_HDR_CRC, IMG_ERR_SLOT, IMG_ERR_SIZE, IMG_ERR_LOADADDR,
    IMG_ERR_PAYLOAD_CRC, IMG_ERR_SHA256, IMG_ERR_SIGNATURE, IMG_ERR_ROLLBACK, IMG_ERR_VECTOR
} img_status_t;

const char *image_status_str(img_status_t s);

/* Verifies a complete image (header + payload) stored at `image` (memory-mapped flash on target,
 * a buffer in host tests). `slot_addr` is the address the slot has in the device memory map.
 * Order of checks is cheapest-first: magic, header CRC, slot/size/addr, payload CRC32, SHA-256,
 * Ed25519 signature, anti-rollback, vector sanity. */
img_status_t image_verify(const uint8_t *image, uint32_t slot_addr, uint32_t slot_id,
                          uint32_t slot_size, uint32_t min_security_counter,
                          const uint8_t pubkey[32], image_header_t *hdr_out);
#endif
