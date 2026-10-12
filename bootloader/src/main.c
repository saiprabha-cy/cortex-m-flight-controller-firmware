/* Bootloader: A/B slot selection with CRC32 + SHA-256 + Ed25519 verification, trial-boot rollback,
 * and a framed-UART firmware updater. Polled I/O only (no interrupts), ~one screen of control flow.
 * Requirements: FW-BOOT-001..012 (docs/01_requirements.md). */
#include <string.h>
#include "stm32f4xx.h"
#include "bsp.h"
#include "dbg.h"
#include "fault.h"
#include "flash.h"
#include "gpio.h"
#include "bl_config.h"
#include "pubkey.h"
#include "../../common/frame.h"
#include "../../common/boot_meta.h"
#include "../../common/image_verify.h"
#include "../../common/crc.h"
#include "../../config/memory_map.h"

static flashlog_t s_metalog;
static boot_meta_t s_meta;

static const uint32_t k_slot_base[2] = { SLOT_A_BASE, SLOT_B_BASE };

/* ---------- polled USART helpers ---------- */
static void usart_polled_init(USART_TypeDef *r, uint32_t baud)
{
    r->BRR = (g_pclk1_hz + (baud / 2u)) / baud;       /* USART2 on APB1; USART1 uses PCLK2, set below */
    r->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}
static void link_putc(uint8_t b) { while ((USART2->SR & USART_SR_TXE) == 0u) { } USART2->DR = b; }
static void link_send(uint8_t type, uint8_t seq, const uint8_t *p, uint8_t n)
{
    uint8_t buf[FRAME_MAX_SIZE]; size_t len = frame_encode(type, seq, p, n, buf);
    for (size_t i = 0u; i < len; i++) { link_putc(buf[i]); }
    while ((USART2->SR & USART_SR_TC) == 0u) { }
}
static void nak(uint8_t seq, uint8_t code) { link_send(MSG_NAK, seq, &code, 1u); }

/* ---------- verification glue ---------- */
static img_status_t verify_slot_cb(uint32_t slot, uint32_t min_sec, void *ctx)
{
    (void)ctx;
    return image_verify((const uint8_t *)(uintptr_t)k_slot_base[slot], k_slot_base[slot], slot, SLOT_SIZE, min_sec, g_fw_pubkey, NULL);
}

static void meta_persist(const boot_meta_t *before)
{
    if (memcmp(before, &s_meta, sizeof s_meta) != 0) {
        if (boot_meta_save(&s_metalog, &s_meta) != FLASH_OK) { dbg_polled_printf(USART1, "BL: WARN meta save failed\r\n"); }
    }
}

static void log_bad_transitions(const uint8_t prev[2])
{
    for (uint32_t s = 0u; s < 2u; s++) {
        if ((prev[s] != SLOT_STATE_BAD) && (s_meta.slot_state[s] == SLOT_STATE_BAD)) {
            dbg_polled_printf(USART1, "BL: slot %c marked BAD (%s) -> rollback/fallback\r\n", (s == SLOT_A) ? 'A' : 'B',
                              image_status_str((img_status_t)s_meta.last_boot_result));
            (void)fault_log_record(FAULT_BOOT_ROLLBACK, (uint16_t)((s << 8) | s_meta.last_boot_result), 0u, 0u, 0u, 0u);
        }
    }
}

/* ---------- jump to the application ---------- */
static void jump_to_app(uint32_t slot)
{
    uint32_t app = k_slot_base[slot] + IMG_HEADER_SIZE;
    uint32_t sp = *(const volatile uint32_t *)app, rv = *(const volatile uint32_t *)(app + 4u);
    dbg_polled_printf(USART1, "BL: jumping to slot %c @%08x\r\n", (slot == SLOT_A) ? 'A' : 'B', app);
    while ((USART1->SR & USART_SR_TC) == 0u) { }
    __disable_irq();
    SysTick->CTRL = 0u;
    for (uint32_t i = 0u; i < 8u; i++) { NVIC->ICER[i] = 0xFFFFFFFFu; NVIC->ICPR[i] = 0xFFFFFFFFu; }
    RCC->APB1RSTR |= RCC_APB1RSTR_USART2RST | RCC_APB1RSTR_TIM2RST; RCC->APB1RSTR &= ~(RCC_APB1RSTR_USART2RST | RCC_APB1RSTR_TIM2RST);
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST; RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
    SCB->VTOR = app;
    __set_MSP(sp);
    ((void (*)(void))(uintptr_t)rv)();
    for (;;) { }
}

/* ---------- firmware updater ---------- */
typedef struct { uint32_t target; uint32_t next_off; bool erased; } upd_t;

static uint32_t pick_target(void)
{
    /* never overwrite the slot we would roll back to: install into the inactive one,
     * unless the active slot is BAD/EMPTY (recovery), then reuse it. */
    uint32_t act = s_meta.active_slot;
    if ((s_meta.slot_state[act] == SLOT_STATE_BAD) || (s_meta.slot_state[act] == SLOT_STATE_EMPTY)) { return act; }
    return 1u - act;
}

