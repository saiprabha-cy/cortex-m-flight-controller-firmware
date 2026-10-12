#include "unity.h"
#include "flash_model.h"
#include "../../common/boot_meta.h"
#include <string.h>
static boot_meta_t M; static img_status_t R[2]; static int vcalls; static uint32_t seen_min;
void setUp(void) { flash_model_reset(); boot_meta_default(&M); R[0] = R[1] = IMG_OK; vcalls = 0; }
void tearDown(void) {}
static img_status_t fake_verify(uint32_t slot, uint32_t min, void *ctx) { (void)ctx; vcalls++; seen_min = min; return R[slot]; }

void test_factory_boots_slot_a(void) { TEST_ASSERT_EQUAL(SLOT_A, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL_UINT32(1, M.boot_count); }
void test_no_valid_image_goes_to_recovery(void) { R[0] = IMG_ERR_SIGNATURE; TEST_ASSERT_EQUAL(BOOT_DECISION_NONE, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(SLOT_STATE_BAD, M.slot_state[SLOT_A]); TEST_ASSERT_EQUAL_UINT8(IMG_ERR_SIGNATURE, M.last_boot_result); }
void test_update_request_stays_in_bootloader(void) { M.flags |= META_FLAG_UPDATE_REQUEST; TEST_ASSERT_EQUAL(BOOT_DECISION_UPDATE, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(0, vcalls); }
void test_new_image_trial_then_confirm(void)
{
    boot_meta_mark_installed(&M, SLOT_B, 0x01010000u);
    TEST_ASSERT_EQUAL(SLOT_B, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(1, M.attempts);
    boot_meta_mark_confirmed(&M, SLOT_B, 7);
    TEST_ASSERT_EQUAL(SLOT_STATE_CONFIRMED, M.slot_state[SLOT_B]); TEST_ASSERT_EQUAL_UINT32(7, M.min_sec_counter);
    TEST_ASSERT_EQUAL(SLOT_B, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(0, M.attempts);
}
void test_unconfirmed_image_rolls_back_after_max_attempts(void)
{
    boot_meta_mark_installed(&M, SLOT_B, 1);
    for (unsigned i = 0; i < BOOT_MAX_ATTEMPTS; i++) { TEST_ASSERT_EQUAL(SLOT_B, boot_decide(&M, fake_verify, NULL)); }  /* app crashes, never confirms */
    TEST_ASSERT_EQUAL(SLOT_A, boot_decide(&M, fake_verify, NULL));                       /* 4th boot: rollback */
    TEST_ASSERT_EQUAL(SLOT_STATE_BAD, M.slot_state[SLOT_B]); TEST_ASSERT_EQUAL(SLOT_A, M.active_slot);
    TEST_ASSERT_EQUAL(SLOT_A, boot_decide(&M, fake_verify, NULL));                       /* stays on A */
}
void test_corrupt_new_image_falls_back_immediately(void)
{
    boot_meta_mark_installed(&M, SLOT_B, 1); R[SLOT_B] = IMG_ERR_PAYLOAD_CRC;
    TEST_ASSERT_EQUAL(SLOT_A, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(SLOT_STATE_BAD, M.slot_state[SLOT_B]);
    TEST_ASSERT_EQUAL_UINT8(IMG_ERR_PAYLOAD_CRC, M.last_boot_result);
}
void test_bad_slot_never_retried(void)
{
    boot_meta_mark_installed(&M, SLOT_B, 1); R[SLOT_B] = IMG_ERR_SIGNATURE; boot_decide(&M, fake_verify, NULL);
    R[SLOT_B] = IMG_OK; int before = vcalls; TEST_ASSERT_EQUAL(SLOT_A, boot_decide(&M, fake_verify, NULL)); TEST_ASSERT_EQUAL(before + 1, vcalls);
}
void test_min_sec_counter_is_passed_to_verifier(void) { M.min_sec_counter = 9; boot_decide(&M, fake_verify, NULL); TEST_ASSERT_EQUAL_UINT32(9, seen_min); }
void test_install_clears_update_flag(void) { M.flags = META_FLAG_UPDATE_REQUEST; boot_meta_mark_installed(&M, SLOT_B, 1); TEST_ASSERT_EQUAL_UINT8(0, M.flags); }
void test_meta_persists_through_flashlog_and_powerfail(void)
{
    flashlog_t l; boot_meta_t a, b; boot_meta_default(&a); a.boot_count = 41; a.min_sec_counter = 3;
    flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
    TEST_ASSERT_EQUAL(0, boot_meta_save(&l, &a)); a.boot_count = 42;
    flash_model_fail_after(3); boot_meta_save(&l, &a);                    /* power dies mid-save */
    flash_model_power_on();
    flashlog_init(&l, &g_flash_model_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
    boot_meta_load(&l, &b);
    TEST_ASSERT_TRUE(b.boot_count == 41 || b.boot_count == 42); TEST_ASSERT_EQUAL_UINT32(3, b.min_sec_counter);
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_factory_boots_slot_a); RUN_TEST(test_no_valid_image_goes_to_recovery);
    RUN_TEST(test_update_request_stays_in_bootloader); RUN_TEST(test_new_image_trial_then_confirm);
    RUN_TEST(test_unconfirmed_image_rolls_back_after_max_attempts); RUN_TEST(test_corrupt_new_image_falls_back_immediately);
    RUN_TEST(test_bad_slot_never_retried); RUN_TEST(test_min_sec_counter_is_passed_to_verifier);
    RUN_TEST(test_install_clears_update_flag); RUN_TEST(test_meta_persists_through_flashlog_and_powerfail); return UNITY_END();
}
