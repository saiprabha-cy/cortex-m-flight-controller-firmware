/* mode_fsm.h - system mode state machine (pure logic, host-testable).
 *
 *            SELF_TEST_OK        SENSOR_FAULT / NODE_LOST / WDG_SOFT_MISS
 *   BOOT ---------------> NOMINAL ----------------------------------> DEGRADED
 *    |                       ^   <---- RECOVERED (all healthy for N s) ----+
 *    | SELF_TEST_FAIL        |                                            |
 *    v                       | CMD_RECOVER (ground)                       | FAULT_CRITICAL / CMD_SAFE /
 *   SAFE <------ FAULT_CRITICAL / CMD_SAFE / WDG_RESET_LIMIT / BROWNOUT ---+ WDG_RESET_LIMIT
 */
#ifndef MODE_FSM_H
#define MODE_FSM_H
#include <stdint.h>

typedef enum { MODE_BOOT = 0, MODE_NOMINAL, MODE_DEGRADED, MODE_SAFE, MODE_COUNT } sys_mode_t;
typedef enum {
    EV_SELF_TEST_OK = 0, EV_SELF_TEST_FAIL, EV_SENSOR_FAULT, EV_NODE_LOST, EV_WDG_SOFT_MISS,
    EV_RECOVERED, EV_FAULT_CRITICAL, EV_WDG_RESET_LIMIT, EV_BROWNOUT, EV_CMD_SAFE, EV_CMD_RECOVER,
    EV_COUNT
} sys_event_t;

sys_mode_t  mode_fsm_next(sys_mode_t cur, sys_event_t ev);
const char *mode_name(sys_mode_t m);
const char *event_name(sys_event_t e);
#endif
