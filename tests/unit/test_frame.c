#include "unity.h"
#include "../../common/frame.h"
#include <string.h>
#include <stdlib.h>
void setUp(void) {} void tearDown(void) {}

static bool feed_all(frame_parser_t *p, const uint8_t *b, size_t n)
{ bool got = false; for (size_t i = 0; i < n; i++) { if (frame_parser_feed(p, b[i])) { got = true; } } return got; }

void test_roundtrip(void)
{
    uint8_t buf[FRAME_MAX_SIZE], pl[5] = {1,2,3,4,5}; frame_parser_t p; frame_parser_init(&p);
    size_t n = frame_encode(MSG_SET_MODE, 42, pl, 5, buf);
    TEST_ASSERT_EQUAL(12, n);
    TEST_ASSERT_TRUE(feed_all(&p, buf, n));
    TEST_ASSERT_EQUAL_UINT8(MSG_SET_MODE, p.f.type); TEST_ASSERT_EQUAL_UINT8(42, p.f.seq);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pl, p.f.payload, 5);
}
void test_empty_and_max_payload(void)
{
    uint8_t buf[FRAME_MAX_SIZE], pl[FRAME_MAX_PAYLOAD]; frame_parser_t p; frame_parser_init(&p);
    for (unsigned i = 0; i < FRAME_MAX_PAYLOAD; i++) { pl[i] = (uint8_t)i; }
    TEST_ASSERT_TRUE(feed_all(&p, buf, frame_encode(MSG_PING, 0, NULL, 0, buf)));
    TEST_ASSERT_TRUE(feed_all(&p, buf, frame_encode(MSG_TELEM, 1, pl, FRAME_MAX_PAYLOAD, buf)));
    TEST_ASSERT_EQUAL_UINT8(FRAME_MAX_PAYLOAD, p.f.len); TEST_ASSERT_EQUAL_UINT8_ARRAY(pl, p.f.payload, FRAME_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL(0, frame_encode(MSG_PING, 0, pl, 241, buf));
}
void test_every_single_bit_flip_is_rejected(void)
{
    uint8_t buf[FRAME_MAX_SIZE], pl[8] = {9,8,7,6,5,4,3,2};
    size_t n = frame_encode(MSG_GET_TELEM, 7, pl, 8, buf);
    for (size_t byte = 0; byte < n; byte++) for (int bit = 0; bit < 8; bit++) {
        uint8_t t[FRAME_MAX_SIZE]; frame_parser_t p; frame_parser_init(&p);
        memcpy(t, buf, n); t[byte] ^= (uint8_t)(1u << bit);
        bool ok = feed_all(&p, t, n);
        if (ok) { TEST_ASSERT_TRUE_MESSAGE(false, "corrupted frame accepted"); }
    }
}
void test_garbage_prefix_then_valid_frame(void)
{
    uint8_t buf[FRAME_MAX_SIZE], junk[64]; frame_parser_t p; frame_parser_init(&p);
    memset(junk, 0x33, sizeof junk); junk[10] = FRAME_SOF0;                 /* stray SOF0 */
    feed_all(&p, junk, sizeof junk);
    TEST_ASSERT_TRUE(feed_all(&p, buf, frame_encode(MSG_PING, 3, NULL, 0, buf)));
    TEST_ASSERT_GREATER_THAN_UINT32(0, p.resyncs);
}
void test_crc_error_counted_and_recovers(void)
{
    uint8_t buf[FRAME_MAX_SIZE]; frame_parser_t p; frame_parser_init(&p);
    size_t n = frame_encode(MSG_PING, 1, NULL, 0, buf); buf[n-1] ^= 0xFF;
    TEST_ASSERT_FALSE(feed_all(&p, buf, n)); TEST_ASSERT_EQUAL_UINT32(1, p.crc_errors);
    TEST_ASSERT_TRUE(feed_all(&p, buf, frame_encode(MSG_PING, 2, NULL, 0, buf)));
}
void test_oversize_len_rejected(void)
{
    frame_parser_t p; frame_parser_init(&p); uint8_t b[] = { FRAME_SOF0, FRAME_SOF1, 250, 1, 1 };
    TEST_ASSERT_FALSE(feed_all(&p, b, sizeof b)); TEST_ASSERT_EQUAL_UINT32(1, p.len_errors);
}
void test_random_noise_never_crashes(void)    /* run under -fsanitize=address,undefined */
{
    frame_parser_t p; frame_parser_init(&p); srand(1234);
    for (int i = 0; i < 2000000; i++) { frame_parser_feed(&p, (uint8_t)rand()); }
    TEST_PASS();
}
void test_back_to_back_frames(void)
{
    uint8_t a[FRAME_MAX_SIZE], b[FRAME_MAX_SIZE], s[2*FRAME_MAX_SIZE]; frame_parser_t p; frame_parser_init(&p);
    size_t na = frame_encode(MSG_PING, 1, NULL, 0, a), nb = frame_encode(MSG_GET_VERSION, 2, NULL, 0, b);
    memcpy(s, a, na); memcpy(s + na, b, nb); feed_all(&p, s, na + nb);
    TEST_ASSERT_EQUAL_UINT32(2, p.frames_ok);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip); RUN_TEST(test_empty_and_max_payload); RUN_TEST(test_every_single_bit_flip_is_rejected);
    RUN_TEST(test_garbage_prefix_then_valid_frame); RUN_TEST(test_crc_error_counted_and_recovers);
    RUN_TEST(test_oversize_len_rejected); RUN_TEST(test_random_noise_never_crashes); RUN_TEST(test_back_to_back_frames);
    return UNITY_END();
}
