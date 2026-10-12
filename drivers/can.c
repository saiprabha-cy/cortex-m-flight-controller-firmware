#include "can.h"
#include "bsp.h"
#include <string.h>

typedef struct { uint16_t prescaler; uint8_t bs1, bs2; } timing_t;

/* tq count = 1 + BS1 + BS2 ; sample point ~ (1+BS1)/tq. PCLK1 = 42 MHz. */
static bool timing_for(uint32_t bitrate, timing_t *t)
{
    switch (bitrate) {
    case 1000000u: *t = (timing_t){ 3u, 11u, 2u };  return true;     /* 42M/3 = 14 MHz, 14 tq */
    case 500000u:  *t = (timing_t){ 6u, 11u, 2u };  return true;     /* 7 MHz,  14 tq, SP 85.7 % */
    case 250000u:  *t = (timing_t){ 12u, 11u, 2u }; return true;
    case 125000u:  *t = (timing_t){ 24u, 11u, 2u }; return true;
    default: return false;
    }
}

static int wait_msr(CAN_TypeDef *r, uint32_t mask, bool set)
{
    for (uint32_t i = 0u; i < 100000u; i++) { if (((r->MSR & mask) != 0u) == set) { return 0; } }
    return -1;
}

int can_init(can_t *c, CAN_TypeDef *reg, uint32_t bitrate, bool loopback)
{
    timing_t t;
    memset(c, 0, sizeof *c); c->reg = reg;
    if (!timing_for(bitrate, &t)) { return -2; }
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN; (void)RCC->APB1ENR;
    gpio_config(GPIOD, 0, GPIO_AF, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_PULLUP, 9);     /* PD0 = CAN1_RX */
    gpio_config(GPIOD, 1, GPIO_AF, GPIO_PUSHPULL, GPIO_HIGH_SPEED, GPIO_NOPULL, 9);     /* PD1 = CAN1_TX */

    reg->MCR &= ~CAN_MCR_SLEEP;
    reg->MCR |= CAN_MCR_INRQ;
    if (wait_msr(reg, CAN_MSR_INAK, true) != 0) { return -1; }
    /* ABOM: automatic bus-off recovery; TXFP: FIFO priority by request order; NART off: hardware retries */
    reg->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;
    reg->BTR = ((uint32_t)(t.prescaler - 1u)) | ((uint32_t)(t.bs1 - 1u) << CAN_BTR_TS1_Pos) |
               ((uint32_t)(t.bs2 - 1u) << CAN_BTR_TS2_Pos) | (0u << CAN_BTR_SJW_Pos) | (loopback ? CAN_BTR_LBKM : 0u);
    can_set_filter_accept_all(c);
    reg->IER = CAN_IER_FMPIE0 | CAN_IER_FOVIE0 | CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE | CAN_IER_LECIE | CAN_IER_ERRIE;
    reg->MCR &= ~CAN_MCR_INRQ;
    if (wait_msr(reg, CAN_MSR_INAK, false) != 0) { return -1; }
    NVIC_SetPriority(CAN1_RX0_IRQn, 6u); NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_SetPriority(CAN1_SCE_IRQn, 6u);  NVIC_EnableIRQ(CAN1_SCE_IRQn);
    return 0;
}

void can_set_filter_accept_all(can_t *c)
{
    CAN_TypeDef *r = c->reg;
    r->FMR |= CAN_FMR_FINIT;
    r->FA1R &= ~1u;
    r->FS1R |= 1u;                                  /* bank 0: 32-bit scale */
    r->FM1R &= ~1u;                                 /* mask mode */
    r->sFilterRegister[0].FR1 = 0u; r->sFilterRegister[0].FR2 = 0u;      /* mask 0 -> accept everything */
    r->FFA1R &= ~1u;                                /* assign to FIFO 0 */
    r->FA1R |= 1u;
    r->FMR &= ~CAN_FMR_FINIT;
}

