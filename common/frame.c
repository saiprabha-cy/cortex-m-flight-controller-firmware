#include "frame.h"
#include "crc.h"
#include <string.h>

enum { S_SOF0 = 0, S_SOF1, S_LEN, S_TYPE, S_SEQ, S_PAYLOAD, S_CRC_HI, S_CRC_LO };

void frame_parser_init(frame_parser_t *p) { memset(p, 0, sizeof *p); p->state = S_SOF0; }

bool frame_parser_feed(frame_parser_t *p, uint8_t b)
{
    switch (p->state) {
    case S_SOF0:
        if (b == FRAME_SOF0) { p->state = S_SOF1; } else { p->resyncs++; }
        break;
    case S_SOF1:
        if (b == FRAME_SOF1) { p->state = S_LEN; }
        else { p->state = (b == FRAME_SOF0) ? S_SOF1 : S_SOF0; p->resyncs++; }
        break;
    case S_LEN:
        if (b > FRAME_MAX_PAYLOAD) { p->len_errors++; p->state = S_SOF0; break; }
        p->f.len = b; p->crc_calc = crc16_ccitt_update(0xFFFFu, b); p->state = S_TYPE;
        break;
    case S_TYPE:
        p->f.type = b; p->crc_calc = crc16_ccitt_update(p->crc_calc, b); p->state = S_SEQ;
        break;
    case S_SEQ:
        p->f.seq = b; p->crc_calc = crc16_ccitt_update(p->crc_calc, b); p->idx = 0u;
        p->state = (p->f.len > 0u) ? S_PAYLOAD : S_CRC_HI;
        break;
    case S_PAYLOAD:
        p->f.payload[p->idx++] = b; p->crc_calc = crc16_ccitt_update(p->crc_calc, b);
        if (p->idx >= p->f.len) { p->state = S_CRC_HI; }
        break;
    case S_CRC_HI:
        p->crc_rx = (uint16_t)((uint16_t)b << 8); p->state = S_CRC_LO;
        break;
    case S_CRC_LO:
        p->crc_rx |= b; p->state = S_SOF0;
        if (p->crc_rx == p->crc_calc) { p->frames_ok++; return true; }
        p->crc_errors++;
        break;
    default:
        p->state = S_SOF0;
        break;
    }
    return false;
}

size_t frame_encode(uint8_t type, uint8_t seq, const uint8_t *payload, uint8_t len, uint8_t *out)
{
    if (len > FRAME_MAX_PAYLOAD) { return 0u; }
    size_t n = 0u;
    out[n++] = FRAME_SOF0; out[n++] = FRAME_SOF1;
    out[n++] = len; out[n++] = type; out[n++] = seq;
    if (len > 0u) { memcpy(&out[n], payload, len); n += len; }
    uint16_t crc = crc16_ccitt(&out[2], (size_t)len + 3u);
    out[n++] = (uint8_t)(crc >> 8); out[n++] = (uint8_t)crc;
    return n;
}
