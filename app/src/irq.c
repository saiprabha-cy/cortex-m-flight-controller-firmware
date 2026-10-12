/* irq.c - interrupt service routines. Kept tiny: they call the driver handler and return. */
#include "app.h"
#include "bsp.h"
#include "stm32f4xx.h"

static volatile bool s_brownout_flag;
bool irq_brownout_pending(void) { bool f = s_brownout_flag; s_brownout_flag = false; return f; }

void USART1_IRQHandler(void)       { uart_irq_handler(&g_dbg); }
void USART2_IRQHandler(void)       { uart_irq_handler(&g_link); }
void USART3_IRQHandler(void)       { uart_irq_handler(&g_rs485); }
void DMA1_Stream6_IRQHandler(void) { uart_dma_tx_irq_handler(&g_link); }
void CAN1_RX0_IRQHandler(void)     { can_irq_rx0(&g_can); }
void CAN1_SCE_IRQHandler(void)     { can_irq_sce(&g_can); }

/* Programmable voltage detector: fires when VDD drops below the threshold (here ~2.9 V) */
void PVD_IRQHandler(void)
{
    EXTI->PR = (1u << 16);
    s_brownout_flag = true;
}

void pvd_enable(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN; (void)RCC->APB1ENR;
    PWR->CR = (PWR->CR & ~PWR_CR_PLS) | PWR_CR_PLS_LEV6 | PWR_CR_PVDE;     /* PLS=6: 2.9 V */
    EXTI->IMR |= (1u << 16); EXTI->RTSR |= (1u << 16);                      /* rising = VDD below threshold */
    NVIC_SetPriority(PVD_IRQn, 6u); NVIC_EnableIRQ(PVD_IRQn);
}