int can_send(can_t *c, const can_msg_t *m, uint32_t timeout_us)
{
    CAN_TypeDef *r = c->reg; uint32_t t0 = time_us(); int mb = -1;
    for (;;) {
        uint32_t tsr = r->TSR;
        if ((tsr & CAN_TSR_TME0) != 0u) { mb = 0; break; }
        if ((tsr & CAN_TSR_TME1) != 0u) { mb = 1; break; }
        if ((tsr & CAN_TSR_TME2) != 0u) { mb = 2; break; }
        if ((uint32_t)(time_us() - t0) > timeout_us) { c->tx_fail++; return -1; }
    }
    CAN_TxMailBox_TypeDef *b = &r->sTxMailBox[mb];
    b->TIR = m->ext ? ((m->id << 3) | CAN_TI0R_IDE) : (m->id << 21);
    if (m->rtr) { b->TIR |= CAN_TI0R_RTR; }
    b->TDTR = m->dlc & 0xFu;
    b->TDLR = (uint32_t)m->data[0] | ((uint32_t)m->data[1] << 8) | ((uint32_t)m->data[2] << 16) | ((uint32_t)m->data[3] << 24);
    b->TDHR = (uint32_t)m->data[4] | ((uint32_t)m->data[5] << 8) | ((uint32_t)m->data[6] << 16) | ((uint32_t)m->data[7] << 24);
    b->TIR |= CAN_TI0R_TXRQ;
    c->tx_count++;
    return 0;
}

bool can_receive(can_t *c, can_msg_t *m)
{
    if (c->rx_head == c->rx_tail) { return false; }
    *m = c->rxq[c->rx_tail]; c->rx_tail = (uint8_t)((c->rx_tail + 1u) % CAN_RXQ_LEN);
    return true;
}

void can_irq_rx0(can_t *c)
{
    CAN_TypeDef *r = c->reg;
    while ((r->RF0R & CAN_RF0R_FMP0) != 0u) {
        CAN_FIFOMailBox_TypeDef *f = &r->sFIFOMailBox[0];
        can_msg_t m;
        m.ext = (f->RIR & CAN_RI0R_IDE) != 0u; m.rtr = (f->RIR & CAN_RI0R_RTR) != 0u;
        m.id = m.ext ? (f->RIR >> 3) : (f->RIR >> 21);
        m.dlc = (uint8_t)(f->RDTR & 0xFu);
        for (int i = 0; i < 4; i++) { m.data[i] = (uint8_t)(f->RDLR >> (8 * i)); m.data[4 + i] = (uint8_t)(f->RDHR >> (8 * i)); }
        m.rx_time_us = time_us();
        uint8_t next = (uint8_t)((c->rx_head + 1u) % CAN_RXQ_LEN);
        if (next != c->rx_tail) { c->rxq[c->rx_head] = m; c->rx_head = next; c->rx_count++; } else { c->rx_overflow++; }
        r->RF0R |= CAN_RF0R_RFOM0;                     /* release the FIFO slot */
    }
    if ((r->RF0R & CAN_RF0R_FOVR0) != 0u) { r->RF0R |= CAN_RF0R_FOVR0; c->rx_overflow++; }
    if (c->rx_cb != NULL) { c->rx_cb(c->rx_ctx); }
}

void can_irq_sce(can_t *c)
{
    CAN_TypeDef *r = c->reg; uint32_t esr = r->ESR;
    c->last_esr = esr;
    if ((esr & CAN_ESR_BOFF) != 0u) { c->bus_off_events++; }
    if ((esr & CAN_ESR_EPVF) != 0u) { c->error_passive_events++; }
    r->MSR = CAN_MSR_ERRI;                               /* clear error interrupt flag */
    r->ESR &= ~CAN_ESR_LEC;
}
uint32_t can_tec(const can_t *c) { return (c->reg->ESR & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos; }
uint32_t can_rec(const can_t *c) { return (c->reg->ESR & CAN_ESR_REC) >> CAN_ESR_REC_Pos; }
