#include "unity.h"
#include "image_builder.h"
#include "../../common/image_verify.h"
#include "../../common/crc.h"
#include <string.h>
static test_keys_t K, ATTACKER; static uint8_t img[SLOT_SIZE];
#define SADDR SLOT_A_BASE
void setUp(void) { test_keys_make(&K, 1); test_keys_make(&ATTACKER, 2); build_image(img, &K, SLOT_A, SADDR, 4000, 0x01000000u, 5); }
void tearDown(void) {}
static img_status_t v(uint32_t min) { return image_verify(img, SADDR, SLOT_A, SLOT_SIZE, min, K.pk, NULL); }
static image_header_t *H(void) { return (image_header_t *)img; }

void test_valid_image(void) { image_header_t h; TEST_ASSERT_EQUAL(IMG_OK, image_verify(img, SADDR, SLOT_A, SLOT_SIZE, 5, K.pk, &h)); TEST_ASSERT_EQUAL_HEX32(0x01000000u, h.version); }
void test_bad_magic(void)  { H()->magic ^= 1; TEST_ASSERT_EQUAL(IMG_ERR_MAGIC, v(0)); }
void test_header_bitflip(void) { H()->version ^= 0x100; TEST_ASSERT_EQUAL(IMG_ERR_HDR_CRC, v(0)); }
void test_wrong_slot_image(void) { TEST_ASSERT_EQUAL(IMG_ERR_SLOT, image_verify(img, SLOT_B_BASE, SLOT_B, SLOT_SIZE, 0, K.pk, NULL)); }
void test_bad_size(void) { H()->image_size = SLOT_SIZE; fix_hdr_crc(img); TEST_ASSERT_EQUAL(IMG_ERR_SIZE, v(0)); }
void test_bad_load_addr(void) { H()->load_addr += 0x200; fix_hdr_crc(img); TEST_ASSERT_EQUAL(IMG_ERR_LOADADDR, v(0)); }
void test_payload_bitflip_every_region(void)
{
    for (uint32_t off = 0; off < 4000; off += 397) { img[IMG_HEADER_SIZE + off] ^= 0x10; TEST_ASSERT_EQUAL(IMG_ERR_PAYLOAD_CRC, v(0)); img[IMG_HEADER_SIZE + off] ^= 0x10; }
    TEST_ASSERT_EQUAL(IMG_OK, v(0));
}
void test_crc_forged_but_sha_wrong(void)    /* attacker fixes CRC32 (it is not a MAC) - SHA-256 catches it */
{ img[IMG_HEADER_SIZE + 100] ^= 0xFF; H()->payload_crc32 = crc32_calc(img + IMG_HEADER_SIZE, 4000); resign(img, &K); TEST_ASSERT_EQUAL(IMG_ERR_SHA256, v(0)); }
void test_forged_crc_and_sha_but_signature_catches(void)
{ img[IMG_HEADER_SIZE + 100] ^= 0xFF; rehash_payload(img); fix_hdr_crc(img); TEST_ASSERT_EQUAL(IMG_ERR_SIGNATURE, v(0)); }
void test_signed_with_wrong_key(void) { resign(img, &ATTACKER); TEST_ASSERT_EQUAL(IMG_ERR_SIGNATURE, v(0)); }
void test_signature_bitflip(void) { H()->signature[5] ^= 1; TEST_ASSERT_EQUAL(IMG_ERR_SIGNATURE, v(0)); }
void test_header_field_tamper_with_fixed_crc_caught_by_signature(void)
{ H()->security_counter = 99; fix_hdr_crc(img); TEST_ASSERT_EQUAL(IMG_ERR_SIGNATURE, v(0)); }
void test_rollback_protection(void) { TEST_ASSERT_EQUAL(IMG_OK, v(5)); TEST_ASSERT_EQUAL(IMG_ERR_ROLLBACK, v(6)); }
void test_bad_vector_table(void)
{ uint32_t sp = 0x1000; memcpy(img + IMG_HEADER_SIZE, &sp, 4); rehash_payload(img); resign(img, &K); TEST_ASSERT_EQUAL(IMG_ERR_VECTOR, v(0)); }
void test_erased_flash_is_not_an_image(void) { memset(img, 0xFF, 1024); TEST_ASSERT_EQUAL(IMG_ERR_MAGIC, v(0)); }
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_valid_image); RUN_TEST(test_bad_magic); RUN_TEST(test_header_bitflip); RUN_TEST(test_wrong_slot_image);
    RUN_TEST(test_bad_size); RUN_TEST(test_bad_load_addr); RUN_TEST(test_payload_bitflip_every_region);
    RUN_TEST(test_crc_forged_but_sha_wrong); RUN_TEST(test_forged_crc_and_sha_but_signature_catches);
    RUN_TEST(test_signed_with_wrong_key); RUN_TEST(test_signature_bitflip);
    RUN_TEST(test_header_field_tamper_with_fixed_crc_caught_by_signature); RUN_TEST(test_rollback_protection);
    RUN_TEST(test_bad_vector_table); RUN_TEST(test_erased_flash_is_not_an_image); return UNITY_END();
}
