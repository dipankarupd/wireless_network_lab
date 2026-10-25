/*
 * Session 9 — MP-SAE (Protocol B, Figure 5): group arithmetic + the two FSMs.
 *
 *   A (initiator)                          B (responder)
 *   r_a,m_a random; s_a=(r_a+m_a) mod n    r_b,m_b random; s_b=(r_b+m_b) mod n
 *   t_a = g^-m_a mod p                     t_b = g^-m_b mod p
 *   c1 = (s_a,t_a)  ------------------->   check c1 (s_a,t_a in [1,p-1])
 *                                          store (s_a,t_a) as peer's
 *   check c2 (s_b,t_b)  <----------------  c2 = (s_b,t_b)
 *   k_a = (g^s_b * t_b)^r_a mod p
 *   v_a = prf(KDF(k_a), s_a,t_a,s_b,t_b)
 *   c3 = v_a       ------------------->    check c3
 *                                          k_b = (g^s_a * t_a)^r_b mod p
 *                                          v_b = prf(KDF(k_b), s_a,t_a,s_b,t_b)
 *                                          verify v_b == v_a(received)
 *   check c4 (v_b == v_a, our own)  <---   c4 = v_b
 *
 * Because k_a == k_b (proved by algebra in the README) and both sides hash the
 * SAME canonical tuple (s_a,t_a,s_b,t_b) -- "A's values always first" -- v_a
 * and v_b are literally the same 16 bytes when, and only when, both parties
 * used the same password-derived g. So each party computes its confirmation
 * value exactly once and reuses it both as "what I send" and "what I expect
 * to receive": no separate verification formula needed, just a byte compare.
 *
 * IMPORTANT correctness fix vs. the literal Figure 5 notation: the manual
 * writes "s = (r+m) mod p". Reducing the EXPONENT sum mod p (the field size)
 * rather than mod n=p-1 (the group ORDER) silently injects a stray factor of
 * g^{-1} into the result about half the time (whenever r+m wraps past p-1),
 * because exponents of a cyclic group only cancel correctly when combined
 * modulo the group's own order, not modulo the field it happens to live in.
 * We reduce mod n everywhere an exponent is combined, which makes k_a == k_b
 * hold on every single run, not merely ~50% of the time.
 */
#include <string.h>

#include "sae_mp.h"
#include "hash.h"
#include "auth.h"   /* prf() */

#define SAE_VERBOSE 1

static const char* a_state_name[] = { "A_IDLE", "A_WAIT_C2", "A_WAIT_C4", "A_DONE", "A_ERROR" };
static const char* b_state_name[] = { "B_IDLE", "B_WAIT_C3", "B_DONE", "B_ERROR" };

/* ================================================================== */
/* Group arithmetic: Z*_p, p = SAE_PRIME_P, order n = p-1             */
/* ================================================================== */

/* a*b mod m, overflow-free for any m < 2^63 via double-and-reduce
 * (no need for __int128: SAE_PRIME_P was picked just under 2^62, so every
 * intermediate value here stays safely inside uint64_t -- see sae_config.h). */
uint64_t sae_mulmod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t result = 0;
    a %= m;
    while (b > 0) {
        if (b & 1) {
            result += a;
            if (result >= m) result -= m;
        }
        a <<= 1;
        if (a >= m) a -= m;
        b >>= 1;
    }
    return result;
}

/* base^exp mod m, standard square-and-multiply built on sae_mulmod(). */
uint64_t sae_modpow(uint64_t base, uint64_t exp, uint64_t m) {
    uint64_t result = 1;
    base %= m;
    while (exp > 0) {
        if (exp & 1) result = sae_mulmod(result, base, m);
        base = sae_mulmod(base, base, m);
        exp >>= 1;
    }
    return result;
}

static void put_u64(uint8_t* out, uint64_t v) {   /* big-endian, network byte order */
    for (int i = 7; i >= 0; i--) { out[i] = (uint8_t)v; v >>= 8; }
}
static uint64_t get_u64(const uint8_t* in) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | in[i];
    return v;
}

