#include "uart.h"
#include "bsp.h"
#include <string.h>

#define RX_MASK (UART_RX_BUF - 1u)
#define TX_MASK (UART_TX_BUF - 1u)

static IRQn_Type usart_irqn(USART_TypeDef *r)
{
    if (r == USART1) { return USART1_IRQn; }
    if (r == USART2) { return USART2_IRQn; }
    if (r == USART3) { return USART3_IRQn; }
    return USART6_IRQn;
}

static void usart_clock_enable(USART_TypeDef *r)
{
    if (r == USART1) { RCC->APB2ENR |= RCC_APB2ENR_USART1EN; }
    else if (r == USART2) { RCC->APB1ENR |= RCC_APB1ENR_USART2EN; }
    else if (r == USART3) { RCC->APB1ENR |= RCC_APB1ENR_USART3EN; }
    else { RCC->APB2ENR |= RCC_APB2ENR_USART6EN; }
    (void)RCC->APB1ENR;
}

void uart_init(uart_t *u, USART_TypeDef *reg, uint32_t baud, uint32_t pclk_hz)
{
    memset(u, 0, sizeof *u);
    u->reg = reg;
    usart_clock_enable(reg);
    /* BRR = fck / baud (oversampling by 16): mantissa in [15:4], fraction in [3:0], rounded to nearest */
    reg->BRR = (pclk_hz + (baud / 2u)) / baud;
    reg->CR3 = 0u;
    reg->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_SetPriority(usart_irqn(reg), 6u);                  /* below configMAX_SYSCALL_INTERRUPT_PRIORITY boundary: see FreeRTOSConfig.h */
    NVIC_EnableIRQ(usart_irqn(reg));
}

void uart_set_rs485(uart_t *u, GPIO_TypeDef *p, uint8_t pin)
{
    u->de_port = p; u->de_pin = pin; u->rs485 = true;
    gpio_config(p, pin, GPIO_OUT, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_NOPULL, 0u);
    gpio_clear(p, pin);                                     /* receive by default */
}

