

#include "defs.h"
#include "printstate.h"
#include "aead.h"

/**
 * @brief ASCON-128 authenticated encryption with associated data (AEAD).
 *
 * Encrypts a plaintext message @p m with key @p k and public nonce @p npub,
 * authenticates both the ciphertext and associated data @p ad, and produces
 * ciphertext @p c consisting of the encrypted message followed by a 16-byte
 * authentication tag.
 *
 * Algorithm outline (ASCON-128 AEAD, 12/8 round variant):
 *  1. **Initialization:**
 *     - Load key (K0,K1) and nonce (N0,N1) into the state with IV.
 *     - Apply 12-round permutation P12, then XOR key into state again.
 *  2. **Associated data absorption:**
 *     - Absorb full 16-byte blocks of associated data into (x0,x1), apply P8().
 *     - Absorb final partial block with padding PAD(len), apply P8().
 *     - Apply domain separation via s.x[4] ^= DSEP().
 *  3. **Plaintext encryption:**
 *     - For each full 16-byte block: XOR into (x0,x1), output ciphertext block,
 *       apply P8().
 *     - For final partial block: absorb into (x0,x1), output ciphertext bytes,
 *       apply padding.
 *  4. **Finalization and tag:**
 *     - XOR key words into state, apply P12, XOR key again.
 *     - Output 16-byte tag from (x3,x4), appended to ciphertext.
 *
 * @param[out] c     Ciphertext output buffer of length @p mlen + CRYPTO_ABYTES.
 *                   Contains ciphertext followed by authentication tag.
 * @param[out] clen  Pointer to length of ciphertext output. On return,
 *                   set to @p mlen + CRYPTO_ABYTES.
 * @param[in]  m     Plaintext message buffer to encrypt.
 * @param[in]  mlen  Length of plaintext message in bytes.
 * @param[in]  ad    Associated data buffer to authenticate but not encrypt.
 * @param[in]  adlen Length of associated data in bytes.
 * @param[in]  npub  Public nonce (CRYPTO_NPUBBYTES = 16 bytes).
 * @param[in]  k     Secret key (CRYPTO_KEYBYTES = 16 bytes).
 *
 * @return 0 on success.
 *
 * @pre
 *  - Macros/functions @c LOADBYTES, @c STOREBYTES, @c PAD, @c DSEP,
 *    @c P12(), and @c P8() must be defined according to the ASCON spec.
 *  - @c CRYPTO_ABYTES must equal 16 for ASCON-128.
 *
 * @note The ciphertext buffer @p c must have space for @p mlen + CRYPTO_ABYTES
 *       bytes. Input and output buffers must not overlap.
 * @note This implementation follows the ASCON-128 AEAD reference design
 *       (12 initialization/finalization rounds, 8 absorption/encryption rounds).
 * @warning Printing of sensitive values (keys, state, ciphertext) is disabled
 *          by default; do not enable in production.
 */
int ascon_aead_encrypt(unsigned char* c, unsigned long long* clen,
                        const unsigned char* m, unsigned long long mlen,
                        const unsigned char* ad, unsigned long long adlen,
                        const unsigned char* npub,
                        const unsigned char* k) {

  /* set ciphertext size */
  *clen = mlen + CRYPTO_ABYTES;

  /* print input bytes */
  // print("encrypt\n");
  // printbytes("k", k, CRYPTO_KEYBYTES);
  // printbytes("n", npub, CRYPTO_NPUBBYTES);
  // printbytes("a", ad, adlen);
  // printbytes("m", m, mlen);

  /* load key and nonce */
  const uint64_t K0 = LOADBYTES(k, 8);
  const uint64_t K1 = LOADBYTES(k + 8, 8);
  const uint64_t N0 = LOADBYTES(npub, 8);
  const uint64_t N1 = LOADBYTES(npub + 8, 8);

  /* initialize */
  ascon_state_t s;
  s.x[0] = ASCON_128_IV;
  s.x[1] = K0;
  s.x[2] = K1;
  s.x[3] = N0;
  s.x[4] = N1;
  // printstate("init 1st key xor", &s);
  P12(&s);
  s.x[3] ^= K0;
  s.x[4] ^= K1;
  // printstate("init 2nd key xor", &s);

  if (adlen) {
    /* full associated data blocks */
    while (adlen >= ASCON_128_RATE) {
      s.x[0] ^= LOADBYTES(ad, 8);
      s.x[1] ^= LOADBYTES(ad + 8, 8);
      // printstate("absorb adata", &s);
      P8(&s);
      ad += ASCON_128_RATE;
      adlen -= ASCON_128_RATE;
    }
    /* final associated data block */
    if (adlen >= 8) {
      s.x[0] ^= LOADBYTES(ad, 8);
        s.x[1] ^= LOADBYTES(ad + 8, (int)(adlen - 8));
      s.x[1] ^= PAD(adlen - 8);
    } else {
      s.x[0] ^= LOADBYTES(ad, (int) adlen);
      s.x[0] ^= PAD(adlen);
    }
    // printstate("pad adata", &s);
    P8(&s);
  }
  /* domain separation */
  s.x[4] ^= DSEP();
  // printstate("domain separation", &s);

  /* full plaintext blocks */
  while (mlen >= ASCON_128_RATE) {
    s.x[0] ^= LOADBYTES(m, 8);
    s.x[1] ^= LOADBYTES(m + 8, 8);
    STOREBYTES(c, s.x[0], 8);
    STOREBYTES(c + 8, s.x[1], 8);
    // printstate("absorb plaintext", &s);
    P8(&s);
    m += ASCON_128_RATE;
    c += ASCON_128_RATE;
    mlen -= ASCON_128_RATE;
  }
  /* final plaintext block */
  if (mlen >= 8) {
    s.x[0] ^= LOADBYTES(m, 8);
    s.x[1] ^= LOADBYTES(m + 8, (int) mlen - 8);
    STOREBYTES(c, s.x[0], 8);
    STOREBYTES(c + 8, s.x[1], (int) mlen - 8);
    s.x[1] ^= PAD(mlen - 8);
  } else {
    s.x[0] ^= LOADBYTES(m, (int) mlen);
    STOREBYTES(c, s.x[0], (int) mlen);
    s.x[0] ^= PAD(mlen);
  }
  m += mlen;
  c += mlen;
  // printstate("pad plaintext", &s);

  /* finalize */
  s.x[2] ^= K0;
  s.x[3] ^= K1;
  // printstate("final 1st key xor", &s);
  P12(&s);
  s.x[3] ^= K0;
  s.x[4] ^= K1;
  // printstate("final 2nd key xor", &s);

  /* get tag */
  STOREBYTES(c, s.x[3], 8);
  STOREBYTES(c + 8, s.x[4], 8);

  /* print output bytes */
  // printbytes("c", c - *clen + CRYPTO_ABYTES, *clen - CRYPTO_ABYTES);
  // printbytes("t", c, CRYPTO_ABYTES);
  // print("\n");

  return 0;
}

