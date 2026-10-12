#include "spi.h"
#include "bsp.h"

void spi_init(spi_t *s, SPI_TypeDef *reg, uint8_t mode, uint8_t pre, GPIO_TypeDef *cs_port, uint8_t cs_pin)
{
    s->reg = reg; s->cs_port = cs_port; s->cs_pin = cs_pin; s->timeout_us = 2000u; s->errors = 0u;
    if (reg == SPI1) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        gpio_config(GPIOA, 5, GPIO_AF, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_NOPULL, 5);
        gpio_config(GPIOA, 6, GPIO_AF, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_PULLUP, 5);
        gpio_config(GPIOA, 7, GPIO_AF, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_NOPULL, 5);
    }
    gpio_config(cs_port, cs_pin, GPIO_OUT, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_NOPULL, 0);
    gpio_set(cs_port, cs_pin);                                       /* CS idle high */
    reg->CR1 = 0u;
    reg->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | ((uint32_t)(pre - 1u) << SPI_CR1_BR_Pos) |
               (((mode & 1u) != 0u) ? SPI_CR1_CPHA : 0u) | (((mode & 2u) != 0u) ? SPI_CR1_CPOL : 0u);
    reg->CR1 |= SPI_CR1_SPE;
}

static int wait_sr(spi_t *s, uint32_t mask, uint32_t want)
{
    uint32_t t0 = time_us();
    while ((s->reg->SR & mask) != want) { if ((uint32_t)(time_us() - t0) > s->timeout_us) { s->errors++; return BUS_ERR; } }
    return BUS_OK;
}

int spi_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_t *s = (spi_t *)ctx; int rc = BUS_OK;
    gpio_clear(s->cs_port, s->cs_pin);
    for (size_t i = 0u; i < len; i++) {
        if (wait_sr(s, SPI_SR_TXE, SPI_SR_TXE) != BUS_OK) { rc = BUS_ERR; break; }
        *(volatile uint8_t *)&s->reg->DR = tx[i];
        if (wait_sr(s, SPI_SR_RXNE, SPI_SR_RXNE) != BUS_OK) { rc = BUS_ERR; break; }
        uint8_t b = *(volatile uint8_t *)&s->reg->DR;
        if (rx != NULL) { rx[i] = b; }
    }
    if (rc == BUS_OK) { (void)wait_sr(s, SPI_SR_BSY, 0u); }
    gpio_set(s->cs_port, s->cs_pin);
    if ((s->reg->SR & SPI_SR_OVR) != 0u) { (void)s->reg->DR; (void)s->reg->SR; s->errors++; }   /* clear overrun */
    return rc;
}
