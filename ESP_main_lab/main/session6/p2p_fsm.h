/*
 * Session 6 — P2P protocol state machines (platform independent).
 *
 * p2p_fsm.c holds the initiator and responder FSMs. The SAME file is compiled
 *   - for the ESP32-C6  (main/session6/p2p_protocol.c provides the plat_* functions)
 *   - for the laptop    (Prosim/p2p simulator provides the plat_* functions)
 * so the logic tested in the simulator is exactly the logic that runs on the boards.
 *
 * Wire format (payload of the 802.11 data frame, after the LLC/SNAP header):
 *
 *   +-------+------+-----------+-----------+------------------+-----------+
 *   | magic | type | sender[6] | nonce[16] | body: n[4] code  | tag[16]   |
 *   +-------+------+-----------+-----------+------------------+-----------+
 *   \________ associated data (authenticated) _/ \_ encrypted _/ AEAD only
 */
#pragma once

#include <stdint.h>
#include "p2p_config.h"

#define P2P_ID_LEN    6      /* identity = MAC address */
#define P2P_MAGIC     0x6A
#define P2P_NONCE_LEN 16
#define P2P_TAG_LEN   16
#define P2P_BODY_LEN  5      /* n (4 bytes, big-endian) + code (1 byte) */

/* ---- Events: values 0 and 1 are fixed by the lab library ---- */
enum { EV_TIMEOUT = 0, EV_FRAME = 1, EV_START = 2 };

/* ---- Messages ---- */
typedef enum { MSG_PING = 1, MSG_PONG = 2, MSG_END = 3, MSG_ERROR = 4 } msg_type_t;
typedef enum { ERR_NONE = 0, ERR_OUT_OF_SYNC = 1 } err_code_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t type;
    uint8_t sender[P2P_ID_LEN];
    uint8_t nonce[P2P_NONCE_LEN];
} p2p_header_t;

typedef struct __attribute__((packed)) {
    p2p_header_t hdr;
    uint8_t      body[P2P_BODY_LEN + P2P_TAG_LEN];
} p2p_wire_t;

#define P2P_WIRE_LEN (sizeof(p2p_header_t) + P2P_BODY_LEN + (P2P_USE_AEAD ? P2P_TAG_LEN : 0))

/* ---- Roles and states ---- */
typedef enum { P2P_INITIATOR, P2P_RESPONDER } p2p_role_t;
typedef enum { I_IDLE, I_WAIT_PONG, I_DONE, I_ERROR } initiator_state_t;
typedef enum { R_IDLE, R_IN_SESSION } responder_state_t;

typedef struct {
    uint32_t sent, resent, accepted, duplicates;
    uint32_t drop_sender, drop_auth, drop_counter, drop_type;
    uint32_t sim_lost, sim_corrupt;
} p2p_stats_t;

/* Everything one party remembers (the EFSM variables of Appendix C). */
typedef struct {
    p2p_role_t        role;
    const char*       tag;                     /* log tag, "P2P-I" or "P2P-R" */
    uint8_t           local_id[P2P_ID_LEN];
    uint8_t           remote_id[P2P_ID_LEN];
    initiator_state_t i_state;
    responder_state_t r_state;
    uint32_t          n;          /* I: counter in last PING sent; R: counter expected next */
    uint32_t          rounds;     /* k: completed PING/PONG exchanges */
    int               retries;    /* I: resends of current PING; R: silent timeouts */
    p2p_wire_t        last_tx;    /* last_msg: kept for resending the identical frame */
    uint16_t          last_tx_len;
    uint32_t          tx_count;
    uint32_t          t_start_ms;
    const char*       outcome;    /* NULL while running, set when a session ends */
    p2p_stats_t       stats;
} p2p_party_t;

void p2p_party_init(p2p_party_t* p, p2p_role_t role, const char* tag,
                    const uint8_t local_id[P2P_ID_LEN], const uint8_t remote_id[P2P_ID_LEN]);

/* Feed one event into the party's FSM. For EV_FRAME, src_id is the transmitter
 * address from the 802.11 header and payload/len is the P2P payload. */
void p2p_handle_event(p2p_party_t* p, uint8_t event,
                      const uint8_t* src_id, const uint8_t* payload, uint16_t len);

/* ---- Platform layer: implemented once for the ESP32 and once for the simulator ---- */
void     plat_send(p2p_party_t* p, const uint8_t* data, uint16_t len);
void     plat_timer_start(p2p_party_t* p);   /* (re)start the W timer */
void     plat_timer_stop(p2p_party_t* p);
void     plat_indicate(p2p_party_t* p);      /* LED on the board */
void     plat_random(uint8_t* buf, uint16_t len);
uint32_t plat_now_ms(void);
void     plat_delay_ms(uint32_t ms);
void     plat_log(const p2p_party_t* p, char level, const char* fmt, ...)
             __attribute__((format(printf, 3, 4)));
