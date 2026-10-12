/* uart.h - interrupt-driven USART driver with ring buffers, optional DMA TX, RS-485 driver-enable,
 * and error counters. Register-level (RM0090 section 30). OS-agnostic: an optional RX callback
 * (called from ISR context) lets the application wake a task. */
#ifndef UART_H
#define UART_H
#include "stm32f4xx.h"
#include "gpio.h"
#include <stdbool.h>
#include <stddef.h>

#define UART_RX_BUF 512u
#define UART_TX_BUF 1024u

typedef struct uart uart_t;
struct uart {
    USART_TypeDef *reg;
    uint8_t  rx[UART_RX_BUF]; volatile uint16_t rx_head, rx_tail;
    uint8_t  tx[UART_TX_BUF]; volatile uint16_t tx_head, tx_tail;
    volatile uint16_t tx_inflight;           /* bytes handed to DMA, not yet released from the ring */
    bool     use_dma_tx;
    DMA_Stream_TypeDef *tx_dma; DMA_TypeDef *dma; uint8_t dma_stream;
    GPIO_TypeDef *de_port; uint8_t de_pin; bool rs485;      /* RS-485 driver enable (manual, F4 has no HW DE) */
    void   (*rx_cb)(void *ctx); void *rx_ctx;
    volatile uint32_t err_overrun, err_framing, err_noise, err_parity, rx_overflow, tx_overflow;
    volatile uint32_t rx_bytes, tx_bytes;
};

void   uart_init(uart_t *u, USART_TypeDef *reg, uint32_t baud, uint32_t pclk_hz);
void   uart_enable_dma_tx(uart_t *u);                       /* USART2_TX only: DMA1 stream 6 channel 4 */
void   uart_set_rs485(uart_t *u, GPIO_TypeDef *de_port, uint8_t de_pin);
size_t uart_write(uart_t *u, const uint8_t *d, size_t n);  /* non-blocking; returns bytes queued */
void   uart_write_blocking(uart_t *u, const uint8_t *d, size_t n, uint32_t timeout_us);
bool   uart_read_byte(uart_t *u, uint8_t *b);
size_t uart_rx_available(const uart_t *u);
bool   uart_tx_idle(const uart_t *u);
void   uart_flush(uart_t *u, uint32_t timeout_us);
void   uart_irq_handler(uart_t *u);
void   uart_dma_tx_irq_handler(uart_t *u);
void   uart_puts_polled(USART_TypeDef *reg, const char *s);  /* fault-safe: no interrupts, no buffers */
#endif