/**
 * @brief ASCON-128 authenticated decryption with associated data (AEAD).
 *
 * Decrypts a ciphertext @p c of length @p clen using key @p k and public
 * nonce @p npub, verifies the 16-byte authentication tag, and recovers the
 * plaintext message into @p m if verification succeeds.
 *
 * Algorithm outline (ASCON-128 AEAD, 12/8 round variant):
 *  1. **Input check:** If @p clen < CRYPTO_ABYTES, return -1 (ciphertext too short).
 *  2. **Initialization:** Load key (K0,K1) and nonce (N0,N1) into the state
 *     with IV, apply P12, then XOR key into state again.
 *  3. **Associated data absorption:** Same procedure as in encryption:
 *     - Absorb full 16-byte blocks into (x0,x1), apply P8().
 *     - Absorb final partial block with padding PAD(len), apply P8().
 *     - Apply domain separation via s.x[4] ^= DSEP().
 *  4. **Ciphertext processing:**
 *     - For each full 16-byte block: load ciphertext (c0,c1), derive plaintext
 *       as (s.x[0]^c0, s.x[1]^c1), update state with ciphertext, apply P8().
 *     - For final partial block: recover plaintext from current state, insert
 *       ciphertext with proper padding and masking.
 *  5. **Finalization and tag recomputation:**
 *     - XOR key words into state, apply P12, XOR key again.
 *     - Derive authentication tag from (x3,x4).
 *  6. **Verification:**
 *     - Compare recomputed tag with received tag (last 16 bytes of @p c).
 *     - Return 0 if tags match, -1 otherwise.
 *
 * @param[out] m     Plaintext output buffer. Must have space for clen - CRYPTO_ABYTES bytes.
 * @param[out] mlen  Pointer to length of plaintext output. On success,
 *                   set to clen - CRYPTO_ABYTES.
 * @param[in]  c     Ciphertext buffer, including the authentication tag.
 * @param[in]  clen  Length of ciphertext including tag, in bytes.
 * @param[in]  ad    Associated data buffer to authenticate but not decrypt.
 * @param[in]  adlen Length of associated data in bytes.
 * @param[in]  npub  Public nonce (CRYPTO_NPUBBYTES = 16 bytes).
 * @param[in]  k     Secret key (CRYPTO_KEYBYTES = 16 bytes).
 *
 * @return 0 if decryption and tag verification succeed, -1 otherwise.
 *
 * @pre
 *  - @c clen must be ≥ CRYPTO_ABYTES.
 *  - @c CRYPTO_ABYTES must equal 16 for ASCON-128.
 *  - Macros/functions @c LOADBYTES, @c STOREBYTES, @c CLEARBYTES, @c PAD,
 *    @c DSEP, @c P12(), and @c P8() must be defined consistently with ASCON.
 *
 * @note Tag comparison is done in constant time to mitigate timing side channels.
 * @note Input and output buffers must not overlap.
 * @warning Do not reuse the same (key, nonce) pair for multiple encryptions,
 *          as this breaks AEAD security.
 */
