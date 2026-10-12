#include "unity.h"
#include "../../common/wdg_mgr.h"
#include "../../common/fault_types.h"
void setUp(void) {} void tearDown(void) {}

void test_healthy_tasks_never_late(void)
{
    wdg_t w; wdg_init(&w, 300); int a = wdg_register(&w, "a", 100, 0), b = wdg_register(&w, "b", 500, 0);
    for (uint32_t t = 10; t < 10000; t += 10) { wdg_checkin(&w, a, t); if (t % 200 == 0) { wdg_checkin(&w, b, t); }
        wdg_result_t r = wdg_supervise(&w, t); TEST_ASSERT_EQUAL_UINT32(0, r.late_mask); TEST_ASSERT_FALSE(r.hard_fail); }
}
void test_soft_miss_reported_once_then_hard_fail_after_grace(void)
{
    wdg_t w; wdg_init(&w, 300); int a = wdg_register(&w, "sensor", 100, 0); (void)wdg_register(&w, "ok", 100000, 0);
    wdg_checkin(&w, a, 1000);
    wdg_result_t r = wdg_supervise(&w, 1100); TEST_ASSERT_EQUAL_UINT32(0, r.late_mask);
    r = wdg_supervise(&w, 1150); TEST_ASSERT_EQUAL_UINT32(1, r.newly_late_mask); TEST_ASSERT_FALSE(r.hard_fail);
    r = wdg_supervise(&w, 1200); TEST_ASSERT_EQUAL_UINT32(0, r.newly_late_mask); TEST_ASSERT_EQUAL_UINT32(1, r.late_mask);
    r = wdg_supervise(&w, 1401); TEST_ASSERT_TRUE(r.hard_fail);                          /* > deadline + grace */
    TEST_ASSERT_EQUAL_UINT32(1, w.e[a].miss_count);
}
void test_recovery_clears_late_and_allows_new_miss(void)
{
    wdg_t w; wdg_init(&w, 300); int a = wdg_register(&w, "t", 100, 0);
    wdg_supervise(&w, 150); wdg_checkin(&w, a, 160); wdg_result_t r = wdg_supervise(&w, 170); TEST_ASSERT_EQUAL_UINT32(0, r.late_mask);
    r = wdg_supervise(&w, 400); TEST_ASSERT_EQUAL_UINT32(1, r.newly_late_mask); TEST_ASSERT_EQUAL_UINT32(2, w.e[a].miss_count);
}
void test_tick_counter_wraparound(void)
{
    wdg_t w; wdg_init(&w, 300); int a = wdg_register(&w, "t", 100, 0xFFFFFF00u);
    wdg_checkin(&w, a, 0xFFFFFFF0u); wdg_result_t r = wdg_supervise(&w, 0x00000010u);    /* 32 ms later, across the wrap */
    TEST_ASSERT_EQUAL_UINT32(0, r.late_mask);
    r = wdg_supervise(&w, 0x00000100u); TEST_ASSERT_EQUAL_UINT32(1, r.late_mask);
}
void test_registry_full_and_bad_ids(void)
{
    wdg_t w; wdg_init(&w, 1); for (int i = 0; i < 8; i++) { TEST_ASSERT_EQUAL(i, wdg_register(&w, "x", 1, 0)); }
    TEST_ASSERT_EQUAL(-1, wdg_register(&w, "y", 1, 0)); wdg_checkin(&w, -1, 5); wdg_checkin(&w, 99, 5);
}
void test_policy_counts_trailing_abnormal_resets(void)
{
    uint8_t a[] = { FAULT_COLD_BOOT, FAULT_HARDFAULT, FAULT_SOFT_RESET, FAULT_WDG_RESET, FAULT_TASK_WDG, FAULT_SOFT_RESET, FAULT_STACK_OVF };
    TEST_ASSERT_EQUAL_UINT32(3, fault_policy_trailing_abnormal(a, sizeof a));
    uint8_t b[] = { FAULT_HARDFAULT, FAULT_HARDFAULT, FAULT_NONE, FAULT_WDG_RESET };            /* healthy marker resets the count */
    TEST_ASSERT_EQUAL_UINT32(1, fault_policy_trailing_abnormal(b, sizeof b));
    uint8_t c[] = { FAULT_HARDFAULT, FAULT_BOOT_ROLLBACK }; TEST_ASSERT_EQUAL_UINT32(0, fault_policy_trailing_abnormal(c, sizeof c));
    TEST_ASSERT_EQUAL_UINT32(0, fault_policy_trailing_abnormal(a, 0));
}
int main(void) { UNITY_BEGIN(); RUN_TEST(test_healthy_tasks_never_late); RUN_TEST(test_soft_miss_reported_once_then_hard_fail_after_grace);
    RUN_TEST(test_recovery_clears_late_and_allows_new_miss); RUN_TEST(test_tick_counter_wraparound); RUN_TEST(test_registry_full_and_bad_ids);
    RUN_TEST(test_policy_counts_trailing_abnormal_resets); return UNITY_END(); }