void uart_enable_dma_tx(uart_t *u)
{
    /* USART2_TX = DMA1 stream 6, channel 4 (RM0090 table 42) */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; (void)RCC->AHB1ENR;
    u->dma = DMA1; u->tx_dma = DMA1_Stream6; u->dma_stream = 6u; u->use_dma_tx = true;
    u->tx_dma->CR = 0u;
    while ((u->tx_dma->CR & DMA_SxCR_EN) != 0u) { }
    u->tx_dma->PAR = (uint32_t)&u->reg->DR;
    u->tx_dma->CR = (4u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE;   /* mem->periph */
    u->reg->CR3 |= USART_CR3_DMAT;
    NVIC_SetPriority(DMA1_Stream6_IRQn, 6u);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

static inline void de_assert(uart_t *u)   { if (u->rs485) { gpio_set(u->de_port, u->de_pin); } }
static inline void de_release(uart_t *u)  { if (u->rs485) { gpio_clear(u->de_port, u->de_pin); } }

/* Starts the next transmission from the ring if the transmitter is idle. Called with IRQs masked or from ISR. */
static void tx_kick(uart_t *u)
{
    if (u->tx_inflight != 0u) { return; }
    if (u->tx_head == u->tx_tail) { return; }
    if (u->use_dma_tx) {
        uint16_t tail = u->tx_tail, head = u->tx_head;
        uint16_t n = (head > tail) ? (uint16_t)(head - tail) : (uint16_t)(UART_TX_BUF - tail);   /* contiguous chunk */
        u->tx_inflight = n;
        de_assert(u);
        u->tx_dma->M0AR = (uint32_t)&u->tx[tail];
        u->tx_dma->NDTR = n;
        u->dma->HIFCR = 0x0F7D0F7Du;                         /* clear all stream flags (HIFCR covers streams 4-7) */
        u->dma->LIFCR = 0x0F7D0F7Du;
        u->tx_dma->CR |= DMA_SxCR_EN;
    } else {
        u->tx_inflight = 1u;
        de_assert(u);
        u->reg->CR1 |= USART_CR1_TXEIE;
    }
}

size_t uart_write(uart_t *u, const uint8_t *d, size_t n)
{
    size_t done = 0u;
    uint32_t primask = __get_PRIMASK(); __disable_irq();
    while (done < n) {
        uint16_t next = (uint16_t)((u->tx_head + 1u) & TX_MASK);
        if (next == u->tx_tail) { u->tx_overflow++; break; }
        u->tx[u->tx_head] = d[done++]; u->tx_head = next;
    }
    tx_kick(u);
    if (primask == 0u) { __enable_irq(); }
    return done;
}

void uart_write_blocking(uart_t *u, const uint8_t *d, size_t n, uint32_t timeout_us)
{
    size_t off = 0u; uint32_t t0 = time_us();
    while (off < n) {
        off += uart_write(u, d + off, n - off);
        if ((uint32_t)(time_us() - t0) > timeout_us) { return; }
    }
}

bool uart_read_byte(uart_t *u, uint8_t *b)
{
    if (u->rx_head == u->rx_tail) { return false; }
    *b = u->rx[u->rx_tail]; u->rx_tail = (uint16_t)((u->rx_tail + 1u) & RX_MASK);
    return true;
}
size_t uart_rx_available(const uart_t *u) { return (size_t)((u->rx_head - u->rx_tail) & RX_MASK); }
bool uart_tx_idle(const uart_t *u) { return (u->tx_head == u->tx_tail) && (u->tx_inflight == 0u); }
void uart_flush(uart_t *u, uint32_t timeout_us) { uint32_t t0 = time_us(); while (!uart_tx_idle(u) && (uint32_t)(time_us() - t0) < timeout_us) { } }

void uart_irq_handler(uart_t *u)
{
    uint32_t sr = u->reg->SR;
    if ((sr & USART_SR_ORE) != 0u) { u->err_overrun++; }
    if ((sr & USART_SR_FE) != 0u)  { u->err_framing++; }
    if ((sr & USART_SR_NE) != 0u)  { u->err_noise++; }
    if ((sr & USART_SR_PE) != 0u)  { u->err_parity++; }

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {      /* reading DR also clears ORE/FE/NE/PE */
        uint8_t b = (uint8_t)u->reg->DR;
        if ((sr & (USART_SR_FE | USART_SR_NE | USART_SR_PE)) == 0u) {
            uint16_t next = (uint16_t)((u->rx_head + 1u) & RX_MASK);
            if (next != u->rx_tail) { u->rx[u->rx_head] = b; u->rx_head = next; u->rx_bytes++; } else { u->rx_overflow++; }
            if (u->rx_cb != NULL) { u->rx_cb(u->rx_ctx); }
        }
    }
    if (((sr & USART_SR_TXE) != 0u) && ((u->reg->CR1 & USART_CR1_TXEIE) != 0u)) {
        if (u->tx_head != u->tx_tail) {
            u->reg->DR = u->tx[u->tx_tail]; u->tx_tail = (uint16_t)((u->tx_tail + 1u) & TX_MASK); u->tx_bytes++;
        } else {
            u->reg->CR1 &= ~USART_CR1_TXEIE;
            u->reg->CR1 |= USART_CR1_TCIE;                  /* wait for the last bit to leave before releasing RS-485 */
        }
    }
    if (((sr & USART_SR_TC) != 0u) && ((u->reg->CR1 & USART_CR1_TCIE) != 0u)) {
        u->reg->CR1 &= ~USART_CR1_TCIE;
        if (u->tx_head != u->tx_tail) { u->reg->CR1 |= USART_CR1_TXEIE; }
        else { u->tx_inflight = 0u; de_release(u); }
    }
}

void uart_dma_tx_irq_handler(uart_t *u)
{
    uint32_t hisr = u->dma->HISR;                           /* stream 6 -> bits 16 (FEIF) .. 21 (TCIF) in HISR */
    if ((hisr & DMA_HISR_TCIF6) != 0u) {
        u->dma->HIFCR = DMA_HIFCR_CTCIF6;
        u->tx_tail = (uint16_t)((u->tx_tail + u->tx_inflight) & TX_MASK);
        u->tx_bytes += u->tx_inflight;
        u->tx_inflight = 0u;
        u->tx_dma->CR &= ~DMA_SxCR_EN;
        de_release(u);                                      /* NOTE: with DMA, release after TC for strict RS-485 timing */
        tx_kick(u);
    }
    if ((hisr & (DMA_HISR_TEIF6 | DMA_HISR_DMEIF6)) != 0u) {
        u->dma->HIFCR = DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6;
        u->tx_inflight = 0u; u->tx_dma->CR &= ~DMA_SxCR_EN;
        u->tx_tail = u->tx_head;                            /* drop what we cannot send; count it */
        u->tx_overflow++;
    }
}

void uart_puts_polled(USART_TypeDef *reg, const char *s)
{
    while (*s != '\0') {
        uint32_t guard = 200000u;
        while (((reg->SR & USART_SR_TXE) == 0u) && (guard-- > 0u)) { }
        reg->DR = (uint8_t)*s++;
    }
}
