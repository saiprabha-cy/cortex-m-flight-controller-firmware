/* power_node - second CAN node (bare metal). Simulates a power-distribution unit:
 *   0x200 heartbeat  [state, counter]   every 100 ms
 *   0x201 power      [vbat_mV u16, current_mA i16]  every 100 ms
 * and listens for the OBC (0x100 heartbeat, 0x101 command: byte0 = 1 stop transmitting, 2 resume). */
#include "bsp.h"
#include "can.h"
#include "uart.h"
#include "gpio.h"
#include "fault.h"
#include "flash.h"
#include "../app/inc/app_config.h"

static can_t s_can;
void CAN1_RX0_IRQHandler(void) { can_irq_rx0(&s_can); }
void CAN1_SCE_IRQHandler(void) { can_irq_sce(&s_can); }

int main(void)
{
    (void)bsp_clock_init(); bsp_timebase_init(); dwt_init();
    gpio_config(GPIOA, 9, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; USART1->BRR = (g_pclk2_hz + 57600u) / 115200u; USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
    uart_puts_polled(USART1, "POWER NODE: starting\r\n");
    int rc = can_init(&s_can, CAN1, FC_CAN_BITRATE, false);
    uart_puts_polled(USART1, rc == 0 ? "POWER NODE: CAN ok\r\n" : "POWER NODE: CAN init FAILED\r\n");

    bool tx_enabled = true; uint32_t next = time_us(), cnt = 0u; uint32_t obc_seen = 0u;
    for (;;) {
        can_msg_t m;
        while (can_receive(&s_can, &m)) {
            if (m.id == CANID_OBC_HEARTBEAT) { obc_seen++; }
            if ((m.id == CANID_OBC_CMD) && (m.dlc >= 1u)) { tx_enabled = (m.data[0] != 1u); uart_puts_polled(USART1, tx_enabled ? "POWER NODE: resume\r\n" : "POWER NODE: going silent\r\n"); }
        }
        if (((uint32_t)(time_us() - next) < 0x80000000u) && ((uint32_t)(time_us() - next) < 1000000u)) {
            next += 100000u; cnt++;
            if (tx_enabled && (rc == 0)) {
                uint16_t mv = (uint16_t)(7400u + ((cnt * 7u) % 60u)); int16_t ma = (int16_t)(850 + (int)((cnt * 13u) % 120u));
                can_msg_t hb = { CANID_NODE_HEARTBEAT, false, false, 2u, { 1u, (uint8_t)cnt }, 0u };
                can_msg_t pw = { CANID_NODE_POWER, false, false, 4u, { (uint8_t)mv, (uint8_t)(mv >> 8), (uint8_t)ma, (uint8_t)((uint16_t)ma >> 8) }, 0u };
                (void)can_send(&s_can, &hb, 2000u); (void)can_send(&s_can, &pw, 2000u);
            }
        }
    }
}
