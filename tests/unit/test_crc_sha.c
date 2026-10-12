#include "unity.h"
#include "../../common/crc.h"
#include "../../common/sha256.h"
#include <string.h>
void setUp(void) {} void tearDown(void) {}

static void hex(const uint8_t *d, char *out) { for (int i = 0; i < 32; i++) { sprintf(out + 2*i, "%02x", d[i]); } }

void test_crc32_check_value(void) { TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32_calc((const uint8_t *)"123456789", 9)); }
void test_crc32_empty(void)       { TEST_ASSERT_EQUAL_HEX32(0x00000000u, crc32_calc((const uint8_t *)"", 0)); }
void test_crc32_chained(void)
{
    uint32_t s = crc32_init();
    s = crc32_update(s, (const uint8_t *)"1234", 4); s = crc32_update(s, (const uint8_t *)"56789", 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32_final(s));
}
void test_crc16_check_value(void) { TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc16_ccitt((const uint8_t *)"123456789", 9)); }

void test_sha256_nist_vectors(void)
{
    uint8_t d[32]; char h[65];
    sha256_calc((const uint8_t *)"abc", 3, d); hex(d, h);
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", h);
    sha256_calc((const uint8_t *)"", 0, d); hex(d, h);
    TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", h);
    const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha256_calc((const uint8_t *)m, strlen(m), d); hex(d, h);
    TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", h);
}
void test_sha256_million_a_chunked(void)
{
    sha256_ctx_t c; uint8_t d[32]; char h[65]; uint8_t blk[1000];
    memset(blk, 'a', sizeof blk); sha256_init(&c);
    for (int i = 0; i < 1000; i++) { sha256_update(&c, blk, sizeof blk); }
    sha256_final(&c, d); hex(d, h);
    TEST_ASSERT_EQUAL_STRING("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", h);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_check_value); RUN_TEST(test_crc32_empty); RUN_TEST(test_crc32_chained);
    RUN_TEST(test_crc16_check_value); RUN_TEST(test_sha256_nist_vectors); RUN_TEST(test_sha256_million_a_chunked);
    return UNITY_END();
}
