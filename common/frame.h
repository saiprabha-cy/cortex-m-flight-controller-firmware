/* frame.h - framed link protocol shared by bootloader, application and host tools.
 *
 *   +------+------+-----+------+-----+-----------------+-------------+
 *   | 0xA5 | 0x5A | len | type | seq | payload[len]    | CRC16 (BE)  |
 *   +------+------+-----+------+-----+-----------------+-------------+
 *   CRC16-CCITT-FALSE over len, type, seq, payload.  len <= FRAME_MAX_PAYLOAD.
 *
 * Reliability: the host retries a request (same seq) until it sees a response with that seq.
 * Every command is idempotent or de-duplicated by (seq) in the dispatcher, so a retry after a lost
 * ACK is harmless. The parser resynchronises on SOF after any error and keeps error counters.
 */
#ifndef FRAME_H
#define FRAME_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FRAME_SOF0         0xA5u
#define FRAME_SOF1         0x5Au
#define FRAME_MAX_PAYLOAD  240u
#define FRAME_OVERHEAD     7u
#define FRAME_MAX_SIZE     (FRAME_MAX_PAYLOAD + FRAME_OVERHEAD)

typedef struct { uint8_t type; uint8_t seq; uint8_t len; uint8_t payload[FRAME_MAX_PAYLOAD]; } frame_t;

typedef struct {
    uint8_t  state;
    uint8_t  idx;
    uint16_t crc_calc;
    uint16_t crc_rx;
    frame_t  f;
    /* statistics (readable by telemetry) */
    uint32_t frames_ok, crc_errors, len_errors, resyncs;
} frame_parser_t;

void   frame_parser_init(frame_parser_t *p);
/* Returns true when a complete, CRC-valid frame is available in p->f. */
bool   frame_parser_feed(frame_parser_t *p, uint8_t byte);
/* Encodes into out (>= FRAME_MAX_SIZE). Returns bytes written, or 0 if len too large. */
size_t frame_encode(uint8_t type, uint8_t seq, const uint8_t *payload, uint8_t len, uint8_t *out);

/* ---- message types ---- */
#define MSG_PING            0x01u
#define MSG_GET_VERSION     0x02u
#define MSG_GET_TELEM       0x03u
#define MSG_SET_MODE        0x04u
#define MSG_REBOOT          0x05u
#define MSG_REQ_UPDATE      0x06u   /* app: set update flag in metadata + reset into bootloader */
#define MSG_DUMP_FAULT      0x07u   /* payload: idx(u8) -> 24-byte record */
#define MSG_GET_STATS       0x08u
#define MSG_CLEAR_FAULTLOG  0x09u
#define MSG_SET_SENSOR_SRC  0x0Au   /* payload: 0=hardware drivers, 1=onboard simulation, 2=host HIL injection */
#define MSG_GET_LATENCY     0x0Bu   /* -> ISR latency / task jitter / execution time statistics */
#define MSG_CAN_NODE_CMD    0x0Cu   /* payload: 1 = tell the CAN power node to go silent, 2 = resume (demo/test of node-loss detection) */
#define MSG_HIL_SENSOR      0x50u   /* host -> fc, no ACK: 7 floats (ax ay az gx gy gz [g, rad/s]) + pressure_pa + temp_c */
#define MSG_INJECT_FAULT    0x7Eu   /* only honoured when built with FC_ENABLE_FAULT_INJECTION */

#define MSG_BL_HELLO        0x10u   /* -> slot target, bootloader version, meta */
#define MSG_BL_ERASE        0x11u   /* payload: slot(u8) */
#define MSG_BL_WRITE        0x12u   /* payload: offset(u32 LE) + data (multiple of 4) */
#define MSG_BL_FINISH       0x13u   /* verify the installed image; marks TRIAL on success */
#define MSG_BL_BOOT         0x14u

#define MSG_TELEM           0x40u   /* unsolicited, periodic */
#define MSG_EVENT           0x41u
#define MSG_ACK_FLAG        0x80u   /* response type = request | 0x80 */
#define MSG_NAK             0xFFu   /* payload: error code (u8) */

#define ERR_UNKNOWN_CMD     1u
#define ERR_BAD_PARAM       2u
#define ERR_BAD_STATE       3u
#define ERR_FLASH           4u
#define ERR_VERIFY          5u
#define ERR_NOT_SUPPORTED   6u
#endif
