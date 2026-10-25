/*
 * Session 9 — MP-SAE (Protocol B, Figure 5): shared configuration for both boards.
 *
 * Both boards are flashed with the SAME binary; each picks its role (A or B) by
 * comparing its own MAC address against the two strings below, exactly like
 * Session 6's P2P_INITIATOR_MAC / P2P_RESPONDER_MAC. Despite the protocol's name
 * ("Simultaneous" Authentication of Equals), the message flow in Figure 5 is a
 * strict 4-step lockstep: A sends c1, B replies c2, A replies c3, B replies c4.
 * So A plays the initiator role and B the responder role, same as I/R in P2P.
 */
#pragma once

#include <stdint.h>

/* ---- (0) Pre-shared: a_A, a_B identities, and the MODP group ---- */
#define SAE_A_MAC "48:f6:ee:c7:1f:0c"   
#define SAE_B_MAC "48:f6:ee:c7:20:f0"  

/*
 * Safe prime p = 2q+1 (both p and q prime), generated offline and hardcoded
 * here exactly as the lab's own example does (p = 15116998319914373459) --
 * see Task 5 / README for how it was generated and verified with Miller-Rabin.
 * Chosen just under 2^62 so that a*b for a,b < p never overflows a uint64_t
 * even with plain double-and-reduce multiplication -- no 128-bit arithmetic
 * needed anywhere in this file.
 */
#ifndef SAE_PRIME_P
#define SAE_PRIME_P   4270378237766580047ull   /* 62-bit safe prime  */
#endif
#define SAE_ORDER_N   (SAE_PRIME_P - 1)        /* order of Z*_p = 2q */

/* ---- The shared "password" w. Hardcoded as a constant, as the task allows
 * (manual/offline key sharing). Mapped to a group element g with
 * sae_password_to_element() in sae_mp.c -- see Task 5. */
#define SAE_PASSWORD "TTM4138-SAE-Session9"

/* ---- Timing (same pattern/values as Session 6) ---- */
#ifndef SAE_TIMEOUT_MS
#define SAE_TIMEOUT_MS   2000   /* resend after this long without a reply   */
#endif
#ifndef SAE_MAX_RETRIES
#define SAE_MAX_RETRIES  8      /* give up after this many resends          */
#endif
