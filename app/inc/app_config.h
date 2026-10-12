/* app_config.h - build-time configuration. Override with -D on the CMake command line. */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifndef FC_VERSION
#define FC_VERSION            0x01000000u     /* major<<24 | minor<<16 | patch */
#endif
#ifndef FC_SECURITY_COUNTER
#define FC_SECURITY_COUNTER   1u
#endif
/* 1 = accept debug fault-injection commands (HIL/test builds). MUST be 0 for a "flight" build. */
#ifndef FC_ENABLE_FAULT_INJECTION
#define FC_ENABLE_FAULT_INJECTION 1
#endif
/* default sensor source at boot: 0 hardware, 1 onboard simulation, 2 host HIL injection */
#ifndef FC_SENSOR_DEFAULT
#define FC_SENSOR_DEFAULT     0
#endif
/* if hardware sensors do not answer (e.g. in an emulator) fall back to the simulator instead of DEGRADED */
#ifndef FC_ALLOW_SIM_FALLBACK
#define FC_ALLOW_SIM_FALLBACK 1
#endif
/* 1 = a missing CAN power node is a fault (DEGRADED). 0 = only a node that WAS seen and then vanished is a fault. */
#ifndef FC_REQUIRE_CAN_NODE
#define FC_REQUIRE_CAN_NODE   0
#endif
/* demo: a deliberately broken firmware that never confirms itself (rollback test) */
#ifndef FC_DEMO_CRASH_ON_BOOT
#define FC_DEMO_CRASH_ON_BOOT 0
#endif
/* 1 = use DMA for USART2 TX and ADC; 0 = interrupt-driven TX only (fallback if the emulator lacks DMA) */
#ifndef FC_USE_DMA
#define FC_USE_DMA            1
#endif

#define FC_LINK_BAUD          115200u
#define FC_DBG_BAUD           115200u
#define FC_RS485_BAUD         115200u
#define FC_CAN_BITRATE        500000u

#define FC_SENSOR_PERIOD_MS   10u             /* 100 Hz */
#define FC_TELEM_PERIOD_MS    100u            /* 10 Hz */
#define FC_CAN_HB_PERIOD_MS   100u
#define FC_NODE_TIMEOUT_MS    1000u
#define FC_HK_PERIOD_MS       100u
#define FC_IWDG_TIMEOUT_MS    1000u
#define FC_WDG_GRACE_MS       300u
#define FC_CONFIRM_AFTER_MS   10000u          /* healthy NOMINAL time before the image is confirmed */
#define FC_RECOVER_AFTER_MS   5000u
#define FC_RESET_LIMIT        3u              /* consecutive abnormal resets before booting into SAFE */

/* task priorities (higher number = higher priority) */
#define PRIO_CONTROL          6u
#define PRIO_SENSOR           5u
#define PRIO_COMMS            4u
#define PRIO_CAN              4u
#define PRIO_TELEM            3u
#define PRIO_HK               2u

/* CAN identifiers (11-bit, lower = higher priority) */
#define CANID_OBC_HEARTBEAT   0x100u
#define CANID_OBC_CMD         0x101u
#define CANID_NODE_HEARTBEAT  0x200u
#define CANID_NODE_POWER      0x201u

#endif
