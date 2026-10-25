/*
 * Session 9 — MP-SAE (Protocol B, Figure 5): platform-independent protocol core.
 *
 * sae_mp.c holds the group arithmetic and the two FSMs (A and B). The SAME file
 * is compiled both for the ESP32-C6 (sae_protocol.c supplies the plat_* glue)
 * and for the laptop (host_test.c supplies it), mirroring Session 6's
 * p2p_fsm.c / p2p_protocol.c split.
 *
 * Wire format (payload after the 802.11 header + LLC/SNAP):
 *   magic(1) type(1) sender[6] s(8,BE) t(8,BE) v(16)
 * c1/c2 use s,t (v = 0); c3/c4 use v (s,t = 0). Fixed 40-byte frame for all
 * four messages keeps the code and the Wireshark filter simple.
 */
#pragma once

#include <stdint.h>
#include "sae_config.h"

#define SAE_ID_LEN   6
#define SAE_MAGIC    0x9A
#define SAE_S_LEN    8
#define SAE_T_LEN    8
#define SAE_V_LEN    16   /* prf() output length used as the confirmation value */

typedef enum { SAE_MSG_C1 = 1, SAE_MSG_C2 = 2, SAE_MSG_C3 = 3, SAE_MSG_C4 = 4 } sae_msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t type;
    uint8_t sender[SAE_ID_LEN];
    uint8_t s[SAE_S_LEN];
    uint8_t t[SAE_T_LEN];
    uint8_t v[SAE_V_LEN];
} sae_wire_t;
#define SAE_WIRE_LEN (sizeof(sae_wire_t))

enum { EV_TIMEOUT = 0, EV_FRAME = 1, EV_START = 2 };   /* same convention as Session 6 */

typedef enum { SAE_ROLE_A, SAE_ROLE_B } sae_role_t;

/* A: IDLE -> WAIT_C2 -> WAIT_C4 -> DONE/ERROR
 * B: IDLE -> WAIT_C3 -> DONE/ERROR                       (Figure 5's lockstep order) */
typedef enum { A_IDLE, A_WAIT_C2, A_WAIT_C4, A_DONE, A_ERROR } a_state_t;
typedef enum { B_IDLE, B_WAIT_C3, B_DONE, B_ERROR } b_state_t;

typedef struct {
    sae_role_t role;
    const char* tag;
    const char* password;    /* defaults to SAE_PASSWORD in sae_party_init() */
    uint8_t     local_id[SAE_ID_LEN];
    uint8_t     remote_id[SAE_ID_LEN];

    a_state_t a_state;
    b_state_t b_state;

    uint64_t g;              /* password-derived group element (Task 5)      */
    uint64_t r_self, m_self; /* my secret blinding values for this run       */
    uint64_t s_self, t_self; /* my commit (c1 or c2)                         */
    uint64_t s_peer, t_peer; /* peer's commit, once received                 */
    uint64_t session_key;    /* k = g^(r_self*r_peer) mod p, once computed   */
    uint8_t  v_self[SAE_V_LEN];
    uint8_t  v_peer[SAE_V_LEN];

    int      retries;
    sae_wire_t last_tx;
    const char* outcome;     /* NULL while running */
} sae_party_t;

void sae_party_init(sae_party_t* p, sae_role_t role, const char* tag,
                    const uint8_t local_id[SAE_ID_LEN], const uint8_t remote_id[SAE_ID_LEN]);
void sae_handle_event(sae_party_t* p, uint8_t event, const uint8_t* src_id,
                      const uint8_t* payload, uint16_t len);

/* ---- Group arithmetic (Z*_p, p = SAE_PRIME_P), exposed for the host self-test ---- */
uint64_t sae_mulmod(uint64_t a, uint64_t b, uint64_t m);
uint64_t sae_modpow(uint64_t base, uint64_t exp, uint64_t m);
uint64_t sae_password_to_element(const char* password); /* Task 5: w -> g */

/* ---- Platform layer: implemented once for the ESP32, once for the host test ---- */
void     plat_send(sae_party_t* p, const uint8_t* data, uint16_t len);
void     plat_timer_start(sae_party_t* p);
void     plat_timer_stop(sae_party_t* p);
void     plat_indicate(sae_party_t* p);
uint64_t plat_random_u64(void);
void     plat_log(const sae_party_t* p, char level, const char* fmt, ...)
             __attribute__((format(printf, 3, 4)));
