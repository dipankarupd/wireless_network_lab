/*
 * Copyright (c) 2009 Chris K Cockrum <ckc@cockrum.net>
 *
 * Copyright (c) 2013 Jens Trillmann <jtrillma@tzi.de>
 * Copyright (c) 2013 Marc Müller-Weinhardt <muewei@tzi.de>
 * Copyright (c) 2013 Lars Schmertmann <lars@tzi.de>
 * Copyright (c) 2013 Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * This implementation is based in part on the paper Implementation of an
 * Elliptic Curve Cryptosystem on an 8-bit Microcontroller [0] by
 * Chris K Cockrum <ckc@cockrum.net>.
 *
 * [0]: http://cockrum.net/Implementation_of_ECC_on_an_8-bit_microcontroller.pdf
 *
 * This is a efficient ECC implementation on the secp256r1 curve for 32 Bit CPU
 * architectures. It provides basic operations on the secp256r1 curve and support
 * for ECDH and ECDSA.
 */
#include <inttypes.h>

#define keyLengthInBytes 32
#define arrayLength 8

extern const uint32_t ecc_g_point_x[8];
extern const uint32_t ecc_g_point_y[8];
extern const uint32_t ecc_order_m[9];

//ec Functions
void ecc_ec_mult(const uint32_t *px, const uint32_t *py, const uint32_t *secret, uint32_t *resultx, uint32_t *resulty);

static inline void ecc_ecdh(const uint32_t *px, const uint32_t *py, const uint32_t *secret, uint32_t *resultx, uint32_t *resulty) {
	ecc_ec_mult(px, py, secret, resultx, resulty);
}
int ecc_ecdsa_validate(const uint32_t *x, const uint32_t *y, const uint32_t *e, const uint32_t *r, const uint32_t *s);
int ecc_ecdsa_sign(const uint32_t *d, const uint32_t *e, const uint32_t *k, uint32_t *r, uint32_t *s);

int ecc_is_valid_key(const uint32_t * priv_key);
static inline void ecc_gen_pub_key(const uint32_t *priv_key, uint32_t *pub_x, uint32_t *pub_y)
{
	ecc_ec_mult(ecc_g_point_x, ecc_g_point_y, priv_key, pub_x, pub_y);
}


//ec Functions
void ecc_ec_add(const uint32_t *px, const uint32_t *py, const uint32_t *qx, const uint32_t *qy, uint32_t *Sx, uint32_t *Sy);
void ecc_ec_double(const uint32_t *px, const uint32_t *py, uint32_t *Dx, uint32_t *Dy);

//simple Functions for addition and substraction of big numbers
uint32_t ecc_add( const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);
uint32_t ecc_sub( const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);

//field functions for big numbers
/**
 * @brief Add two field elements with optional modular reduction by carry.
 *
 * Performs the addition of two multi-limb integers `x` and `y` of length
 * `arrayLength` words (uint32_t). If the addition produces a carry out of
 * the most significant limb, the provided `reducer` value is added to the
 * result to fold the carry back into the field representation.
 *
 * This operation is typically used for modular addition in prime fields,
 * where `reducer` is chosen such that adding it conditionally brings the
 * result back into the valid range.
 *
 * @param[in]  x        First addend (arrayLength limbs).
 * @param[in]  y        Second addend (arrayLength limbs).
 * @param[in]  reducer  Field-specific constant added when carry occurs
 *                      (arrayLength limbs, e.g., ecc_prime_r).
 * @param[out] result   Output sum (arrayLength limbs).
 *
 * @return Always returns 0.
 *
 * @note This function does not check whether the result is larger than the
 *       modulus except via the carry check, so it may still require an
 *       additional reduction in some contexts.
 * @note Not constant-time; not safe for secret-dependent conditional use
 *       against timing side-channel attacks.
 */
int ecc_fieldAdd(const uint32_t *x, const uint32_t *y, const uint32_t *reducer, uint32_t *result);
int ecc_fieldSub(const uint32_t *x, const uint32_t *y, const uint32_t *modulus, uint32_t *result);
int ecc_fieldMult(const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);
/**
 * @brief Reduce a large integer modulo the secp256r1 prime field.
 *
 * This function wraps the internal `fieldModP()` implementation, performing
 * an efficient modular reduction using the special form of the secp256r1
 * prime:
 *
 *     p = 0xFFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF
 *
 * The reduction algorithm exploits the pseudo-Mersenne structure of the prime
 * to fold the higher-order limbs of the input back into the lower limbs using
 * a series of additions and subtractions, avoiding a costly division.
 *
 * @param[out] A       Result of the modular reduction (arrayLength = 8 limbs, 256 bits).
 * @param[in]  B       Input value to reduce (up to 16 limbs, 512 bits, e.g., from a multiplication).
 *
 * @note The input and output pointers may reference the same memory location.
 * @note This function is not constant-time and should not be used with secret data
 *       in contexts where timing side channels are a concern.
 */
void ecc_fieldModP(uint32_t *A, const uint32_t *B);

void ecc_fieldModP(uint32_t *A, const uint32_t *B);
void ecc_fieldModO(const uint32_t *A, uint32_t *result, uint8_t length);
void ecc_fieldInv(const uint32_t *A, const uint32_t *modulus, const uint32_t *reducer, uint32_t *B);

//simple functions to work with the big numbers
void ecc_copy(const uint32_t *from, uint32_t *to, uint8_t length);
int ecc_isSame(const uint32_t *A, const uint32_t *B, uint8_t length);
void ecc_setZero(uint32_t *A, const int length);
int ecc_isOne(const uint32_t* A);
void ecc_rshift(uint32_t* A);
int ecc_isGreater(const uint32_t *A, const uint32_t *B, uint8_t length);


