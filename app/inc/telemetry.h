/* telemetry.h - wire formats (little endian, packed). tools/fc_link.py mirrors these layouts. */
#ifndef TELEMETRY_H
#define TELEMETRY_H
#include <stdint.h>

#define TFLAG_SIM_FALLBACK   (1u << 0)
#define TFLAG_IMU_FAULT      (1u << 1)
#define TFLAG_BARO_FAULT     (1u << 2)
#define TFLAG_NODE_LOST      (1u << 3)
#define TFLAG_BROWNOUT_SEEN  (1u << 4)
#define TFLAG_CAN_ERR_PASSIVE (1u << 5)
#define TFLAG_CLOCK_FALLBACK (1u << 6)
#define TFLAG_HIL_SOURCE     (1u << 7)
#define TFLAG_IMAGE_CONFIRMED (1u << 8)

typedef struct __attribute__((packed)) {
    uint8_t  mode;            /* sys_mode_t */
    uint8_t  sensor_src;
    uint16_t flags;           /* TFLAG_* */
    uint32_t uptime_ms;
    float    roll, pitch;                 /* rad (Kalman) */
    float    roll_rate, pitch_rate;       /* rad/s (bias-corrected) */
    float    ax, ay, az;                  /* g */
    float    pressure_pa, altitude_m;
    float    vbat_v, mcu_temp_c;
    float    node_vbat_v, node_current_a;
    uint16_t act_us[4];                   /* actuator pulses */
    uint8_t  cpu_load_pct;
    uint8_t  slot;
} telem_t;                                /* 4+4+... = 82 bytes */

typedef struct __attribute__((packed)) {
    uint32_t fw_version, security_counter;
    uint8_t  slot, mode, reset_cause, bl_state;
    uint32_t image_confirmed;
} version_info_t;

typedef struct __attribute__((packed)) {
    uint32_t uptime_ms;
    uint16_t stack_hwm_words[8];          /* per task: sensor, control, comms, telem, can, hk, timer, idle */
    uint32_t isr_stack_free_bytes;
    uint32_t heap_free, heap_min_free;
    uint8_t  cpu_load_pct; uint8_t pad[3];
    uint32_t frame_ok, frame_crc_err, frame_len_err, frame_resync;
    uint32_t link_rx_overflow, link_tx_overflow, link_err_overrun, link_err_framing;
    uint32_t can_rx, can_tx, can_tx_fail, can_rx_overflow, can_bus_off, can_err_passive; uint8_t can_tec, can_rec; uint16_t pad2;
    uint32_t wdg_soft_miss, wdg_resets_seen, faults_logged;
    uint32_t sensor_imu_fail, sensor_baro_fail;
} stats_t;

typedef struct __attribute__((packed)) {
    uint32_t isr_lat_min_cyc, isr_lat_max_cyc, isr_lat_avg_cyc, isr_lat_samples;
    uint32_t sensor_jitter_max_us, sensor_period_min_us, sensor_period_max_us;
    uint32_t ctrl_exec_max_cyc, ctrl_exec_avg_cyc, ctrl_latency_max_us;
    uint32_t sysclk_hz;
    uint32_t dwt_is_hardware;     /* 1 = real DWT cycle counter, 0 = derived from TIM2 (emulator: 1 us resolution) */
} latency_t;
#endif
