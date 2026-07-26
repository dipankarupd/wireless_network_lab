//
// Ascon header file
//  Definitions for ASCON modes AEAD128, HASH256, XOF128
//
//  Created by Stig F. Mjølses on 04/04/2025.
//  Extracted and refactored from the Ascon reference implementation in https://github.com/ascon/ascon-c
//
// To be used in NTNU course TTM4138 Wireless Security
// -------------------------------------------------------

#pragma once

#include <stdint.h>
// #include "printstates.h"


// Identifier for the algorithm variant
#define ASCON_80PQ_VARIANT 0
#define ASCON_AEAD_VARIANT 1
#define ASCON_HASH_VARIANT 2
#define ASCON_XOF_VARIANT 3
#define ASCON_CXOF_VARIANT 4
#define ASCON_MAC_VARIANT 5
#define ASCON_PRF_VARIANT 6
#define ASCON_PRFS_VARIANT 7


/* AEAD crypto */

#define CRYPTO_KEYBYTES 16
#define CRYPTO_NSECBYTES 0
#define CRYPTO_NPUBBYTES 16
#define CRYPTO_ABYTES 16
#define CRYPTO_NOOVERLAP 1
#define ASCON_AEAD_RATE 16

#define ASCON_128_PA_ROUNDS 12  // Init and final number of rounds
#define ASCON_128_PB_ROUNDS 8  //During processing of AD, plaintext/ciphertext
#define ASCON_128_RATE 16  // Number of text input bytes per permutation
#define ASCON_TAG_SIZE 16  // 128 bits authentication code
#define CRYPTO_BYTES 32  //

// Parameters into the IV according to NISTstandard appendix B.
// Ascon-AEAD256    '0x00001000808c0001'
#define ASCON_128_IV                         \
  (((uint64_t)(ASCON_AEAD_VARIANT) << 0) |    \
   ((uint64_t)(ASCON_128_PA_ROUNDS) << 16) |  \
   ((uint64_t)(ASCON_128_PB_ROUNDS) << 20) | \
   ((uint64_t)(ASCON_TAG_SIZE * 8) << 24) |   \
   ((uint64_t)(ASCON_128_RATE) << 40))


/* HASH */
#define ASCON_HASH_PA_ROUNDS 12  // Init and final number
#define ASCON_HASH_PB_ROUNDS 12 //During processing
#define ASCON_HASH_SIZE 32  // 256 bits hash value
#define ASCON_HASH_RATE 8   // Number of text input bytes per permutation

// Parameters into the IV according to NISTstandard appendix B.
// Ascon-Hash256    '0x0000080100cc0002'
#define ASCON_HASH_IV                         \
  (((uint64_t)(ASCON_HASH_VARIANT) << 0) |    \
   ((uint64_t)(ASCON_HASH_PA_ROUNDS) << 16) |      \
   ((uint64_t)(ASCON_HASH_PB_ROUNDS) << 20) | \
   ((uint64_t)(ASCON_HASH_SIZE * 8) << 24) |  \
   ((uint64_t)(ASCON_HASH_RATE) << 40))



/* XOF128 Extendable output function */
#define ASCON_PRF_PA_ROUNDS 12
#define ASCON_PRF_PB_ROUNDS 12
#define ASCON_PRF_IN_RATE 8
#define ASCON_PRF_OUT_RATE 8

// Parameters into the IV according to NISTstandard appendix B.
// Ascon-XOF128    '0x0000080000cc0003'
#define ASCON_XOF_IV                          \
  (((uint64_t)(ASCON_XOF_VARIANT) << 0) |     \
   ((uint64_t)(ASCON_PRF_PA_ROUNDS) << 16) |      \
   ((uint64_t)(ASCON_PRF_PB_ROUNDS) << 20) | \
   ((uint64_t)(ASCON_PRF_IN_RATE) << 40))

// The 4 internal Ascon 64 bit state registers are enclosed in a struct type
typedef struct {
  uint64_t x[5];
} ascon_state_t;

// Byte-level functions ----
/* get byte from 64-bit Ascon word */
#define GETBYTE(x, i) ((uint8_t)((uint64_t)(x) >> (8 * (i))))

