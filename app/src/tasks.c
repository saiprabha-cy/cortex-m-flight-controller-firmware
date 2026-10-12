/* tasks.c - sensor, control, telemetry and CAN tasks. Each task checks in with the watchdog supervisor every loop. */
#include "app.h"
#include "bsp.h"
#include "dbg.h"
#include "pwm.h"
#include "fault.h"
#include <math.h>
#include <string.h>

volatile uint32_t g_imu_stamp_us;
static uint32_t s_imu_fail_streak;

/* ---------------------------------------------------------------- sensor task (100 Hz) */
void sensor_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t prev_us = time_us(), n = 0u, stuck = 0u, good_streak = 0u, baro_bad = 0u;
    imu_sample_t prev = { 0 };
    bool imu_fault = false, baro_fault = false;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FC_SENSOR_PERIOD_MS));
        uint32_t now = time_us();
        latency_note_sensor_period((uint32_t)(now - prev_us)); prev_us = now;

        imu_sample_t s; bool ok = (sensors_read_imu(&s) == 0) && imu_sample_plausible(&s);
        if (ok) {                                                 /* stuck-at detector: a live sensor has noise */
            if ((s.ax == prev.ax) && (s.ay == prev.ay) && (s.az == prev.az) && (s.gx == prev.gx) && (s.gy == prev.gy)) { stuck++; } else { stuck = 0u; }
            if (stuck >= 50u) { ok = false; }
            prev = s;
        }
        if (ok) { good_streak++; s_imu_fail_streak = 0u; } else { good_streak = 0u; s_imu_fail_streak++; }
        if (!imu_fault && (s_imu_fail_streak >= 5u)) {
            imu_fault = true; (void)fault_log_record(FAULT_SENSOR, 1u, 0u, 0u, 0u, 0u);
            mode_post_event(EV_SENSOR_FAULT); dbg_printf("SENS: IMU fault declared\r\n");
        } else if (imu_fault && (good_streak >= 50u)) { imu_fault = false; dbg_printf("SENS: IMU recovered\r\n"); }

        fc_lock();
        g_fc.imu_ok = ok;
        if (ok) { g_fc.imu = s; }
        if (imu_fault) { g_fc.flags |= TFLAG_IMU_FAULT; } else { g_fc.flags &= (uint16_t)~TFLAG_IMU_FAULT; }
        fc_unlock();
        if (ok) { g_imu_stamp_us = now; xTaskNotifyGive(g_task[TID_CONTROL]); }

        if ((n % 10u) == 0u) {                                    /* 10 Hz: barometer + ADC */
            float p, t; bool bok = (sensors_read_baro(&p, &t) == 0) && (p > 30000.0f) && (p < 120000.0f);
            baro_bad = bok ? 0u : (baro_bad + 1u);
            if (!baro_fault && (baro_bad >= 3u)) { baro_fault = true; mode_post_event(EV_SENSOR_FAULT); }
            else if (baro_fault && (baro_bad == 0u)) { baro_fault = false; }
            adc_readings_t a; adc_get(&a);
            fc_lock();
            if (bok) { g_fc.pressure_pa = p; g_fc.baro_temp_c = t; g_fc.altitude_m = baro_altitude_m(p, 101325.0f); }
            g_fc.baro_ok = bok; g_fc.adc = a;
            if (baro_fault) { g_fc.flags |= TFLAG_BARO_FAULT; } else { g_fc.flags &= (uint16_t)~TFLAG_BARO_FAULT; }
            fc_unlock();
        }
        n++;
        WDG_CHECKIN(TID_SENSOR);
    }
}

/* ---------------------------------------------------------------- control task (event driven by the sensor task) */
void control_task(void *arg)
{
    (void)arg;
    kalman1d_t kr, kp; kalman1d_init(&kr, 0.001f, 0.003f, 0.03f); kalman1d_init(&kp, 0.001f, 0.003f, 0.03f);
    pd_ctrl_t pd = { 3.0f, 0.8f, 1.0f };
    uint32_t last_us = time_us(); bool pwm_on = true;
    pwm_init_servo();

    for (;;) {
        uint32_t got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        WDG_CHECKIN(TID_CONTROL);
        if (got == 0u) { continue; }                              /* no fresh sample: do not integrate stale data */
        uint32_t c0 = dwt_cycles(), now = time_us();
        float dt = (float)(uint32_t)(now - last_us) * 1e-6f; last_us = now;
        if ((dt < 0.001f) || (dt > 0.05f)) { dt = 0.01f; }

        fc_lock(); imu_sample_t m = g_fc.imu; sys_mode_t mode = g_fc.mode; fc_unlock();

        float roll_acc = atan2f(m.ay, m.az), pitch_acc = atan2f(-m.ax, sqrtf((m.ay * m.ay) + (m.az * m.az)));
        float roll = kalman1d_update(&kr, m.gx, roll_acc, dt), pitch = kalman1d_update(&kp, m.gy, pitch_acc, dt);
        float rr = m.gx - kr.bias, pr = m.gy - kp.bias;

        uint16_t act[4] = { 1500u, 1500u, 1500u, 1500u };
        if (mode == MODE_SAFE) {
            if (pwm_on) { pwm_disable_outputs(); pwm_on = false; }
        } else {
            if (!pwm_on) { pwm_init_servo(); pwm_on = true; }
            pd.out_limit = (mode == MODE_DEGRADED) ? 0.5f : 1.0f;   /* reduced authority when degraded */
            float ur = pd_ctrl_step(&pd, 0.0f - roll, rr), up = pd_ctrl_step(&pd, 0.0f - pitch, pr);
            act[0] = (uint16_t)(1500.0f + (500.0f * ur)); act[1] = (uint16_t)(1500.0f + (500.0f * up));
            for (uint8_t i = 0u; i < 2u; i++) { pwm_set_us(i, act[i]); }
        }
        uint32_t exec = dwt_cycles() - c0;

        fc_lock();
        g_fc.roll = roll; g_fc.pitch = pitch; g_fc.roll_rate = rr; g_fc.pitch_rate = pr;
        for (uint8_t i = 0u; i < 4u; i++) { g_fc.act_us[i] = act[i]; }
        fc_unlock();
        latency_note_control(exec, (uint32_t)(time_us() - g_imu_stamp_us));
    }
}

