/* can.h - bxCAN driver (CAN1, RM0090 section 32). Classic CAN 2.0A/B.
 * NOTE: STM32F4 has bxCAN only (no CAN-FD). CAN-FD needs FDCAN (STM32G4/H5/H7); the message layer
 * in app/can_proto.h is kept payload-size agnostic so porting only touches this file. */
#ifndef CAN_H
#define CAN_H
#include "gpio.h"
#include <stdbool.h>
typedef struct { uint32_t id; bool ext; bool rtr; uint8_t dlc; uint8_t data[8]; uint32_t rx_time_us; } can_msg_t;

#define CAN_RXQ_LEN 32u
typedef struct {
    CAN_TypeDef *reg;
    can_msg_t rxq[CAN_RXQ_LEN]; volatile uint8_t rx_head, rx_tail;
    volatile uint32_t rx_count, rx_overflow, tx_count, tx_fail, bus_off_events, error_passive_events, last_esr;
    void (*rx_cb)(void *ctx); void *rx_ctx;
} can_t;

/* bitrate 125k/250k/500k/1M assuming PCLK1 = 42 MHz; loopback=true for internal self-test */
int  can_init(can_t *c, CAN_TypeDef *reg, uint32_t bitrate, bool loopback);
int  can_send(can_t *c, const can_msg_t *m, uint32_t timeout_us);
bool can_receive(can_t *c, can_msg_t *m);
void can_set_filter_accept_all(can_t *c);
void can_irq_rx0(can_t *c);
void can_irq_sce(can_t *c);
uint32_t can_tec(const can_t *c);
uint32_t can_rec(const can_t *c);
#endif