static bool erase_slot(uint32_t slot)
{
    for (uint32_t a = k_slot_base[slot]; a < (k_slot_base[slot] + SLOT_SIZE); a += 0x20000u) {
        if (g_flash_hw_ops.erase_sector(a) != FLASH_OK) { return false; }
    }
    return true;
}

static void handle_frame(const frame_t *f, upd_t *u)
{
    uint8_t out[32];
    switch (f->type) {
    case MSG_BL_HELLO: {
        u->target = pick_target();
        out[0] = BL_VERSION_MAJOR; out[1] = BL_VERSION_MINOR; out[2] = BL_VERSION_PATCH;
        out[3] = s_meta.active_slot; out[4] = (uint8_t)u->target;
        out[5] = s_meta.slot_state[0]; out[6] = s_meta.slot_state[1]; out[7] = s_meta.last_boot_result;
        memcpy(&out[8], &s_meta.min_sec_counter, 4u); memcpy(&out[12], &s_meta.boot_count, 4u);
        link_send((uint8_t)(MSG_BL_HELLO | MSG_ACK_FLAG), f->seq, out, 16u);
        break; }
    case MSG_BL_ERASE:
        if ((f->len != 1u) || (f->payload[0] != u->target)) { nak(f->seq, ERR_BAD_PARAM); break; }
        dbg_polled_printf(USART1, "BL: erasing slot %u ...\r\n", (unsigned)u->target);
        if (erase_slot(u->target)) {
            u->erased = true; u->next_off = 0u;
            s_meta.slot_state[u->target] = SLOT_STATE_EMPTY;    /* an empty slot can never be booted */
            link_send((uint8_t)(MSG_BL_ERASE | MSG_ACK_FLAG), f->seq, NULL, 0u);
        } else { nak(f->seq, ERR_FLASH); }
        break;
    case MSG_BL_WRITE: {
        if (!u->erased || (f->len < 8u) || (((f->len - 4u) & 3u) != 0u)) { nak(f->seq, ERR_BAD_STATE); break; }
        uint32_t off, n = (uint32_t)f->len - 4u; memcpy(&off, f->payload, 4u);
        if ((off + n) > SLOT_SIZE) { nak(f->seq, ERR_BAD_PARAM); break; }
        uint32_t addr = k_slot_base[u->target] + off;
        if (off < u->next_off) {                                 /* host retry of a chunk we already wrote? idempotent ACK */
            if ((off + n) <= u->next_off && memcmp((const void *)(uintptr_t)addr, &f->payload[4], n) == 0) {
                link_send((uint8_t)(MSG_BL_WRITE | MSG_ACK_FLAG), f->seq, NULL, 0u);
            } else { nak(f->seq, ERR_BAD_PARAM); }
            break;
        }
        if (off != u->next_off) { nak(f->seq, ERR_BAD_PARAM); break; }
        if (g_flash_hw_ops.program(addr, &f->payload[4], n) != FLASH_OK) { nak(f->seq, ERR_FLASH); break; }
        u->next_off = off + n;
        link_send((uint8_t)(MSG_BL_WRITE | MSG_ACK_FLAG), f->seq, NULL, 0u);
        break; }
    case MSG_BL_FINISH: {
        image_header_t h;
        img_status_t st = image_verify((const uint8_t *)(uintptr_t)k_slot_base[u->target], k_slot_base[u->target], u->target,
                                       SLOT_SIZE, s_meta.min_sec_counter, g_fw_pubkey, &h);
        dbg_polled_printf(USART1, "BL: verify new image -> %s\r\n", image_status_str(st));
        if (st == IMG_OK) {
            boot_meta_t before = s_meta;
            boot_meta_mark_installed(&s_meta, u->target, h.version);
            meta_persist(&before);
            out[0] = 0u; memcpy(&out[1], &h.version, 4u);
            link_send((uint8_t)(MSG_BL_FINISH | MSG_ACK_FLAG), f->seq, out, 5u);
        } else {
            s_meta.slot_state[u->target] = SLOT_STATE_BAD; s_meta.last_boot_result = (uint8_t)st;
            out[0] = ERR_VERIFY; out[1] = (uint8_t)st; link_send(MSG_NAK, f->seq, out, 2u);
        }
        break; }
    case MSG_BL_BOOT:
        link_send((uint8_t)(MSG_BL_BOOT | MSG_ACK_FLAG), f->seq, NULL, 0u);
        { boot_meta_t before = s_meta; uint8_t prev[2] = { s_meta.slot_state[0], s_meta.slot_state[1] };
          int d = boot_decide(&s_meta, verify_slot_cb, NULL); meta_persist(&before); log_bad_transitions(prev);
          if (d >= 0) { jump_to_app((uint32_t)d); } }
        break;
    case MSG_PING:
        link_send((uint8_t)(MSG_PING | MSG_ACK_FLAG), f->seq, NULL, 0u); break;
    default: nak(f->seq, ERR_UNKNOWN_CMD); break;
    }
}

