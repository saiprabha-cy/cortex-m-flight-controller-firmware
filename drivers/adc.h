/* adc.h - ADC1 scan + circular DMA2 (stream 0, channel 0): battery divider PA1 (ch1), internal temp (ch16), VREFINT (ch17). */
#ifndef ADC_H
#define ADC_H
#include "stm32f4xx.h"
#include <stdint.h>
typedef struct { float vbat_v, mcu_temp_c, vdda_v; uint32_t raw[3]; } adc_readings_t;
void adc_init(void);
void adc_get(adc_readings_t *out);              /* converts the latest DMA snapshot; never blocks */
extern volatile uint16_t g_adc_dma_buf[3];
#endif