/* set byte in 64-bit Ascon word */
#define SETBYTE(b, i) ((uint64_t)(b) << (8 * (i)))

/* set padding byte in 64-bit Ascon word */
#define PAD(i) SETBYTE(0x01, i)

/* define domain separation bit in 64-bit Ascon word */
#define DSEP() SETBYTE(0x80, 7)

/* load bytes into 64-bit Ascon word */
static inline uint64_t LOADBYTES(const uint8_t* bytes, int n) {
  int i;
  uint64_t x = 0;
  for (i = 0; i < n; ++i) x |= SETBYTE(bytes[i], i);
  return x;
}

/* store bytes from 64-bit Ascon word */
static inline void STOREBYTES(uint8_t* bytes, uint64_t x, int n) {
  int i;
  for (i = 0; i < n; ++i) bytes[i] = GETBYTE(x, i);
}

/* clear bytes in 64-bit Ascon word */
static inline uint64_t CLEARBYTES(uint64_t x, int n) {
  int i;
  for (i = 0; i < n; ++i) x &= ~SETBYTE(0xff, i);
  return x;
}


// Ascon PERMUTATION in 3 stages

// Rotational right shift function
static inline uint64_t ROR(uint64_t x, int n) {
  return x >> n | x << (-n & 63);
}

static inline void ROUND(ascon_state_t* s, uint8_t C) {
  ascon_state_t t;
  /* 1. addition of round constant */
  s->x[2] ^= C;
  /* printstate(" round constant", s); */
    
  /* 2. substitution layer */
  s->x[0] ^= s->x[4];
  s->x[4] ^= s->x[3];
  s->x[2] ^= s->x[1];
  /* start of keccak s-box */
  t.x[0] = s->x[0] ^ (~s->x[1] & s->x[2]);
  t.x[1] = s->x[1] ^ (~s->x[2] & s->x[3]);
  t.x[2] = s->x[2] ^ (~s->x[3] & s->x[4]);
  t.x[3] = s->x[3] ^ (~s->x[4] & s->x[0]);
  t.x[4] = s->x[4] ^ (~s->x[0] & s->x[1]);
  /* end of keccak s-box */
  t.x[1] ^= t.x[0];
  t.x[0] ^= t.x[4];
  t.x[3] ^= t.x[2];
  t.x[2] = ~t.x[2];
  /* printstate(" substitution layer", &t); */
  
    /* 3. linear diffusion layer */
  s->x[0] = t.x[0] ^ ROR(t.x[0], 19) ^ ROR(t.x[0], 28);
  s->x[1] = t.x[1] ^ ROR(t.x[1], 61) ^ ROR(t.x[1], 39);
  s->x[2] = t.x[2] ^ ROR(t.x[2], 1) ^ ROR(t.x[2], 6);
  s->x[3] = t.x[3] ^ ROR(t.x[3], 10) ^ ROR(t.x[3], 17);
  s->x[4] = t.x[4] ^ ROR(t.x[4], 7) ^ ROR(t.x[4], 41);
  /* printstate(" round output", s); */
}


// Permutation ROUNDs  ----

static inline void P12(ascon_state_t* s) {
  ROUND(s, 0xf0);
  ROUND(s, 0xe1);
  ROUND(s, 0xd2);
  ROUND(s, 0xc3);
  ROUND(s, 0xb4);
  ROUND(s, 0xa5);
  ROUND(s, 0x96);
  ROUND(s, 0x87);
  ROUND(s, 0x78);
  ROUND(s, 0x69);
  ROUND(s, 0x5a);
  ROUND(s, 0x4b);
}

static inline void P8(ascon_state_t* s) {
  ROUND(s, 0xb4);
  ROUND(s, 0xa5);
  ROUND(s, 0x96);
  ROUND(s, 0x87);
  ROUND(s, 0x78);
  ROUND(s, 0x69);
  ROUND(s, 0x5a);
  ROUND(s, 0x4b);
}

static inline void P6(ascon_state_t* s) {
  ROUND(s, 0x96);
  ROUND(s, 0x87);
  ROUND(s, 0x78);
  ROUND(s, 0x69);
  ROUND(s, 0x5a);
  ROUND(s, 0x4b);
}


