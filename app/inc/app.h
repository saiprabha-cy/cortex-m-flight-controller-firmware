/* app.h - shared application state and cross-module API. */
#ifndef APP_H
#define APP_H
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "app_config.h"
#include "telemetry.h"
#include "uart.h"
#include "can.h"
#include "imu_mpu6500.h"
#include "baro_bmp280.h"
#include "adc.h"
#include "kalman.h"
#include "../../common/mode_fsm.h"
#include "../../common/frame.h"
#include "../../common/wdg_mgr.h"
#include "../../common/fault_types.h"

typedef enum { SRC_HW = 0, SRC_SIM = 1, SRC_HIL = 2 } sensor_src_t;
typedef enum { TID_SENSOR = 0, TID_CONTROL, TID_COMMS, TID_TELEM, TID_CAN, TID_HK, TID_COUNT } task_id_t;

typedef struct {
    sys_mode_t mode; sensor_src_t src; uint16_t flags;
    imu_sample_t imu; float pressure_pa, baro_temp_c, altitude_m;
    float roll, pitch, roll_rate, pitch_rate;
    uint16_t act_us[4];
    adc_readings_t adc;
    float node_vbat_v, node_current_a; uint32_t node_last_rx_ms; bool node_alive;
    uint8_t cpu_load_pct;
    bool imu_ok, baro_ok;
} fc_state_t;

extern fc_state_t g_fc;
extern SemaphoreHandle_t g_fc_mx;
extern uart_t g_link, g_dbg, g_rs485;
extern can_t g_can;
extern TaskHandle_t g_task[TID_COUNT];
extern bool g_hw_sensors_present;
extern wdg_t g_wdg;
extern int g_wdg_id[TID_COUNT];
extern frame_parser_t g_link_parser, g_rs485_parser;
extern volatile uint32_t g_isr_stamp_cyc;
extern volatile uint32_t g_imu_stamp_us;
#define WDG_CHECKIN(tid) wdg_checkin(&g_wdg, g_wdg_id[(tid)], app_now_ms())
extern uint32_t g_boot_slot;
extern uint32_t g_wdg_soft_miss_total, g_wdg_resets_seen;

static inline void fc_lock(void)   { (void)xSemaphoreTake(g_fc_mx, portMAX_DELAY); }
static inline void fc_unlock(void) { (void)xSemaphoreGive(g_fc_mx); }
static inline uint32_t app_now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

/* tasks */
void sensor_task(void *arg); void control_task(void *arg); void comms_task(void *arg);
void telemetry_task(void *arg); void can_task(void *arg); void housekeeping_task(void *arg);

/* mode manager */
void mode_post_event(sys_event_t ev);                  /* ISR-unsafe; call from tasks */
void mode_post_event_from_isr(sys_event_t ev);
bool mode_event_receive(sys_event_t *ev);              /* housekeeping only */

/* sensors */
int  sensors_init(void);
void sensors_hil_push(const uint8_t *payload, uint8_t len);
void sensors_set_source(sensor_src_t s);
void sensors_freeze(bool on);                          /* fault injection: stuck-at */
int  sensors_read_imu(imu_sample_t *s);
int  sensors_read_baro(float *pressure_pa, float *temp_c);
uint32_t sensors_imu_fail_count(void); uint32_t sensors_baro_fail_count(void);

/* commands */
void cmd_dispatch(uart_t *reply, const frame_t *f);
void telemetry_snapshot(telem_t *t);
void send_frame(uart_t *u, uint8_t type, uint8_t seq, const uint8_t *p, uint8_t n);

/* latency instrumentation */
void latency_probe_trigger(void);
void latency_get(latency_t *l);
void latency_note_sensor_period(uint32_t period_us);
void latency_note_control(uint32_t exec_cycles, uint32_t latency_us);

/* boot-confirm */
void app_confirm_image(void);
bool app_image_confirmed(void);
uint32_t app_fw_slot(void);
#endif
