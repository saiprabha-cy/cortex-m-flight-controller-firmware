/* verify_cli - host tool: runs the SAME image_verify() code the bootloader uses against a .signed.bin.
 * usage: verify_cli <image.bin> <pubkey.bin(32 bytes)> <slot 0|1> [min_sec_counter]
 * Prints the status string; exit code 0 only for IMG_OK. Used by tests/integration to prove that the
 * Python packager and the C bootloader agree on header layout, CRC, SHA-256 and Ed25519. */
#include "../../common/image_verify.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s image pubkey slot [min_sec]\n", argv[0]); return 2; }
    static uint8_t img[SLOT_SIZE]; uint8_t pk[32];
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror("image"); return 2; }
    size_t n = fread(img, 1, sizeof img, f); fclose(f);
    f = fopen(argv[2], "rb"); if (!f || fread(pk, 1, 32, f) != 32) { fprintf(stderr, "bad pubkey\n"); return 2; } fclose(f);
    uint32_t slot = (uint32_t)atoi(argv[3]); uint32_t min = (argc > 4) ? (uint32_t)atoi(argv[4]) : 0u;
    if (n < IMG_HEADER_SIZE) { puts("TOO_SHORT"); return 1; }
    img_status_t s = image_verify(img, slot ? SLOT_B_BASE : SLOT_A_BASE, slot, SLOT_SIZE, min, pk, NULL);
    puts(image_status_str(s)); return (s == IMG_OK) ? 0 : 1;
}