int ascon_aead_decrypt(unsigned char* m, unsigned long long* mlen,
                        const unsigned char* c, unsigned long long clen,
                        const unsigned char* ad, unsigned long long adlen,
                        const unsigned char* npub,
                        const unsigned char* k) {

  if (clen < CRYPTO_ABYTES) return -1;

  /* set plaintext size */
  *mlen = clen - CRYPTO_ABYTES;

  /* print input bytes */
  // print("decrypt\n");
  // printbytes("k", k, CRYPTO_KEYBYTES);
  // printbytes("n", npub, CRYPTO_NPUBBYTES);
  // printbytes("a", ad, adlen);
  // printbytes("c", c, *mlen);
  // printbytes("t", c + *mlen, CRYPTO_ABYTES);

  /* load key and nonce */
  const uint64_t K0 = LOADBYTES(k, 8);
  const uint64_t K1 = LOADBYTES(k + 8, 8);
  const uint64_t N0 = LOADBYTES(npub, 8);
  const uint64_t N1 = LOADBYTES(npub + 8, 8);

  /* initialize */
  ascon_state_t s;
  s.x[0] = ASCON_128_IV;
  s.x[1] = K0;
  s.x[2] = K1;
  s.x[3] = N0;
  s.x[4] = N1;
  // printstate("init 1st key xor", &s);
  P12(&s);
  s.x[3] ^= K0;
  s.x[4] ^= K1;
  // printstate("init 2nd key xor", &s);

  if (adlen) {
    /* full associated data blocks */
    while (adlen >= ASCON_128_RATE) {
      s.x[0] ^= LOADBYTES(ad, 8);
      s.x[1] ^= LOADBYTES(ad + 8, 8);
      // printstate("absorb adata", &s);
      P8(&s);
      ad += ASCON_128_RATE;
      adlen -= ASCON_128_RATE;
    }
    /* final associated data block */
    if (adlen >= 8) {
      s.x[0] ^= LOADBYTES(ad, 8);
      s.x[1] ^= LOADBYTES(ad + 8, (int) adlen - 8);
      s.x[1] ^= PAD(adlen - 8);
    } else {
      s.x[0] ^= LOADBYTES(ad, (int) adlen);
      s.x[0] ^= PAD(adlen);
    }
    // printstate("pad adata", &s);
    P8(&s);
  }
  /* domain separation */
  s.x[4] ^= DSEP();
  // printstate("domain separation", &s);

  /* full ciphertext blocks */
  clen -= CRYPTO_ABYTES;
  while (clen >= ASCON_128_RATE) {
    uint64_t c0 = LOADBYTES(c, 8);
    uint64_t c1 = LOADBYTES(c + 8, 8);
    STOREBYTES(m, s.x[0] ^ c0, 8);
    STOREBYTES(m + 8, s.x[1] ^ c1, 8);
    s.x[0] = c0;
    s.x[1] = c1;
    // printstate("insert ciphertext", &s);
    P8(&s);
    m += ASCON_128_RATE;
    c += ASCON_128_RATE;
    clen -= ASCON_128_RATE;
  }
  /* final ciphertext block */
  if (clen >= 8) {
    uint64_t c0 = LOADBYTES(c, 8);
    uint64_t c1 = LOADBYTES(c + 8, (int) clen - 8);
    STOREBYTES(m, s.x[0] ^ c0, 8);
    STOREBYTES(m + 8, s.x[1] ^ c1, (int) clen - 8);
    s.x[0] = c0;
    s.x[1] = CLEARBYTES(s.x[1], (int) clen - 8);
    s.x[1] |= c1;
    s.x[1] ^= PAD(clen - 8);
  } else {
    uint64_t c0 = LOADBYTES(c, (int) clen);
    STOREBYTES(m, s.x[0] ^ c0, (int) clen);
    s.x[0] = CLEARBYTES(s.x[0], (int) clen);
    s.x[0] |= c0;
    s.x[0] ^= PAD(clen);
  }
  m += clen;
  c += clen;
  // printstate("pad ciphertext", &s);

  /* finalize */
  s.x[2] ^= K0;
  s.x[3] ^= K1;
  // printstate("final 1st key xor", &s);
  P12(&s);
  s.x[3] ^= K0;
  s.x[4] ^= K1;
  // printstate("final 2nd key xor", &s);

  /* get tag */
  uint8_t t[16];
  STOREBYTES(t, s.x[3], 8);
  STOREBYTES(t + 8, s.x[4], 8);

  /* verify should be constant time, check compiler output */
  int i;
  int result = 0;
  for (i = 0; i < CRYPTO_ABYTES; ++i) result |= c[i] ^ t[i];
  result = (((result - 1) >> 8) & 1) - 1;

  /* print output bytes */
  // printbytes("m", m - *mlen, *mlen);
  // print("\n");

  return result;
}
