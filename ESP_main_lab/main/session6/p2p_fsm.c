/*
 * Session 6, Tasks 2-7 — P2P ping-pong protocol state machines.
 *
 *   (0) I and R pre-share n0, T, a_I, a_R (and the AEAD key)   -> p2p_config.h
 *   (1) I -> R : PING(n0, a_I)
 *   (2) R      : verify a_I and n,  reply PONG(n+1, a_R)
 *   (3) I      : verify a_R and n+1, reply PING(n+2, a_I)
 *   (4) repeat T rounds, then I -> R : END
 *
 * Each party is an extended finite state machine (Appendix C): outer switch on
 * the state, inner switch on the event (EV_START, EV_FRAME, EV_TIMEOUT), and a
 * chain of guards (if / else if) for received frames. Every event is listed in
 * every state. Platform-independent: see p2p_fsm.h.
 */
#include <string.h>

#include "p2p_fsm.h"
#include "aead.h"

/* Per-round logging only for small T, otherwise a progress line every 1000 rounds. */
#define P2P_VERBOSE   (P2P_T <= 100)

static const char* initiator_state_name[] = { "I_IDLE", "I_WAIT_PONG", "I_DONE", "I_ERROR" };
static const char* responder_state_name[] = { "R_IDLE", "R_IN_SESSION" };

typedef struct {
    msg_type_t type;
    uint32_t   n;
    uint8_t    code;
} p2p_msg_t;

typedef enum { RX_OK, RX_NOT_P2P, RX_BAD_SENDER, RX_AUTH_FAIL } rx_result_t;

/* ================================================================== */
/* Encoding / decoding                                                 */
/* ================================================================== */

static void put_u32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t get_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static const char* msg_name(uint8_t type) {
    switch (type) {
    case MSG_PING:  return "PING";
    case MSG_PONG:  return "PONG";
    case MSG_END:   return "END";
    case MSG_ERROR: return "ERROR";
    default:        return "?";
    }
}

static uint16_t p2p_encode(const p2p_party_t* p, p2p_wire_t* w, msg_type_t type, uint32_t n, uint8_t code) {
    uint8_t body[P2P_BODY_LEN];
    memset(w, 0, sizeof(*w));
    w->hdr.magic = P2P_MAGIC;
    w->hdr.type  = type;
    memcpy(w->hdr.sender, p->local_id, P2P_ID_LEN);
    put_u32(body, n);
    body[4] = code;
#if P2P_USE_AEAD
    /* Fresh random 128-bit nonce per frame: never reused under the same key. */
    plat_random(w->hdr.nonce, P2P_NONCE_LEN);
    unsigned long long clen;
    ascon_aead_encrypt(w->body, &clen, body, P2P_BODY_LEN,
                       (const uint8_t*)&w->hdr, sizeof(w->hdr), w->hdr.nonce, P2P_KEY);
#else
    memcpy(w->body, body, P2P_BODY_LEN);
#endif
    return P2P_WIRE_LEN;
}

/* Identity and cryptographic checks. Protocol-level checks (message type,
 * counter value) are the guards in the state machines. */
static rx_result_t p2p_decode(const p2p_party_t* p, const uint8_t* src_id,
                              const uint8_t* payload, uint16_t len, p2p_msg_t* out) {
    if (len < P2P_WIRE_LEN) return RX_NOT_P2P;

    const p2p_wire_t* w = (const p2p_wire_t*)payload;
    if (w->hdr.magic != P2P_MAGIC) return RX_NOT_P2P;
    if (memcmp(src_id, p->remote_id, P2P_ID_LEN) != 0 ||
        memcmp(w->hdr.sender, p->remote_id, P2P_ID_LEN) != 0) {
        return RX_BAD_SENDER;
    }

    uint8_t body[P2P_BODY_LEN];
#if P2P_USE_AEAD
    unsigned long long mlen;
    if (ascon_aead_decrypt(body, &mlen, w->body, P2P_BODY_LEN + P2P_TAG_LEN,
                           (const uint8_t*)&w->hdr, sizeof(w->hdr), w->hdr.nonce, P2P_KEY) != 0) {
        return RX_AUTH_FAIL;
    }
#else
    memcpy(body, w->body, P2P_BODY_LEN);
#endif
    out->type = w->hdr.type;
    out->n    = get_u32(body);
    out->code = body[4];
    return RX_OK;
}

/* ================================================================== */
/* Sending (with fault injection for checkpoint 4)                     */
/* ================================================================== */

