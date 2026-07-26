//
// Ascon HASH256 function
//  
//
//  Created by Stig F. Mjølses on 04/04/2025.
//  Extracted and refactored from the Ascon reference implementation in https://github.com/ascon/ascon-c
//
// To be used in NTNU course TTM4138 Wireless Security
// -------------------------------------------------------

#include "defs.h"
#include "hash.h"
#include "printstate.h"

/**
 * @brief Compute the ASCON hash of a message.
 *
 * Implements the ASCON hash function using the permutation with 12 rounds.
 * The function initializes the internal state with the ASCON hash IV,
 * absorbs the input message in 64-bit blocks with padding, and then squeezes
 * the final digest of fixed size @c ASCON_HASH_SIZE.
 *
 * Processing outline:
 *  1) Initialize state s = { ASCON_HASH_IV, 0, 0, 0, 0 }; apply P12(s).
 *  2) Absorb input @p in in blocks of @c ASCON_HASH_RATE (8 bytes):
 *     - For each block: s.x[0] ^= LOADBYTES(in, 8); apply P12(s).
 *  3) Absorb the last partial block: XOR remaining bytes into s.x[0],
 *     apply padding PAD(len), then apply P12(s).
 *  4) Squeeze output of length @c ASCON_HASH_SIZE:
 *     - Store full 8-byte blocks from s.x[0], applying P12(s) between blocks.
 *     - Write the final partial block of the digest.
 *
 * @param[out] out  Output buffer for the hash digest.
 *                  Must have space for @c ASCON_HASH_SIZE bytes.
 * @param[in]  in   Input message buffer.
 * @param[in]  len  Length of the input message in bytes.
 *
 * @return 0 on success.
 *
 * @pre
 *  - Macros/functions used:
 *    - P12(&s) applies the 12-round ASCON permutation.
 *    - LOADBYTES/STOREBYTES pack/unpack up to 8 bytes.
 *    - PAD(len) produces the required padding constant.
 *  - Constants @c ASCON_HASH_IV, @c ASCON_HASH_RATE, and @c ASCON_HASH_SIZE
 *    must be defined.
 *
 * @note The hash output length is fixed by @c ASCON_HASH_SIZE (compile-time).
 * @warning This implementation is straightforward and prints nothing,
 *          but commented-out printstate/printbytes calls show intermediate
 *          values for debugging. Do not enable them in production, as they
 *          leak internal state.
 */
int hash(unsigned char* out, const unsigned char* in,
                uint64_t len) {
  // printbytes("m", in, len);
  /* initialize */
  ascon_state_t s;
  s.x[0] = ASCON_HASH_IV;
  s.x[1] = 0;
  s.x[2] = 0;
  s.x[3] = 0;
  s.x[4] = 0;
  // printstate("initial value", &s);
  P12(&s);
  // printstate("initialization", &s);

  /* absorb full plaintext blocks */
  while (len >= ASCON_HASH_RATE) {
    s.x[0] ^= LOADBYTES(in, 8);
    // printstate("absorb plaintext", &s);
    P12(&s);
    in += ASCON_HASH_RATE;
    len -= ASCON_HASH_RATE;
  }
  /* absorb final plaintext block */
  s.x[0] ^= LOADBYTES(in, (int) len);
  s.x[0] ^= PAD(len);
  // printstate("pad plaintext", &s);
  P12(&s);

  /* squeeze full output blocks */
  len = ASCON_HASH_SIZE;
  while (len > ASCON_HASH_RATE) {
    STOREBYTES(out, s.x[0], 8);
    // printstate("squeeze output", &s);
    P12(&s);
    out += ASCON_HASH_RATE;
    len -= ASCON_HASH_RATE;
  }
  /* squeeze final output block */
  STOREBYTES(out, s.x[0], (int) len);
  // printstate("squeeze output", &s);
  // printbytes("h", out + len - ASCON_HASH_SIZE, ASCON_HASH_SIZE);

  return 0;
}
