/*
 * Session 9 — host-side self-test for MP-SAE, before ever touching the boards.
 *
 * Compiles sae_mp.c as-is (the SAME file that runs on the ESP32) with host
 * implementations of the plat_* functions: delivery is a direct in-process
 * function call instead of a radio, so there is no loss/timeout to simulate
 * here -- this harness exists to prove the GROUP MATH and the FSM logic are
 * correct, not to re-test robustness (Session 6 already covers that pattern
 * on real hardware).
 *
 * Three things are checked:
 *   1. one full run, logged, so you can show/paste it as evidence
 *   2. N repeated runs with fresh randomness each time, asserting every run
 *      reaches SUCCESS with A and B computing the identical session key --
 *      this is the concrete proof that reducing exponents mod n (not mod p,
 *      see the long comment in sae_mp.c) makes K_a==K_b on EVERY run, not
 *      only about half the time as the literal "mod p" reading would give
 *   3. a deliberate password mismatch between A and B, confirming the
 *      confirmation step correctly rejects it instead of silently accepting
 *
 * Build:
 *   cc -O2 -I../../components/auth/include -I../../components/hash/include \
 *      -o host_test host_test.c sae_mp.c \
 *      ../../components/auth/prf.c ../../components/hash/hash.c
 * Run:  ./host_test
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sae_mp.h"

static sae_party_t *g_a, *g_b;   /* the pair currently wired together */
static int g_verbose = 1;

/* A small FIFO stands in for the RTOS queue the ESP32 build uses (session6's
 * fsm_event_queue). plat_send() only ENQUEUES; it must not call the peer's
 * handler directly -- a direct call would let B's reply to A's frame race
 * back into A before A has finished updating its own state from sending
 * that very frame, which is not how the real (queued) system behaves. */
typedef struct { sae_party_t* target; uint8_t event; uint8_t src[SAE_ID_LEN]; uint8_t data[SAE_WIRE_LEN]; uint16_t len; } qitem_t;
static qitem_t g_queue[32];
static int g_qhead, g_qtail;

static void q_push(sae_party_t* target, uint8_t event, const uint8_t* src, const uint8_t* data, uint16_t len) {
    qitem_t* it = &g_queue[g_qtail++];
    it->target = target; it->event = event;
    if (src) memcpy(it->src, src, SAE_ID_LEN);
    if (data && len) memcpy(it->data, data, len);
    it->len = len;
}
static void q_drain(void) {
    while (g_qhead < g_qtail) {
        qitem_t* it = &g_queue[g_qhead++];
        sae_handle_event(it->target, it->event, it->src, it->data, it->len);
    }
    g_qhead = g_qtail = 0;
}

/* ---- plat_* : host implementations ---- */

void plat_send(sae_party_t* p, const uint8_t* data, uint16_t len) {
    sae_party_t* peer = (p == g_a) ? g_b : g_a;
    q_push(peer, EV_FRAME, p->local_id, data, len);
}
void plat_timer_start(sae_party_t* p) { (void)p; /* no timeouts in this deterministic harness */ }
void plat_timer_stop(sae_party_t* p)  { (void)p; }
void plat_indicate(sae_party_t* p)    { if (g_verbose) printf("  (%s: LED would change)\n", p->tag); }

uint64_t plat_random_u64(void) {
    /* host TRNG stand-in: two arc4random() words combined (macOS/BSD libc).
     * The ESP32 build uses the real hardware TRNG instead -- see sae_protocol.c. */
    uint64_t hi = arc4random();
    uint64_t lo = arc4random();
    return (hi << 32) | lo;
}

void plat_log(const sae_party_t* p, char level, const char* fmt, ...) {
    if (!g_verbose) return;
    va_list args; va_start(args, fmt);
    printf("[%s %c] ", p->tag, level);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/* ---- test driver ---- */

static const uint8_t MAC_A[SAE_ID_LEN] = {0x02,0,0,0,0,0x01};
static const uint8_t MAC_B[SAE_ID_LEN] = {0x02,0,0,0,0,0x02};

/* Runs one full exchange. Returns 1 if both sides reached SUCCESS with equal
 * session keys, 0 otherwise. */
static int run_once(const char* pw_a, const char* pw_b, int verbose) {
    sae_party_t a, b;
    sae_party_init(&a, SAE_ROLE_A, "A", MAC_A, MAC_B);
    sae_party_init(&b, SAE_ROLE_B, "B", MAC_B, MAC_A);
    a.password = pw_a;
    b.password = pw_b;
    g_a = &a; g_b = &b;
    g_verbose = verbose;

    q_push(&a, EV_START, NULL, NULL, 0);
    q_drain();                                        /* breadth-first delivery, exactly like
                                                        * draining an RTOS queue in a loop     */

    int ok = (a.a_state == A_DONE) && (b.b_state == B_DONE) &&
             (a.session_key == b.session_key) &&
             (memcmp(a.v_self, b.v_self, SAE_V_LEN) == 0);
    return ok;
}

int main(void) {
    printf("=== 1) one full run (verbose) ===\n");
    int ok = run_once(SAE_PASSWORD, SAE_PASSWORD, 1);
    printf(ok ? "\n--> SUCCESS, K_a == K_b\n\n" : "\n--> FAILED (unexpected)\n\n");
    if (!ok) return 1;
    printf("group parameters:\n");
    printf("  p (safe prime) = %llu\n", (unsigned long long)SAE_PRIME_P);
    printf("  n = p-1        = %llu\n", (unsigned long long)SAE_ORDER_N);
    printf("  password       = \"%s\"\n", SAE_PASSWORD);
    printf("  g (password -> element, Task 5) = %llu\n\n", (unsigned long long)sae_password_to_element(SAE_PASSWORD));

    printf("=== 2) %d repeated runs, fresh randomness each time ===\n", 50);
    int pass = 0;
    for (int i = 0; i < 50; i++) pass += run_once(SAE_PASSWORD, SAE_PASSWORD, 0);
    printf("%d / 50 runs reached SUCCESS with matching session keys\n\n", pass);
    if (pass != 50) { printf("--> FAILED: the mod-n fix is not holding on every run\n"); return 1; }

    printf("=== 3) negative test: A and B use DIFFERENT passwords ===\n");
    sae_party_t a, b;
    sae_party_init(&a, SAE_ROLE_A, "A", MAC_A, MAC_B);
    sae_party_init(&b, SAE_ROLE_B, "B", MAC_B, MAC_A);
    a.password = SAE_PASSWORD;
    b.password = SAE_PASSWORD "-WRONG";
    g_a = &a; g_b = &b; g_verbose = 1;
    q_push(&a, EV_START, NULL, NULL, 0);
    q_drain();
    /* B must detect the mismatch and refuse to confirm (no c4 sent), and A --
     * which only hears back from B -- must never reach a false A_DONE. On
     * real hardware A then times out into A_ERROR after its retries (Session
     * 6 already exercises that timeout path on actual boards); this harness
     * has no timers, so A is left waiting, which is the correct, safe state
     * to be stuck in: not a false accept. */
    int rejected = (b.b_state == B_ERROR) && (a.a_state != A_DONE);
    printf(rejected ? "\n--> correctly REJECTED (B refused to confirm; A never falsely succeeded)\n"
                    : "\n--> FAILED: wrong password was accepted!\n");

    return (pass == 50 && rejected) ? 0 : 1;
}