/* Task 5: map the shared password w to a group element g.
 * Ascon-hash the password (prefixed with a retry counter), take the first 8
 * bytes big-endian, fold into [2, p-2]. Reject the two low-order elements
 * {1, p-1} (order 1 and 2) by re-hashing with the next counter -- for a
 * random 62-bit field this branch is astronomically unlikely to ever run,
 * but it keeps the mapping correct in principle rather than by luck. */
uint64_t sae_password_to_element(const char* password) {
    for (uint8_t counter = 0; ; counter++) {
        uint8_t buf[128];
        size_t plen = strlen(password);
        if (plen > sizeof(buf) - 1) plen = sizeof(buf) - 1;
        memcpy(buf, password, plen);
        buf[plen] = counter;          /* retry salt; 0 on the normal (first) pass */

        uint8_t digest[32];
        hash(digest, buf, plen + 1);

        uint64_t cand = get_u64(digest) % (SAE_PRIME_P - 3) + 2;   /* land in [2, p-2] */
        uint64_t sq   = sae_mulmod(cand, cand, SAE_PRIME_P);
        if (sq != 1) return cand;     /* order > 2: good generator-ish element */
        /* cand was 1 or p-1 (order <=2): astronomically rare, retry with next counter */
    }
}

/* Reduce a raw 64-bit TRNG word into a nonzero exponent in [1, n-1]. */
static uint64_t random_exponent(void) {
    uint64_t v;
    do { v = plat_random_u64() % SAE_ORDER_N; } while (v == 0);
    return v;
}

/* ================================================================== */
/* Commit / confirm computations shared by both roles                 */
/* ================================================================== */

/* Generates r,m, computes s=(r+m) mod n and t=g^-m mod p, then zeroes m
 * ("m_a = 0" in Figure 5 -- the blinding value is not needed again). */
static void make_commit(sae_party_t* p) {
    p->r_self = random_exponent();
    p->m_self = random_exponent();
    p->s_self = (p->r_self + p->m_self) % SAE_ORDER_N;          /* mod ORDER, see header note */
    uint64_t inv_m = (SAE_ORDER_N - (p->m_self % SAE_ORDER_N)) % SAE_ORDER_N;
    p->t_self = sae_modpow(p->g, inv_m, SAE_PRIME_P);           /* g^{-m} mod p */
    p->m_self = 0;                                              /* discard, as the spec requires */
}

/* K = (g^s_peer * t_peer)^r_self mod p */
static void compute_session_key(sae_party_t* p) {
    uint64_t gs = sae_modpow(p->g, p->s_peer, SAE_PRIME_P);
    uint64_t base = sae_mulmod(gs, p->t_peer, SAE_PRIME_P);
    p->session_key = sae_modpow(base, p->r_self, SAE_PRIME_P);
}

/* v = prf(KDF(K), s_A || t_A || s_B || t_B) -- canonical order: A's pair
 * always first, regardless of which role is computing it (see file header). */
static void build_confirmation(sae_party_t* p, uint8_t out_v[SAE_V_LEN]) {
    uint8_t kbuf[8], kdigest[32], key16[16];
    put_u64(kbuf, p->session_key);
    hash(kdigest, kbuf, sizeof(kbuf));       /* K -> well-distributed 16-byte PRF key */
    memcpy(key16, kdigest, 16);

    uint64_t sA, tA, sB, tB;
    if (p->role == SAE_ROLE_A) { sA = p->s_self; tA = p->t_self; sB = p->s_peer; tB = p->t_peer; }
    else                       { sA = p->s_peer; tA = p->t_peer; sB = p->s_self; tB = p->t_self; }

    uint8_t data[32];
    put_u64(data +  0, sA); put_u64(data +  8, tA);
    put_u64(data + 16, sB); put_u64(data + 24, tB);

    prf(out_v, SAE_V_LEN, data, sizeof(data), key16);
}

