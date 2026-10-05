/*
 * Session 6 — shared configuration for both boards.
 *
 * Both boards are flashed with the SAME binary; each one picks its role
 * (initiator I or responder R) by comparing its own MAC address against the
 * two strings below. On first boot the log prints "This board's MAC is ..." —
 * copy those values in here, rebuild and flash both boards.
 *
 * The numeric settings are wrapped in #ifndef so the laptop simulator
 * (Prosim/p2p) can override them on the compiler command line, e.g. -DP2P_T=1000.
 */
#pragma once

#include <stdint.h>

/* ---- (0) Pre-shared constants: identities, n0, T ---- */
#define P2P_INITIATOR_MAC "48:f6:ee:c7:1f:18"  // Dipankar
#define P2P_RESPONDER_MAC "48:f6:ee:c7:20:f0"  // Subu

#ifndef P2P_N0
#define P2P_N0            0x3A7F12C4u  /* pre-shared random start value n0 */
#endif
#ifndef P2P_T
#define P2P_T             5            /* number of ping-pong rounds (try 10000 too) */
#endif

/* ---- Timing ---- */
#ifndef P2P_TIMEOUT_MS
#define P2P_TIMEOUT_MS              2000  /* I: resend after this long without a reply */
#endif
#ifndef P2P_MAX_RETRIES
#define P2P_MAX_RETRIES             8     /* I: give up after this many resends */
#endif
/* R: abandon a session after this many timeouts of silence. It must outlast
 * I's whole retry window ((MAX_RETRIES + 1) x TIMEOUT), otherwise R gives up
 * while I is still resending and the session dies with an ERROR (found with
 * the simulator: 15 of 1000 runs at 20% loss failed this way with a value of 5). */
#ifndef P2P_RESPONDER_IDLE_TIMEOUTS
#define P2P_RESPONDER_IDLE_TIMEOUTS (P2P_MAX_RETRIES + 2)
#endif
#ifndef P2P_ROUND_DELAY_MS
#define P2P_ROUND_DELAY_MS          500   /* I: pause between rounds (human speed); 0 = full speed */
#endif

/* ---- Task 7: Ascon-AEAD128 on/off ----
 * 1 = counter is encrypted and every frame carries a 16-byte tag.
 * 0 = plaintext (same frame layout, zero nonce, no tag) for the Wireshark
 *     comparison in task 9. */
#ifndef P2P_USE_AEAD
#define P2P_USE_AEAD 1
#endif

/* 128-bit pre-shared key, hard-coded as the task allows. Demo only. */
static const uint8_t P2P_KEY[16] = {
    0x54, 0x54, 0x4d, 0x34, 0x31, 0x33, 0x38, 0x2d,
    0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x36,
};

/* ---- Checkpoint 4: fault injection (0 = off) ----
 * LOSS:    the sender silently skips every Nth transmission (lost frame)
 *          -> exercises the timeout / resend / duplicate path.
 * CORRUPT: the initiator flips one byte in every Nth frame it sends
 *          -> exercises tag verification (AEAD) or counter checks (plaintext). */
#ifndef P2P_SIMULATE_LOSS_EVERY
#define P2P_SIMULATE_LOSS_EVERY    0
#endif
#ifndef P2P_SIMULATE_CORRUPT_EVERY
#define P2P_SIMULATE_CORRUPT_EVERY 0
#endif
