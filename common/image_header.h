/* image_header.h - 512-byte firmware image header (little endian, packed).
 * Layout is mirrored by tools/fw_packager.py (struct format "<IHHIIIIIII32s"). */
#ifndef IMAGE_HEADER_H
#define IMAGE_HEADER_H
#include <stdint.h>
#include "../config/memory_map.h"

#define IMG_MAGIC            0x31574346u      /* "FCW1" */
#define IMG_HEADER_VERSION   1u
#define IMG_SIG_OFFSET       448u             /* signature covers bytes [0, 448) */
#define IMG_HDR_CRC_OFFSET   444u             /* header CRC32 covers bytes [0, 444) */

#define IMG_FLAG_NONE        0u

typedef struct __attribute__((packed)) {
    uint32_t magic;                 /*   0 */
    uint16_t header_version;        /*   4 */
    uint16_t slot_id;               /*   6  0 = slot A, 1 = slot B */
    uint32_t image_size;            /*   8  payload bytes (after the header) */
    uint32_t load_addr;             /*  12  must equal slot base + 512 */
    uint32_t version;               /*  16  major<<24 | minor<<16 | patch */
    uint32_t security_counter;      /*  20  anti-rollback counter */
    uint32_t flags;                 /*  24 */
    uint32_t build_time;            /*  28  unix seconds */
    uint32_t payload_crc32;         /*  32 */
    uint8_t  payload_sha256[32];    /*  36 */
    uint8_t  reserved[376];         /*  68 */
    uint32_t header_crc32;          /* 444 */
    uint8_t  signature[64];         /* 448  Ed25519 over bytes [0,448) */
} image_header_t;

_Static_assert(sizeof(image_header_t) == IMG_HEADER_SIZE, "image header must be 512 bytes");

#define FW_VER_MAJOR(v) (((v) >> 24) & 0xFFu)
#define FW_VER_MINOR(v) (((v) >> 16) & 0xFFu)
#define FW_VER_PATCH(v) ((v) & 0xFFFFu)
#endif