/* ================================================================== */
/* Wire encode / decode                                                */
/* ================================================================== */

static uint16_t sae_encode(const sae_party_t* p, sae_wire_t* w, sae_msg_type_t type) {
    memset(w, 0, sizeof(*w));
    w->magic = SAE_MAGIC;
    w->type  = (uint8_t)type;
    memcpy(w->sender, p->local_id, SAE_ID_LEN);
    if (type == SAE_MSG_C1 || type == SAE_MSG_C2) {
        put_u64(w->s, p->s_self);
        put_u64(w->t, p->t_self);
    } else {
        memcpy(w->v, p->v_self, SAE_V_LEN);
    }
    return SAE_WIRE_LEN;
}

typedef struct { sae_msg_type_t type; uint64_t s, t; uint8_t v[SAE_V_LEN]; } sae_msg_t;
typedef enum { RX_OK, RX_NOT_SAE, RX_BAD_SENDER, RX_BAD_ELEMENT } rx_result_t;

static rx_result_t sae_decode(const sae_party_t* p, const uint8_t* src_id,
                              const uint8_t* payload, uint16_t len, sae_msg_t* out) {
    if (len < SAE_WIRE_LEN) return RX_NOT_SAE;
    const sae_wire_t* w = (const sae_wire_t*)payload;
    if (w->magic != SAE_MAGIC) return RX_NOT_SAE;
    if (memcmp(src_id, p->remote_id, SAE_ID_LEN) != 0 ||
        memcmp(w->sender, p->remote_id, SAE_ID_LEN) != 0) {
        return RX_BAD_SENDER;
    }
    out->type = (sae_msg_type_t)w->type;
    if (out->type == SAE_MSG_C1 || out->type == SAE_MSG_C2) {
        out->s = get_u64(w->s);
        out->t = get_u64(w->t);
        /* both must be valid nonzero field elements mod p (defends against
         * malformed/forged frames the same way Session 6 checks its counter) */
        if (out->s == 0 || out->s >= SAE_PRIME_P || out->t == 0 || out->t >= SAE_PRIME_P) {
            return RX_BAD_ELEMENT;
        }
    } else {
        memcpy(out->v, w->v, SAE_V_LEN);
    }
    return RX_OK;
}

/* ================================================================== */
/* Logging / bookkeeping helpers                                       */
/* ================================================================== */

static void a_enter(sae_party_t* p, a_state_t s) {
    plat_log(p, 'I', "state %s -> %s", a_state_name[p->a_state], a_state_name[s]);
    p->a_state = s;
}
static void b_enter(sae_party_t* p, b_state_t s) {
    plat_log(p, 'I', "state %s -> %s", b_state_name[p->b_state], b_state_name[s]);
    p->b_state = s;
}

static void send_msg(sae_party_t* p, sae_msg_type_t type) {
    uint16_t len = sae_encode(p, &p->last_tx, type);
    plat_send(p, (const uint8_t*)&p->last_tx, len);
    const char* names[] = { "", "C1", "C2", "C3", "C4" };
    plat_log(p, 'I', "-> %s", names[type]);
}
static void resend_last(sae_party_t* p) {
    plat_send(p, (const uint8_t*)&p->last_tx, SAE_WIRE_LEN);
}

static void print_summary(sae_party_t* p, const char* outcome) {
    p->outcome = outcome;
    plat_log(p, 'I', "==================== SUMMARY ====================");
    plat_log(p, 'I', "outcome : %s", outcome);
    if (p->a_state == A_DONE || p->b_state == B_DONE) {
        uint8_t kbuf[8]; put_u64(kbuf, p->session_key);
        plat_log(p, 'I', "session_key (g^(ra*rb) mod p) = %llu",
                 (unsigned long long)p->session_key);
    }
    plat_log(p, 'I', "=================================================");
}

/* ================================================================== */
/* A: initiator FSM                                                    */
/* ================================================================== */