/* ---------------------------------------------------------------- telemetry task (10 Hz) */
void telemetry_snapshot(telem_t *t)
{
    fc_lock();
    t->mode = (uint8_t)g_fc.mode; t->sensor_src = (uint8_t)g_fc.src; t->flags = g_fc.flags;
    if (app_image_confirmed()) { t->flags |= TFLAG_IMAGE_CONFIRMED; }
    t->uptime_ms = app_now_ms();
    t->roll = g_fc.roll; t->pitch = g_fc.pitch; t->roll_rate = g_fc.roll_rate; t->pitch_rate = g_fc.pitch_rate;
    t->ax = g_fc.imu.ax; t->ay = g_fc.imu.ay; t->az = g_fc.imu.az;
    t->pressure_pa = g_fc.pressure_pa; t->altitude_m = g_fc.altitude_m;
    t->vbat_v = g_fc.adc.vbat_v; t->mcu_temp_c = g_fc.adc.mcu_temp_c;
    t->node_vbat_v = g_fc.node_vbat_v; t->node_current_a = g_fc.node_current_a;
    for (uint8_t i = 0u; i < 4u; i++) { t->act_us[i] = g_fc.act_us[i]; }
    t->cpu_load_pct = g_fc.cpu_load_pct; t->slot = (uint8_t)app_fw_slot();
    fc_unlock();
}

void telemetry_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount(); uint8_t seq = 0u; uint32_t n = 0u;
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FC_TELEM_PERIOD_MS));
        telem_t t; telemetry_snapshot(&t);
        send_frame(&g_link, MSG_TELEM, seq, (const uint8_t *)&t, (uint8_t)sizeof t);
        if ((n % 5u) == 0u) { send_frame(&g_rs485, MSG_TELEM, seq, (const uint8_t *)&t, (uint8_t)sizeof t); }
        seq++; n++;
        WDG_CHECKIN(TID_TELEM);
    }
}

/* ---------------------------------------------------------------- CAN task */
static bool s_can_ok;
void can_task_set_ok(bool ok) { s_can_ok = ok; }

void can_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount(); uint32_t n = 0u; bool seen = false, lost_reported = false;
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10u));
        uint32_t now = app_now_ms();
        if (s_can_ok) {
            can_msg_t m;
            while (can_receive(&g_can, &m)) {
                fc_lock();
                if ((m.id == CANID_NODE_HEARTBEAT) && !m.ext) { g_fc.node_last_rx_ms = now; g_fc.node_alive = true; seen = true; }
                else if ((m.id == CANID_NODE_POWER) && !m.ext && (m.dlc >= 4u)) {
                    g_fc.node_vbat_v = (float)((uint16_t)(m.data[0] | (m.data[1] << 8))) / 1000.0f;
                    g_fc.node_current_a = (float)((int16_t)(m.data[2] | (m.data[3] << 8))) / 1000.0f;
                    g_fc.node_last_rx_ms = now; g_fc.node_alive = true; seen = true;
                }
                fc_unlock();
            }
            if ((n % (FC_CAN_HB_PERIOD_MS / 10u)) == 0u) {
                can_msg_t hb = { CANID_OBC_HEARTBEAT, false, false, 4u, { (uint8_t)g_fc.mode, (uint8_t)app_fw_slot(), (uint8_t)(now), (uint8_t)(now >> 8) }, 0u };
                (void)can_send(&g_can, &hb, 2000u);
            }
        }
        fc_lock();
        bool expect = seen || (FC_REQUIRE_CAN_NODE != 0);
        bool lost = expect && ((uint32_t)(now - g_fc.node_last_rx_ms) > FC_NODE_TIMEOUT_MS) && (now > FC_NODE_TIMEOUT_MS);
        if (lost) { g_fc.node_alive = false; g_fc.flags |= TFLAG_NODE_LOST; } else { g_fc.flags &= (uint16_t)~TFLAG_NODE_LOST; }
        if (g_can.error_passive_events != 0u) { g_fc.flags |= TFLAG_CAN_ERR_PASSIVE; }
        fc_unlock();
        if (lost && !lost_reported) { lost_reported = true; (void)fault_log_record(FAULT_NODE_LOST, 0u, 0u, 0u, 0u, 0u); mode_post_event(EV_NODE_LOST); dbg_printf("CAN: power node LOST\r\n"); }
        if (!lost && lost_reported) { lost_reported = false; dbg_printf("CAN: power node back\r\n"); }
        n++;
        WDG_CHECKIN(TID_CAN);
    }
}
