#include "i2c.h"
#include "bsp.h"

static void pins_af(void)
{
    gpio_config(GPIOB, 6, GPIO_AF, GPIO_OPENDRAIN, GPIO_HIGH_SPEED, GPIO_PULLUP, 4);
    gpio_config(GPIOB, 9, GPIO_AF, GPIO_OPENDRAIN, GPIO_HIGH_SPEED, GPIO_PULLUP, 4);
}

void i2c_init(i2c_t *d, I2C_TypeDef *reg, uint32_t scl_hz)
{
    d->reg = reg; d->timeout_us = 5000u; d->errors = d->recoveries = d->nacks = 0u;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; (void)RCC->APB1ENR;
    pins_af();
    reg->CR1 = I2C_CR1_SWRST; reg->CR1 = 0u;
    uint32_t f = g_pclk1_hz / 1000000u;
    reg->CR2 = f;                                                    /* peripheral clock in MHz */
    if (scl_hz <= 100000u) {
        reg->CCR = g_pclk1_hz / (2u * scl_hz); reg->TRISE = f + 1u;  /* standard mode */
    } else {
        reg->CCR = I2C_CCR_FS | (g_pclk1_hz / (3u * scl_hz)); reg->TRISE = ((f * 300u) / 1000u) + 1u;   /* fast mode 2:1 duty */
    }
    reg->CR1 = I2C_CR1_PE;
}

static int wait_sr1(i2c_t *d, uint32_t mask)
{
    uint32_t t0 = time_us();
    for (;;) {
        uint32_t sr1 = d->reg->SR1;
        if ((sr1 & mask) != 0u) { return BUS_OK; }
        if ((sr1 & I2C_SR1_AF) != 0u) { d->reg->SR1 &= ~I2C_SR1_AF; d->nacks++; return BUS_ERR; }
        if ((sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) != 0u) { return BUS_ERR; }
        if ((uint32_t)(time_us() - t0) > d->timeout_us) { return BUS_ERR; }
    }
}

static int wait_not_busy(i2c_t *d)
{
    uint32_t t0 = time_us();
    while ((d->reg->SR2 & I2C_SR2_BUSY) != 0u) { if ((uint32_t)(time_us() - t0) > d->timeout_us) { return BUS_ERR; } }
    return BUS_OK;
}

void i2c_bus_recover(i2c_t *d)
{
    /* A slave stuck holding SDA low is freed by clocking SCL up to 9 times, then issue STOP (I2C spec UM10204 3.1.16). */
    d->reg->CR1 &= ~I2C_CR1_PE;
    gpio_config(GPIOB, 6, GPIO_OUT, GPIO_OPENDRAIN, GPIO_HIGH_SPEED, GPIO_PULLUP, 0);
    gpio_config(GPIOB, 9, GPIO_IN, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_PULLUP, 0);
    gpio_set(GPIOB, 6);
    for (int i = 0; (i < 9) && (gpio_read(GPIOB, 9) == 0); i++) { gpio_clear(GPIOB, 6); delay_us(5); gpio_set(GPIOB, 6); delay_us(5); }
    pins_af();
    d->reg->CR1 |= I2C_CR1_SWRST; d->reg->CR1 &= ~I2C_CR1_SWRST;
    uint32_t ccr = d->reg->CCR, tr = d->reg->TRISE, cr2 = d->reg->CR2;
    d->reg->CR2 = cr2; d->reg->CCR = ccr; d->reg->TRISE = tr; d->reg->CR1 |= I2C_CR1_PE;
    d->recoveries++;
}

static int do_xfer(i2c_t *d, uint8_t addr7, const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxlen)
{
    I2C_TypeDef *r = d->reg;
    if (wait_not_busy(d) != BUS_OK) { return BUS_ERR; }
    if (txlen > 0u) {
        r->CR1 |= I2C_CR1_START;
        if (wait_sr1(d, I2C_SR1_SB) != BUS_OK) { return BUS_ERR; }
        r->DR = (uint8_t)(addr7 << 1);
        if (wait_sr1(d, I2C_SR1_ADDR) != BUS_OK) { return BUS_ERR; }
        (void)r->SR2;
        for (size_t i = 0u; i < txlen; i++) {
            if (wait_sr1(d, I2C_SR1_TXE) != BUS_OK) { return BUS_ERR; }
            r->DR = tx[i];
        }
        if (wait_sr1(d, I2C_SR1_BTF) != BUS_OK) { return BUS_ERR; }
    }
    if (rxlen == 0u) { r->CR1 |= I2C_CR1_STOP; return BUS_OK; }

    r->CR1 |= I2C_CR1_START | I2C_CR1_ACK;                           /* repeated start */
    if (wait_sr1(d, I2C_SR1_SB) != BUS_OK) { return BUS_ERR; }
    r->DR = (uint8_t)((addr7 << 1) | 1u);
    if (wait_sr1(d, I2C_SR1_ADDR) != BUS_OK) { return BUS_ERR; }
    if (rxlen == 1u) {
        r->CR1 &= ~I2C_CR1_ACK; (void)r->SR2; r->CR1 |= I2C_CR1_STOP;
        if (wait_sr1(d, I2C_SR1_RXNE) != BUS_OK) { return BUS_ERR; }
        rx[0] = (uint8_t)r->DR;
        return BUS_OK;
    }
    (void)r->SR2;
    for (size_t i = 0u; i < rxlen; i++) {
        if (i == (rxlen - 1u)) { r->CR1 &= ~I2C_CR1_ACK; r->CR1 |= I2C_CR1_STOP; }   /* NACK + STOP before the last byte */
        if (wait_sr1(d, I2C_SR1_RXNE) != BUS_OK) { return BUS_ERR; }
        rx[i] = (uint8_t)r->DR;
    }
    return BUS_OK;
}

int i2c_wr_rd(void *ctx, uint8_t addr7, const uint8_t *tx, size_t txlen, uint8_t *rx, size_t rxlen)
{
    i2c_t *d = (i2c_t *)ctx;
    int rc = do_xfer(d, addr7, tx, txlen, rx, rxlen);
    if (rc != BUS_OK) {
        d->errors++;
        d->reg->CR1 |= I2C_CR1_STOP;
        d->reg->SR1 = 0u;
        if ((d->reg->SR2 & I2C_SR2_BUSY) != 0u) { i2c_bus_recover(d); }
    }
    return rc;
}
