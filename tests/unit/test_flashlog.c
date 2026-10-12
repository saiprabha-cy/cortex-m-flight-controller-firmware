#include "unity.h"
#include "flash_model.h"
#include "../../common/flashlog.h"
#include "../../config/memory_map.h"
#include <string.h>
void setUp(void) { flash_model_reset(); } void tearDown(void) {}

static void mk(uint8_t p[24], uint8_t v) { memset(p, v, 24); }

void test_empty_log(void)
{
    flashlog_t l; uint8_t p[24]; uint32_t s;
    TEST_ASSERT_EQUAL(0, flashlog_init(&l, &g_flash_model_ops, FAULTLOG_BASE, FAULTLOG_SIZE, 0, 0));
    TEST_ASSERT_FALSE(flashlog_latest(&l, p, &s)); TEST_ASSERT_EQUAL_UINT32(0, flashlog_count(&l));
}
void test_append_and_reload(void)
{
    flashlog_t l; uint8_t p[24], q[24]; uint32_t s;
    flashlog_init(&l, &g_flash_model_ops, FAULTLOG_BASE, FAULTLOG_SIZE, 0, 0);
    for (uint8_t i = 1; i <= 5; i++) { mk(p, i); TEST_ASSERT_EQUAL(0, flashlog_append(&l, p)); }
    flashlog_init(&l, &g_flash_model_ops, FAULTLOG_BASE, FAULTLOG_SIZE, 0, 0);       /* "reboot" */
    TEST_ASSERT_EQUAL_UINT32(5, flashlog_count(&l));
    TEST_ASSERT_TRUE(flashlog_latest(&l, q, &s)); TEST_ASSERT_EQUAL_UINT32(5, s); TEST_ASSERT_EQUAL_UINT8(5, q[0]);
    TEST_ASSERT_TRUE(flashlog_read(&l, 0, q, &s)); TEST_ASSERT_EQUAL_UINT8(1, q[0]);
    TEST_ASSERT_FALSE(flashlog_read(&l, 5, q, &s));
    TEST_ASSERT_EQUAL_UINT32(0, flash_model_violations());
}
void test_single_bank_wrap_keeps_going(void)
{
    flashlog_t l; uint8_t p[24];
    flashlog_init(&l, &g_flash_model_ops, FAULTLOG_BASE, FAULTLOG_SIZE, 0, 0);
    uint32_t cap = FAULTLOG_SIZE / 32;
    for (uint32_t i = 0; i < cap + 10; i++) { mk(p, (uint8_t)i); TEST_ASSERT_EQUAL(0, flashlog_append(&l, p)); }
    TEST_ASSERT_EQUAL_UINT32(10, flashlog_count(&l));
    TEST_ASSERT_EQUAL_UINT32(0, flash_model_violations());
}
void test_two_bank_pingpong_never_loses_latest(void)
{
    flashlog_t l; uint8_t p[24], q[24]; uint32_t s, cap = META_BANK_SIZE / 32;
    flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
    for (uint32_t i = 1; i <= 3 * cap + 7; i++) {
        mk(p, (uint8_t)i); TEST_ASSERT_EQUAL(0, flashlog_append(&l, p));
        flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
        TEST_ASSERT_TRUE(flashlog_latest(&l, q, &s)); TEST_ASSERT_EQUAL_UINT32(i, s); TEST_ASSERT_EQUAL_UINT8((uint8_t)i, q[0]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, flash_model_violations());
}
/* Property test: cut power at every possible event while appending (including bank switch-over).
 * Invariant: after power-up the newest record is either the old one or the new one - never lost. */
static void powerfail_sweep(uint32_t prefill)
{
    for (long cut = 0; cut < 40; cut++) {
        flash_model_reset();
        flashlog_t l; uint8_t p[24], q[24]; uint32_t s;
        flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
        for (uint32_t i = 1; i <= prefill; i++) { mk(p, (uint8_t)i); flashlog_append(&l, p); }
        mk(p, (uint8_t)(prefill + 1));
        flash_model_fail_after(cut); flashlog_append(&l, p);      /* may be interrupted */
        flash_model_power_on();
        flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
        TEST_ASSERT_TRUE_MESSAGE(flashlog_latest(&l, q, &s), "record lost after power failure");
        TEST_ASSERT_TRUE(q[0] == (uint8_t)prefill || q[0] == (uint8_t)(prefill + 1));
        mk(p, 0x77);                                              /* and the log must keep working */
        TEST_ASSERT_EQUAL(0, flashlog_append(&l, p));
        flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
        TEST_ASSERT_TRUE(flashlog_latest(&l, q, &s)); TEST_ASSERT_EQUAL_UINT8(0x77, q[0]);
    }
}
void test_powerfail_mid_log(void)        { powerfail_sweep(5); }
void test_powerfail_at_bank_switch(void) { powerfail_sweep(META_BANK_SIZE / 32); }   /* bank exactly full */
void test_powerfail_after_first_switch(void) { powerfail_sweep(META_BANK_SIZE / 32 + 3); }
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_empty_log); RUN_TEST(test_append_and_reload); RUN_TEST(test_single_bank_wrap_keeps_going);
    RUN_TEST(test_two_bank_pingpong_never_loses_latest); RUN_TEST(test_powerfail_mid_log);
    RUN_TEST(test_powerfail_at_bank_switch); RUN_TEST(test_powerfail_after_first_switch); return UNITY_END();
}
