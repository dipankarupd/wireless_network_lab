#include "defs.h"
#include "auth.h"
#include "printstate.h"

/**
 * @brief ASCON-based pseudorandom function (PRF).
 *
 * Derives a pseudorandom output from a secret key and input message using
 * the ASCON permutation. This construction is suitable for MACs, KDFs,
 * or other primitives that require a fixed-length PRF output.
 *
 * Algorithm outline:
 *  1. **Key load:** Interpret @p k as two 64-bit words (K0, K1).
 *  2. **Initialization:** Initialize state
 *     s = {ASCON_MAC_IV, K0, K1, 0, 0}, then apply P12(s).
 *  3. **Absorption:** XOR input @p in into state words in 8-byte chunks
 *     (cycled across s.x[0..3]). Apply P12(s) every 4 full words.
 *     For the remaining bytes, XOR them into the current word, apply
 *     padding PAD(inlen), and apply domain separation with DSEP().
 *  4. **Squeezing:** Apply P12(s), then extract 8-byte words from
 *     s.x[0..1] in alternation. Apply P12(s) after every two blocks
 *     until @p outlen bytes are produced. The last block may be partial.
 *
 * @param[out] out     Output buffer of length @p outlen.
 * @param[in]  outlen  Number of bytes to generate. Must be ≤ @c CRYPTO_BYTES.
 * @param[in]  in      Input message buffer to absorb.
 * @param[in]  inlen   Length of @p in in bytes.
 * @param[in]  k       Secret key buffer (16 bytes, 128 bits).
 *
 * @return 0 on success, or -1 if @p outlen > @c CRYPTO_BYTES.
 *
 * @pre
 *  - @c CRYPTO_BYTES defines the maximum allowed output length.
 *  - Macros @c LOADBYTES, @c STOREBYTES, @c PAD, @c DSEP, and function
 *    @c P12() must be defined consistently with the ASCON specification.
 *  - The type @c ascon_state_t contains 5×64-bit words (s.x[0..4]).
 *
 * @note Input and output buffers must not overlap.
 * @warning This implementation is for reference and includes conditional
 *          permutation calls; production implementations should ensure
 *          constant-time behavior and disable debug printing.
 */
int prf(unsigned char* out, unsigned long long outlen,
               const unsigned char* in, unsigned long long inlen,
               const unsigned char* k) {
  if (CRYPTO_BYTES && outlen > CRYPTO_BYTES) return -1;
  /* load key */
  const uint64_t K0 = LOADBYTES(k, 8);
  const uint64_t K1 = LOADBYTES(k + 8, 8);
  int i;
  // printbytes("k", k, CRYPTO_KEYBYTES);
  // printbytes("m", in, inlen);
  /* initialize */
  ascon_state_t s;
  s.x[0] = ASCON_MAC_IV;
  s.x[1] = K0;
  s.x[2] = K1;
  s.x[3] = 0;
  s.x[4] = 0;
  // printstate("initial value", &s);
  P12(&s);
  // printstate("initialization", &s);

  /* absorb full plaintext words */
  i = 0;
  while (inlen >= 8) {
    ((uint64_t*)(&s.x[0]))[i] ^= LOADBYTES(in, 8);
    if (++i == 4) i = 0;
    // if (i == 0) printstate("absorb plaintext", &s);
    if (i == 0) P12(&s);
    in += 8;
    inlen -= 8;
  }
  /* absorb final plaintext word */
  ((uint64_t*)(&s.x[0]))[i] ^= LOADBYTES(in, (int) inlen);
  ((uint64_t*)(&s.x[0]))[i] ^= PAD(inlen);
  // printstate("pad plaintext", &s);
  /* domain separation */
  s.x[4] ^= DSEP();
  // printstate("domain separation", &s);

  /* squeeze */
  P12(&s);
  /* squeeze output words */
  i = 0;
  while (outlen > 8) {
    STOREBYTES(out, ((uint64_t*)(&s.x[0]))[i], 8);
    if (++i == 2) i = 0;
    // if (i == 0) printstate("squeeze output", &s);
    if (i == 0) P12(&s);
    out += 8;
    outlen -= 8;
  }
  /* squeeze final output word */
  STOREBYTES(out, ((uint64_t*)(&s.x[0]))[i], (int) outlen);
  // printstate("squeeze output", &s);
  // printbytes("t", out, CRYPTO_BYTES);
  // print("\n");
  return 0;
}

/**
 * @brief Authentication function using the ASCON-based PRF.
 *
 * Computes a fixed-length authentication tag for the input message by
 * invoking the ASCON-based pseudorandom function (PRF) with the given key.
 * The tag length is defined by the compile-time constant @c CRYPTO_BYTES.
 *
 * Processing outline:
 *  1) Load the 128-bit key @p k.
 *  2) Call @ref prf() with the message @p in, its length @p len, and the key.
 *  3) Write the resulting authentication tag of length @c CRYPTO_BYTES to
 *     @p out.
 *
 * @param[out] out  Output buffer for the authentication tag.
 *                  Must have space for @c CRYPTO_BYTES bytes.
 * @param[in]  in   Input message buffer to authenticate.
 * @param[in]  len  Length of the input message in bytes.
 * @param[in]  k    Secret key buffer (16 bytes, 128 bits).
 *
 * @return 0 on success; -1 if PRF fails (e.g., if @c CRYPTO_BYTES is
 *         exceeded internally).
 *
 * @note This is a thin wrapper around @ref prf() with @p outlen set to
 *       @c CRYPTO_BYTES. The security level is defined by that constant.
 * @warning The output length is fixed; truncating or extending the tag
 *          outside this function is not supported.
 */
int auth(unsigned char* out, const unsigned char* in,
                unsigned long long len, const unsigned char* k) {
  return prf(out, CRYPTO_BYTES, in, len, k);
}

/**
 * @brief Verify a message authentication tag using the ASCON-based PRF.
 *
 * Recomputes the authentication tag for the input message with the provided
 * key and compares it in constant time against the expected tag @p h.
 *
 * Processing outline:
 *  1) Call @ref prf() with the message @p in, its length @p len, and the key
 *     @p k to generate a candidate tag into a local buffer.
 *  2) Compute the bitwise XOR of each byte of @p h and the recomputed tag.
 *     Accumulate the XORs into @c diff.
 *  3) Use arithmetic on @c diff to return a constant-time equality result.
 *
 * @param[in] h    Expected authentication tag (CRYPTO_BYTES bytes).
 * @param[in] in   Input message buffer to authenticate.
 * @param[in] len  Length of the input message in bytes.
 * @param[in] k    Secret key buffer (16 bytes, 128 bits).
 *
 * @return 0 if the tag matches (verification successful),
 *        -1 if the tag does not match.
 *
 * @note Comparison is performed in constant time to avoid leaking timing
 *       information about the tag contents.
 * @warning The output length is fixed by @c CRYPTO_BYTES; both @p h and the
 *          recomputed tag must be exactly this length.
 */
int auth_verify(const unsigned char* h, const unsigned char* in,
                       unsigned long long len, const unsigned char* k) {
  int i;
  uint8_t diff = 0;
  uint8_t tag[CRYPTO_BYTES];
  prf(tag, CRYPTO_BYTES, in, len, k);
  for (i = 0; i < CRYPTO_BYTES; ++i) diff |= h[i] ^ tag[i];
  return (1 & ((diff - 1) >> 8)) - 1;
}
