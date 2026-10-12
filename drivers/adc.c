#include "adc.h"
#include "gpio.h"

volatile uint16_t g_adc_dma_buf[3];
#define VREFINT_CAL (*(const volatile uint16_t *)0x1FFF7A2Au)   /* factory VREFINT @3.3 V (RM0090 / DS8626) */
#define TS_CAL1     (*(const volatile uint16_t *)0x1FFF7A2Cu)   /* temp sensor @30 degC, VDDA 3.3 V */
#define TS_CAL2     (*(const volatile uint16_t *)0x1FFF7A2Eu)   /* temp sensor @110 degC */

void adc_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN; RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; (void)RCC->APB2ENR;
    gpio_config(GPIOA, 1, GPIO_ANALOG, GPIO_PUSHPULL, GPIO_LOW_SPEED, GPIO_NOPULL, 0);

    ADC->CCR = ADC_CCR_TSVREFE | ADC_CCR_ADCPRE_0;               /* temp+vref channels on, ADCCLK = PCLK2/4 */
    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->SMPR2 = (7u << ADC_SMPR2_SMP1_Pos);                    /* 480 cycles: high-impedance divider */
    ADC1->SMPR1 = (7u << ADC_SMPR1_SMP16_Pos) | (7u << ADC_SMPR1_SMP17_Pos);   /* temp sensor needs >= 10 us */
    ADC1->SQR1 = (2u << ADC_SQR1_L_Pos);                         /* 3 conversions */
    ADC1->SQR3 = (1u << 0) | (16u << 5) | (17u << 10);
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_CONT | ADC_CR2_DMA | ADC_CR2_DDS;

    DMA2_Stream0->CR = 0u;
    while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0u) { }
    DMA2->LIFCR = 0x3Du;                                         /* clear stream 0 flags */
    DMA2_Stream0->PAR = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)g_adc_dma_buf;
    DMA2_Stream0->NDTR = 3u;
    DMA2_Stream0->CR = (0u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC | (1u << DMA_SxCR_MSIZE_Pos) | (1u << DMA_SxCR_PSIZE_Pos);
    DMA2_Stream0->CR |= DMA_SxCR_EN;
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

void adc_get(adc_readings_t *o)
{
    uint16_t b[3] = { g_adc_dma_buf[0], g_adc_dma_buf[1], g_adc_dma_buf[2] };   /* 16-bit atomic reads */
    for (int i = 0; i < 3; i++) { o->raw[i] = b[i]; }
    float vdda = (b[2] != 0u) ? (3.3f * (float)VREFINT_CAL / (float)b[2]) : 3.3f;
    o->vdda_v = vdda;
    o->vbat_v = ((float)b[0] / 4095.0f) * vdda * 2.0f;           /* 1:2 divider on the board */
    float ts = (float)b[1] * vdda / 3.3f;                        /* normalise to the 3.3 V calibration point */
    float span = (float)TS_CAL2 - (float)TS_CAL1;
    o->mcu_temp_c = (span != 0.0f) ? (80.0f * (ts - (float)TS_CAL1) / span + 30.0f) : 25.0f;
}