static void radio_send(p2p_party_t* p, const p2p_wire_t* w, uint16_t len) {
    p->tx_count++;
#if P2P_SIMULATE_LOSS_EVERY
    if (p->tx_count % P2P_SIMULATE_LOSS_EVERY == 0) {
        plat_log(p, 'W', "[sim] frame #%lu \"lost\" (not transmitted)", (unsigned long)p->tx_count);
        p->stats.sim_lost++;
        return;
    }
#endif
    p2p_wire_t copy = *w;
#if P2P_SIMULATE_CORRUPT_EVERY
    if (p->role == P2P_INITIATOR && p->tx_count % P2P_SIMULATE_CORRUPT_EVERY == 0) {
        plat_log(p, 'W', "[sim] frame #%lu corrupted (one bit flipped in body)", (unsigned long)p->tx_count);
        copy.body[0] ^= 0x01;
        p->stats.sim_corrupt++;
    }
#endif
    plat_send(p, (const uint8_t*)&copy, len);
}

static void p2p_send(p2p_party_t* p, msg_type_t type, uint32_t n, uint8_t code) {
    p->last_tx_len = p2p_encode(p, &p->last_tx, type, n, code);
    p->stats.sent++;
    if (P2P_VERBOSE || type != MSG_PING) {
        plat_log(p, 'I', "-> %s n=%lu%s", msg_name(type), (unsigned long)n,
                 type == MSG_ERROR ? " (reporting error to peer)" : "");
    }
    radio_send(p, &p->last_tx, p->last_tx_len);
}

static void resend_last(p2p_party_t* p) {
    p->stats.resent++;
    radio_send(p, &p->last_tx, p->last_tx_len);
}

/* ================================================================== */
/* Logging helpers                                                     */
/* ================================================================== */

static void log_rx_drop(p2p_party_t* p, rx_result_t rx) {
    switch (rx) {
    case RX_NOT_P2P:
        plat_log(p, 'D', "ignored non-P2P frame");
        break;
    case RX_BAD_SENDER:
        p->stats.drop_sender++;
        plat_log(p, 'W', "invalid_drop: sender identity is not the expected peer");
        break;
    case RX_AUTH_FAIL:
        p->stats.drop_auth++;
        plat_log(p, 'W', "invalid_drop: AEAD tag verification failed (tampered or wrong key)");
        break;
    default:
        break;
    }
}

static void print_summary(p2p_party_t* p, const char* outcome) {
    uint32_t ms = plat_now_ms() - p->t_start_ms;
    p->outcome = outcome;
    plat_log(p, 'I', "==================== SUMMARY ====================");
    plat_log(p, 'I', "outcome            : %s", outcome);
    plat_log(p, 'I', "rounds completed   : %lu / %d", (unsigned long)p->rounds, P2P_T);
    plat_log(p, 'I', "time               : %lu ms (%lu rounds/s)", (unsigned long)ms,
             (unsigned long)(ms ? (uint64_t)p->rounds * 1000 / ms : 0));
    plat_log(p, 'I', "frames sent/resent : %lu / %lu",
             (unsigned long)p->stats.sent, (unsigned long)p->stats.resent);
    plat_log(p, 'I', "frames accepted    : %lu", (unsigned long)p->stats.accepted);
    plat_log(p, 'I', "duplicates handled : %lu", (unsigned long)p->stats.duplicates);
    plat_log(p, 'I', "dropped: sender=%lu auth=%lu counter=%lu type=%lu",
             (unsigned long)p->stats.drop_sender, (unsigned long)p->stats.drop_auth,
             (unsigned long)p->stats.drop_counter, (unsigned long)p->stats.drop_type);
    plat_log(p, 'I', "simulated: lost=%lu corrupted=%lu",
             (unsigned long)p->stats.sim_lost, (unsigned long)p->stats.sim_corrupt);
    plat_log(p, 'I', "=================================================");
}

static void initiator_enter(p2p_party_t* p, initiator_state_t s) {
    plat_log(p, 'I', "state %s -> %s", initiator_state_name[p->i_state], initiator_state_name[s]);
    p->i_state = s;
}

static void responder_enter(p2p_party_t* p, responder_state_t s) {
    plat_log(p, 'I', "state %s -> %s", responder_state_name[p->r_state], responder_state_name[s]);
    p->r_state = s;
}

/* ================================================================== */
/* Initiator FSM                                                       */
/* ================================================================== */

static void initiator_accept_pong(p2p_party_t* p, uint32_t x) {
    plat_timer_stop(p);
    plat_indicate(p);
    p->stats.accepted++;
    p->rounds++;
    if (P2P_VERBOSE) {
        plat_log(p, 'I', "<- PONG n=%lu %s (round %lu/%d)", (unsigned long)x,
                 p->rounds >= P2P_T ? "accept_end" : "accept_continue", (unsigned long)p->rounds, P2P_T);
    } else if (p->rounds % 1000 == 0) {
        plat_log(p, 'I', "progress: %lu/%d rounds", (unsigned long)p->rounds, P2P_T);
    }

    if (p->rounds >= P2P_T) {
        p2p_send(p, MSG_END, x + 1, ERR_NONE);
        initiator_enter(p, I_DONE);
        print_summary(p, "SUCCESS");
        return;
    }

    if (P2P_ROUND_DELAY_MS > 0) {
        plat_delay_ms(P2P_ROUND_DELAY_MS);
    }
    p->n       = x + 1;
    p->retries = 0;
    p2p_send(p, MSG_PING, p->n, ERR_NONE);
    plat_timer_start(p);
}