static void a_fsm(sae_party_t* p, uint8_t event, const sae_msg_t* m) {
    switch (p->a_state) {
    case A_IDLE:
        switch (event) {
        case EV_START:
            p->outcome = NULL;
            p->g = sae_password_to_element(p->password);
            make_commit(p);
            send_msg(p, SAE_MSG_C1);
            plat_timer_start(p);
            a_enter(p, A_WAIT_C2);
            break;
        default:
            plat_log(p, 'D', "event %u before start, ignored", event);
            break;
        }
        break;

    case A_WAIT_C2:
        switch (event) {
        case EV_FRAME:
            if (m->type != SAE_MSG_C2) {
                plat_log(p, 'W', "invalid_drop: unexpected type %u in A_WAIT_C2", m->type);
                break;
            }
            p->s_peer = m->s; p->t_peer = m->t;
            compute_session_key(p);
            build_confirmation(p, p->v_self);
            send_msg(p, SAE_MSG_C3);
            plat_timer_start(p);
            a_enter(p, A_WAIT_C4);
            break;
        case EV_TIMEOUT:
            if (++p->retries > SAE_MAX_RETRIES) {
                plat_log(p, 'E', "no reply after %d resends: B unreachable", SAE_MAX_RETRIES);
                a_enter(p, A_ERROR);
                print_summary(p, "FAILED (timeout waiting for c2)");
                break;
            }
            plat_log(p, 'W', "TIMEOUT: resending C1 (%d/%d)", p->retries, SAE_MAX_RETRIES);
            resend_last(p);
            plat_timer_start(p);
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case A_WAIT_C4:
        switch (event) {
        case EV_FRAME:
            if (m->type == SAE_MSG_C2) {
                /* B never saw our c3 (lost); resend it */
                plat_log(p, 'W', "duplicate C2: resending C3");
                resend_last(p);
                plat_timer_start(p);
                break;
            }
            if (m->type != SAE_MSG_C4) {
                plat_log(p, 'W', "invalid_drop: unexpected type %u in A_WAIT_C4", m->type);
                break;
            }
            plat_timer_stop(p);
            if (memcmp(m->v, p->v_self, SAE_V_LEN) != 0) {
                plat_log(p, 'E', "confirmation MISMATCH: wrong password on B (or forged frame)");
                a_enter(p, A_ERROR);
                print_summary(p, "FAILED (confirmation mismatch)");
                break;
            }
            plat_indicate(p);
            a_enter(p, A_DONE);
            print_summary(p, "SUCCESS");
            break;
        case EV_TIMEOUT:
            if (++p->retries > SAE_MAX_RETRIES) {
                plat_log(p, 'E', "no confirmation after %d resends: B unreachable", SAE_MAX_RETRIES);
                a_enter(p, A_ERROR);
                print_summary(p, "FAILED (timeout waiting for c4)");
                break;
            }
            plat_log(p, 'W', "TIMEOUT: resending C3 (%d/%d)", p->retries, SAE_MAX_RETRIES);
            resend_last(p);
            plat_timer_start(p);
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case A_DONE:
    case A_ERROR:
        plat_log(p, 'D', "session over (%s), event %u ignored", a_state_name[p->a_state], event);
        break;
    }
}

/* ================================================================== */
/* B: responder FSM                                                    */
/* ================================================================== */

static void b_fsm(sae_party_t* p, uint8_t event, const sae_msg_t* m) {
    switch (p->b_state) {
    case B_IDLE:
        switch (event) {
        case EV_FRAME:
            if (m->type != SAE_MSG_C1) {
                plat_log(p, 'W', "invalid_drop: unexpected type %u in B_IDLE", m->type);
                break;
            }
            p->outcome = NULL;
            p->g = sae_password_to_element(p->password);
            p->s_peer = m->s; p->t_peer = m->t;
            make_commit(p);
            send_msg(p, SAE_MSG_C2);
            plat_timer_start(p);
            b_enter(p, B_WAIT_C3);
            break;
        case EV_TIMEOUT:
            plat_log(p, 'D', "stray timeout in B_IDLE, ignored");
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case B_WAIT_C3:
        switch (event) {
        case EV_FRAME:
            if (m->type == SAE_MSG_C1) {
                /* A never saw our c2 (lost); resend it */
                plat_log(p, 'W', "duplicate C1: resending C2");
                resend_last(p);
                plat_timer_start(p);
                break;
            }
            if (m->type != SAE_MSG_C3) {
                plat_log(p, 'W', "invalid_drop: unexpected type %u in B_WAIT_C3", m->type);
                break;
            }
            compute_session_key(p);
            build_confirmation(p, p->v_self);
            plat_timer_stop(p);
            if (memcmp(m->v, p->v_self, SAE_V_LEN) != 0) {
                plat_log(p, 'E', "confirmation MISMATCH: wrong password on A (or forged frame)");
                b_enter(p, B_ERROR);
                print_summary(p, "FAILED (confirmation mismatch)");
                break;
            }
            send_msg(p, SAE_MSG_C4);
            plat_indicate(p);
            b_enter(p, B_DONE);
            print_summary(p, "SUCCESS");
            break;
        case EV_TIMEOUT:
            if (++p->retries > SAE_MAX_RETRIES) {
                plat_log(p, 'E', "no confirmation after %d resends: A unreachable", SAE_MAX_RETRIES);
                b_enter(p, B_ERROR);
                print_summary(p, "FAILED (timeout waiting for c3)");
                break;
            }
            plat_log(p, 'W', "TIMEOUT: resending C2 (%d/%d)", p->retries, SAE_MAX_RETRIES);
            resend_last(p);
            plat_timer_start(p);
            break;
        default:
            plat_log(p, 'E', "unknown event %u", event);
            break;
        }
        break;

    case B_DONE:
        switch (event) {
        case EV_FRAME:
            if (m->type == SAE_MSG_C3) {   /* A never saw our c4 (lost) */
                plat_log(p, 'W', "duplicate C3: resending C4");
                resend_last(p);
            } else {
                plat_log(p, 'D', "session over (B_DONE), frame type %u ignored", m->type);
            }
            break;
        default:
            plat_log(p, 'D', "session over (B_DONE), event %u ignored", event);
            break;
        }
        break;

    case B_ERROR:
        plat_log(p, 'D', "session over (B_ERROR), event %u ignored", event);
        break;
    }
}

/* ================================================================== */
/* Public entry points                                                 */
/* ================================================================== */

void sae_party_init(sae_party_t* p, sae_role_t role, const char* tag,
                    const uint8_t local_id[SAE_ID_LEN], const uint8_t remote_id[SAE_ID_LEN]) {
    memset(p, 0, sizeof(*p));
    p->role     = role;
    p->tag      = tag;
    p->password = SAE_PASSWORD;
    p->a_state = A_IDLE;
    p->b_state = B_IDLE;
    memcpy(p->local_id, local_id, SAE_ID_LEN);
    memcpy(p->remote_id, remote_id, SAE_ID_LEN);
}

void sae_handle_event(sae_party_t* p, uint8_t event, const uint8_t* src_id,
                      const uint8_t* payload, uint16_t len) {
    sae_msg_t m = { 0 };
    if (event == EV_FRAME) {
        rx_result_t rx = sae_decode(p, src_id, payload, len, &m);
        if (rx == RX_NOT_SAE) { plat_log(p, 'D', "ignored non-SAE frame"); return; }
        if (rx == RX_BAD_SENDER) { plat_log(p, 'W', "invalid_drop: sender is not the expected peer"); return; }
        if (rx == RX_BAD_ELEMENT) { plat_log(p, 'W', "invalid_drop: s or t out of range [1,p-1]"); return; }
    }
    if (p->role == SAE_ROLE_A) a_fsm(p, event, &m);
    else                       b_fsm(p, event, &m);
}
