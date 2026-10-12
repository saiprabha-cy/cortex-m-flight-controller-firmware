#include "unity.h"
#include "../../common/mode_fsm.h"
void setUp(void) {} void tearDown(void) {}
void test_boot_to_nominal(void)  { TEST_ASSERT_EQUAL(MODE_NOMINAL, mode_fsm_next(MODE_BOOT, EV_SELF_TEST_OK)); }
void test_self_test_fail_goes_safe(void) { TEST_ASSERT_EQUAL(MODE_SAFE, mode_fsm_next(MODE_BOOT, EV_SELF_TEST_FAIL)); }
void test_degrade_and_recover(void)
{
    sys_mode_t m = mode_fsm_next(MODE_NOMINAL, EV_SENSOR_FAULT); TEST_ASSERT_EQUAL(MODE_DEGRADED, m);
    TEST_ASSERT_EQUAL(MODE_NOMINAL, mode_fsm_next(m, EV_RECOVERED));
}
void test_safe_is_sticky_except_ground_command(void)
{
    for (int e = 0; e < EV_COUNT; e++) {
        sys_mode_t n = mode_fsm_next(MODE_SAFE, (sys_event_t)e);
        if (e == EV_CMD_RECOVER) { TEST_ASSERT_EQUAL(MODE_NOMINAL, n); } else { TEST_ASSERT_EQUAL(MODE_SAFE, n); }
    }
}
void test_critical_events_always_reach_safe_from_operational_modes(void)
{
    const sys_event_t crit[] = { EV_FAULT_CRITICAL, EV_WDG_RESET_LIMIT, EV_BROWNOUT, EV_CMD_SAFE, EV_SELF_TEST_FAIL };
    for (int m = MODE_BOOT; m <= MODE_DEGRADED; m++) for (unsigned i = 0; i < sizeof crit / sizeof crit[0]; i++)
        TEST_ASSERT_EQUAL(MODE_SAFE, mode_fsm_next((sys_mode_t)m, crit[i]));
}
void test_total_function_and_invalid_inputs_fail_safe(void)
{
    for (int m = 0; m < MODE_COUNT; m++) for (int e = 0; e < EV_COUNT; e++) {
        sys_mode_t n = mode_fsm_next((sys_mode_t)m, (sys_event_t)e); TEST_ASSERT_TRUE(n < MODE_COUNT); }
    TEST_ASSERT_EQUAL(MODE_SAFE, mode_fsm_next((sys_mode_t)77, EV_RECOVERED));
    TEST_ASSERT_EQUAL(MODE_SAFE, mode_fsm_next(MODE_NOMINAL, (sys_event_t)99));
}
int main(void)
{
    UNITY_BEGIN(); RUN_TEST(test_boot_to_nominal); RUN_TEST(test_self_test_fail_goes_safe); RUN_TEST(test_degrade_and_recover);
    RUN_TEST(test_safe_is_sticky_except_ground_command); RUN_TEST(test_critical_events_always_reach_safe_from_operational_modes);
    RUN_TEST(test_total_function_and_invalid_inputs_fail_safe); return UNITY_END();
}