static void initiator_fsm(p2p_party_t* p, uint8_t event, const p2p_msg_t* m) {
    switch (p->i_state) {
    case I_IDLE:
        switch (event) {
        case EV_START:
            p->n          = P2P_N0;
            p->retries    = 0;
            p->rounds     = 0;
            p->outcome    = NULL;
            p->t_start_ms = plat_now_ms();
            p2p_send(p, MSG_PING, p->n, ERR_NONE);
            plat_timer_start(p);
            initiator_enter(p, I_WAIT_PONG);
            break;
        case EV_FRAME:
            plat_log(p, 'W', "frame before the session started, ignored");
            break;
        case EV_TIMEOUT:
            plat_log(p, 'D', "stray timeout in I_IDLE, ignored");
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case I_WAIT_PONG:
        switch (event) {
        case EV_FRAME:
            if (m->type == MSG_ERROR) {
                if (m->n != p->n) {  /* echo mismatch: an old/replayed error */
                    p->stats.drop_counter++;
                    plat_log(p, 'W', "replay_drop: ERROR for n=%lu, not for current n=%lu",
                             (unsigned long)m->n, (unsigned long)p->n);
                    break;
                }
                plat_timer_stop(p);
                plat_log(p, 'E', "<- ERROR code=%u: responder is out of sync (did it reboot?)", m->code);
                initiator_enter(p, I_ERROR);
                print_summary(p, "FAILED (responder reported error)");
            } else if (m->type != MSG_PONG) {
                p->stats.drop_type++;
                plat_log(p, 'W', "invalid_drop: unexpected %s in I_WAIT_PONG", msg_name(m->type));
            } else if (m->n == p->n + 1) {
                initiator_accept_pong(p, m->n);
            } else if (m->n == p->n - 1) {
                /* R answered a resent PING we had already got the reply for. */
                p->stats.duplicates++;
                plat_log(p, 'W', "duplicate_drop: PONG n=%lu already handled", (unsigned long)m->n);
            } else {
                p->stats.drop_counter++;
                plat_log(p, 'W', "replay_drop: PONG n=%lu, expected %lu (stale or replayed)",
                         (unsigned long)m->n, (unsigned long)(p->n + 1));
            }
            break;

        case EV_TIMEOUT:
            if (++p->retries > P2P_MAX_RETRIES) {
                plat_log(p, 'E', "no reply after %d resends: responder unreachable", P2P_MAX_RETRIES);
                initiator_enter(p, I_ERROR);
                print_summary(p, "FAILED (timeout)");
                break;
            }
            plat_log(p, 'W', "TIMEOUT: resending PING n=%lu (%d/%d)",
                     (unsigned long)p->n, p->retries, P2P_MAX_RETRIES);
            resend_last(p);
            plat_timer_start(p);
            break;

        case EV_START:
            plat_log(p, 'W', "START while a session is running, ignored");
            break;

        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case I_DONE:
    case I_ERROR:
        /* Terminal states: nothing more is sent. Press RESET to run again. */
        switch (event) {
        case EV_START:
        case EV_FRAME:
        case EV_TIMEOUT:
            plat_log(p, 'D', "session over (%s), event %u ignored",
                     initiator_state_name[p->i_state], event);
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;
    }
}

/* ================================================================== */
/* Responder FSM                                                       */
/* ================================================================== */

static void responder_accept_ping(p2p_party_t* p, uint32_t x) {
    plat_indicate(p);
    p->stats.accepted++;
    p->rounds++;
    p->n       = x + 2;
    p->retries = 0;
    if (P2P_VERBOSE) {
        plat_log(p, 'I', "<- PING n=%lu accept (round %lu)", (unsigned long)x, (unsigned long)p->rounds);
    } else if (p->rounds % 1000 == 0) {
        plat_log(p, 'I', "progress: %lu rounds", (unsigned long)p->rounds);
    }
    p2p_send(p, MSG_PONG, x + 1, ERR_NONE);
    plat_timer_start(p);
}

static void responder_start_session(p2p_party_t* p) {
    if (p->r_state == R_IN_SESSION) {
        plat_log(p, 'W', "initiator restarted the session from n0");
    }
    memset(&p->stats, 0, sizeof(p->stats));
    p->rounds     = 0;
    p->outcome    = NULL;
    p->t_start_ms = plat_now_ms();
    responder_accept_ping(p, P2P_N0);
    if (p->r_state != R_IN_SESSION) {
        responder_enter(p, R_IN_SESSION);
    }
}

static void responder_report_out_of_sync(p2p_party_t* p, uint32_t x) {
    p->stats.drop_counter++;
    plat_log(p, 'W', "PING n=%lu does not fit the session state", (unsigned long)x);
    p2p_send(p, MSG_ERROR, x, ERR_OUT_OF_SYNC);
}

static void responder_fsm(p2p_party_t* p, uint8_t event, const p2p_msg_t* m) {
    switch (p->r_state) {
    case R_IDLE:
        switch (event) {
        case EV_FRAME:
            if (m->type == MSG_PING && m->n == P2P_N0) {
                responder_start_session(p);
            } else if (m->type == MSG_PING) {
                responder_report_out_of_sync(p, m->n);
            } else {
                p->stats.drop_type++;
                plat_log(p, 'W', "invalid_drop: unexpected %s in R_IDLE", msg_name(m->type));
            }
            break;
        case EV_TIMEOUT:
            plat_log(p, 'D', "stray timeout in R_IDLE, ignored");
            break;
        case EV_START:
            plat_log(p, 'W', "START is not used by the responder, ignored");
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case R_IN_SESSION:
        switch (event) {
        case EV_FRAME:
            if (m->type == MSG_PING && m->n == p->n) {
                responder_accept_ping(p, m->n);
            } else if (m->type == MSG_PING && m->n == p->n - 2) {
                /* Our PONG was lost and I resent its PING: repeat the same PONG. */
                p->stats.duplicates++;
                p->retries = 0;   /* a resent PING proves I is alive: reset the silence count */
                plat_log(p, 'W', "duplicate: PING n=%lu again, resending last PONG", (unsigned long)m->n);
                resend_last(p);
                plat_timer_start(p);
            } else if (m->type == MSG_PING && m->n == P2P_N0) {
                responder_start_session(p);
            } else if (m->type == MSG_PING) {
                /* Never abort on a PING: an old one is a replay, and a "future" one
                 * cannot come from the honest I. Aborting would let an attacker
                 * kill the session just by replaying a recorded frame. */
                p->stats.drop_counter++;
                if ((int32_t)(m->n - p->n) < 0) {
                    plat_log(p, 'W', "replay_drop: old PING n=%lu, expected %lu",
                             (unsigned long)m->n, (unsigned long)p->n);
                } else {
                    plat_log(p, 'W', "invalid_drop: PING n=%lu is ahead of expected %lu",
                             (unsigned long)m->n, (unsigned long)p->n);
                }
            } else if (m->type == MSG_END && m->n == p->n) {
                plat_timer_stop(p);
                plat_log(p, 'I', "<- END n=%lu", (unsigned long)m->n);
                responder_enter(p, R_IDLE);
                print_summary(p, "SUCCESS");
            } else {
                p->stats.drop_type++;
                plat_log(p, 'W', "invalid_drop: unexpected %s n=%lu in R_IN_SESSION",
                         msg_name(m->type), (unsigned long)m->n);
            }
            break;

        case EV_TIMEOUT:
            if (++p->retries >= P2P_RESPONDER_IDLE_TIMEOUTS) {
                plat_log(p, 'E', "initiator silent for %d ms: abandoning session",
                         P2P_RESPONDER_IDLE_TIMEOUTS * P2P_TIMEOUT_MS);
                responder_enter(p, R_IDLE);
                print_summary(p, "ABANDONED (initiator silent, or END lost)");
            } else {
                plat_timer_start(p);
            }
            break;

        case EV_START:
            plat_log(p, 'W', "START is not used by the responder, ignored");
            break;

        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;
    }
}

/* ================================================================== */
/* Public entry points                                                 */
/* ================================================================== */

void p2p_party_init(p2p_party_t* p, p2p_role_t role, const char* tag,
                    const uint8_t local_id[P2P_ID_LEN], const uint8_t remote_id[P2P_ID_LEN]) {
    memset(p, 0, sizeof(*p));
    p->role    = role;
    p->tag     = tag;
    p->i_state = I_IDLE;
    p->r_state = R_IDLE;
    memcpy(p->local_id, local_id, P2P_ID_LEN);
    memcpy(p->remote_id, remote_id, P2P_ID_LEN);
}

void p2p_handle_event(p2p_party_t* p, uint8_t event,
                      const uint8_t* src_id, const uint8_t* payload, uint16_t len) {
    p2p_msg_t m = { 0 };
    if (event == EV_FRAME) {
        rx_result_t rx = p2p_decode(p, src_id, payload, len, &m);
        if (rx != RX_OK) {
            log_rx_drop(p, rx);
            return;
        }
    }
    if (p->role == P2P_INITIATOR) {
        initiator_fsm(p, event, &m);
    } else {
        responder_fsm(p, event, &m);
    }
}