/* Returns when a BL_BOOT command succeeds (never returns otherwise). */
static void updater_loop(bool wait_forever, uint32_t window_ms, upd_t *u)
{
    frame_parser_t p; frame_parser_init(&p);
    uint32_t t_last = time_us(), t_start = time_ms();
    for (;;) {
        if ((USART2->SR & USART_SR_ORE) != 0u) { (void)USART2->DR; }
        if ((USART2->SR & USART_SR_RXNE) != 0u) {
            uint8_t b = (uint8_t)USART2->DR; t_last = time_us();
            if (frame_parser_feed(&p, b)) {
                handle_frame(&p.f, u);
                wait_forever = true;                               /* host is talking to us: stay until told to boot */
            }
        } else {
            for (volatile uint32_t k = 0u; k < 200u; k++) { }          /* poll every ~1 us: leaves the bus free (and Renode's UART lock uncontended) */
            if ((uint32_t)(time_us() - t_last) > BL_IDLE_RESET_US) { p.state = 0u; t_last = time_us(); }   /* stalled frame */
            if (!wait_forever && ((time_ms() - t_start) >= window_ms)) { return; }
        }
    }
}

int main(void)
{
    clk_status_t clk = bsp_clock_init();
    bsp_timebase_init(); dwt_init(); bsp_enable_fault_handlers();

    gpio_config(GPIOA, 9, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);     /* USART1 TX (debug) */
    gpio_config(GPIOA, 2, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);     /* USART2 TX (link)  */
    gpio_config(GPIOA, 3, GPIO_AF, GPIO_PUSHPULL, GPIO_FAST_SPEED, GPIO_PULLUP, 7);     /* USART2 RX */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; RCC->APB1ENR |= RCC_APB1ENR_USART2EN; (void)RCC->APB1ENR;
    usart_polled_init(USART2, BL_BAUD);
    USART1->BRR = (g_pclk2_hz + (BL_BAUD / 2u)) / BL_BAUD; USART1->CR1 = USART_CR1_UE | USART_CR1_TE;

    reset_cause_t rc = bsp_reset_cause();
    bsp_reset_cause_latch(rc);                              /* RCC flags are cleared now: hand the cause on to the application */
    dbg_polled_printf(USART1, "\r\n=== BOOTLOADER v%u.%u.%u  clk=%s  reset=%s ===\r\n", BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_PATCH,
                      (clk == CLK_PLL_168MHZ) ? "168MHz" : "HSI-fallback", bsp_reset_cause_name(rc));

    (void)fault_log_init(&g_flash_hw_ops);
    (void)flashlog_init(&s_metalog, &g_flash_hw_ops, META_BANK0_BASE, META_BANK_SIZE, META_BANK1_BASE, META_BANK_SIZE);
    (void)boot_meta_load(&s_metalog, &s_meta);

    bool ram_update = bsp_shared_valid() && (bsp_shared()->request == SHARED_REQ_UPDATE);
    bsp_shared_clear();
    if (ram_update) { s_meta.flags |= META_FLAG_UPDATE_REQUEST; dbg_polled_printf(USART1, "BL: update requested by application\r\n"); }

    upd_t u = { pick_target(), 0u, false };
    for (;;) {
        boot_meta_t before = s_meta;
        uint8_t prev_state[2] = { s_meta.slot_state[0], s_meta.slot_state[1] };
        int d = boot_decide(&s_meta, verify_slot_cb, NULL);
        if ((s_meta.flags & META_FLAG_UPDATE_REQUEST) == 0u) { meta_persist(&before); }
        log_bad_transitions(prev_state);
        if (d >= 0) {
            dbg_polled_printf(USART1, "BL: slot %c state=%u attempt=%u boot#%lu - waiting %u ms for host\r\n", (d == SLOT_A) ? 'A' : 'B',
                              (unsigned)s_meta.slot_state[d], (unsigned)s_meta.attempts, (unsigned long)s_meta.boot_count, BL_BOOT_WINDOW_MS);
            updater_loop(false, BL_BOOT_WINDOW_MS, &u);
            jump_to_app((uint32_t)d);                            /* returns only if the stack is corrupt */
        }
        dbg_polled_printf(USART1, "BL: %s - entering update mode (send HELLO/ERASE/WRITE/FINISH/BOOT)\r\n",
                          (d == BOOT_DECISION_UPDATE) ? "update requested" : "NO bootable image");
        s_meta.flags &= (uint8_t)~META_FLAG_UPDATE_REQUEST;      /* consumed (only in RAM until an install completes) */
        u.target = pick_target();
        updater_loop(true, 0u, &u);
    }
}
