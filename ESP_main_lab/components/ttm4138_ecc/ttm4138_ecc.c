#include <stdint.h>
#include <stdio.h>
#include "ecc.h"
#include "ttm4138_ecc.h"

/**
 * @brief Check if a multi-precision integer is equal to zero.
 *
 * Iterates over all @p length 32-bit words of @p a and returns whether
 * the value is exactly zero.
 *
 * @param[in] a       Input array of 32-bit words.
 * @param[in] length  Number of 32-bit words in @p a.
 *
 * @return
 *   - 1 if A == 0  
 *   - 0 otherwise
 *
 * @note Generalized version of isZero() that works for arbitrary operand
 *       sizes, not just 256-bit values.
 * @note For a fixed public length, all limbs are scanned without branching
 *       on their values. Constant-time behavior still depends on compiled code.
 */
int ecc_isZero(const uint32_t* a, uint8_t length) {
    uint32_t acc = 0; for (int i=0;i<length;++i) acc |= a[i]; return acc == 0;
}
// int ecc_isZero(const uint32_t* A, uint8_t length) {
//     for (uint8_t n = 0; n < length; n++) {
//         if (A[n] != 0) {
//             return 0;
//         }
//     }
//     return 1;
// }

/**
 * @brief Modular negation of a field element modulo the group order n.
 *
 * Computes:
 *   result = (n - a) mod n
 *
 * Special case:
 *   - If a == 0, then result = 0.
 *
 * Since 0 < a < n is guaranteed for valid scalars, subtraction n - a
 * never underflows.
 *
 * @param[in]  a       Reduced scalar in a 9-word buffer with a[8] = 0.
 * @param[out] result  Output buffer with space for 9 words. The zero-input
 *                     path clears only the first 8 words.
 * @param[in]  length  Scalar limb count; use 8 for secp256r1. The nonzero
 *                     path reads and writes length + 1 words.
 *
 * @note Uses the global constant @c ecc_order_m (group order of secp256r1).
 * @warning Input @p a must already be reduced modulo n (i.e., less than n).
 */
void ecc_fieldNegationModO(const uint32_t* a, uint32_t* result, uint8_t length) {
    if (ecc_isZero(a, length)) { ecc_setZero(result, length); return; }
    // out = n - a  (borrow can’t occur because a < n)
    ecc_sub(ecc_order_m, a, result, length + 1);
}

/**
 * @brief Add two scalars modulo the group order n.
 *
 * Computes:
 *   result = (a + b) mod n
 *
 * The addition is performed word-wise, with the carry placed in an
 * extra high limb. The intermediate sum is then reduced modulo the
 * group order @c n using @ref ecc_fieldModO().
 *
 * @param[in]  a       First operand (scalar), array of 32-bit words.
 * @param[in]  b       Second operand (scalar), array of 32-bit words.
 * @param[out] result  Output array with space for 9 words.
 * @param[in]  length  Number of input words; use 8 for secp256r1. Reduction
 *                     writes 9 words to result.
 *
 * @note Uses the group order modulus internally via @ref ecc_fieldModO().
 * @warning Inputs must already be reduced modulo n; this function does not
 *          accept unreduced operands outside [0, n).
 */
void ecc_fieldAddModO(const uint32_t* a, const uint32_t* b, uint32_t* result, uint8_t length) {
    uint32_t sum9[length + 1];
    sum9[length] = ecc_add(a, b, sum9, length);  // returns carry in sum9[8]
    ecc_fieldModO(sum9, result, length + 1);
}


#include <stdint.h>

/*
 * Prevent the compiler from optimizing masked selections
 * into secret-dependent branches.
 *
 * GCC/Clang extension supported by ESP-IDF toolchains.
 */
static inline uint64_t ct_barrier_u64(uint64_t x)
{
    __asm__ volatile ("" : "+r" (x));
    return x;
}

/**
 * @brief Add two reduced 64-bit integers modulo m.
 *
 * Computes:
 *   result = (a + b) mod m
 *
 * Uses unsigned arithmetic and bitwise masking to avoid
 * overflow and secret-dependent conditional branches.
 *
 * @param[in] a  First operand.
 * @param[in] b  Second operand.
 * @param[in] m  Modulus.
 *
 * @return The modular sum.
 *
 * @warning Requires a < m, b < m, and m > 1.
 */
static uint64_t add_mod_ct(uint64_t a, uint64_t b, uint64_t m)
{
    uint64_t gap = m - b;
    uint64_t diff = a - gap;

    uint64_t borrow =
        ((~a & gap) | (~(a ^ gap) & diff)) >> 63;

    uint64_t mask = ct_barrier_u64(UINT64_C(0) - borrow);
    uint64_t sum = a + b;

    return (sum & mask) | (diff & ~mask);
}

/**
 * @brief Reduce a 64-bit integer modulo m in constant work.
 *
 * Computes:
 *   result = x mod m
 *
 * Processes all 64 bits without division or
 * secret-dependent branches.
 *
 * @param[in] x  Integer to reduce.
 * @param[in] m  Modulus.
 *
 * @return The reduced integer.
 *
 * @warning Requires m > 1.
 */
static uint64_t reduce_mod_ct(uint64_t x, uint64_t m)
{
    uint64_t r = 0;

    for (unsigned i = 0; i < 64; ++i) {
        uint64_t bit = x >> 63;
        x <<= 1;

        r = add_mod_ct(r, r, m);
        r = add_mod_ct(r, bit, m);
    }

    return r;
}

/**
 * @brief Multiply two reduced integers modulo m.
 *
 * Computes:
 *   result = (a * b) mod m
 *
 * Uses 64 iterations of modular addition and doubling.
 * Conditional additions are implemented using bitwise masks.
 *
 * @param[in] a  First operand.
 * @param[in] b  Second operand.
 * @param[in] m  Modulus.
 *
 * @return The modular product.
 *
 * @warning Requires a < m, b < m, and m > 1.
 */
static uint64_t mul_mod_ct(uint64_t a, uint64_t b, uint64_t m)
{
    uint64_t r = 0;

    for (unsigned i = 0; i < 64; ++i) {
        uint64_t candidate = add_mod_ct(r, a, m);

        uint64_t mask = ct_barrier_u64(
            UINT64_C(0) - (b & UINT64_C(1))
        );

        r = (r & ~mask) | (candidate & mask);

        a = add_mod_ct(a, a, m);
        b >>= 1;
    }

    return r;
}

/**
 * @brief Conditionally swap two 64-bit integers.
 *
 * Swaps the operands when bit is 1 and leaves them
 * unchanged when bit is 0, using bitwise masking.
 *
 * @param[in,out] a    First operand.
 * @param[in,out] b    Second operand.
 * @param[in]     bit  Swap condition (0 or 1).
 */
static void cswap_u64(uint64_t *a, uint64_t *b, uint64_t bit)
{
    uint64_t mask = ct_barrier_u64(UINT64_C(0) - bit);
    uint64_t t = (*a ^ *b) & mask;

    *a ^= t;
    *b ^= t;
}

/**
 * @brief Compute modular exponentiation using the Montgomery ladder.
 *
 * Computes:
 *   result = (b^e) mod m
 *
 * The exponentiation uses a fixed 64-iteration Montgomery ladder
 * with masked conditional swaps. Each iteration performs two
 * modular multiplications using @ref mul_mod_ct().
 *
 * The base is reduced using @ref reduce_mod_ct(), and modular
 * addition is performed using @ref add_mod_ct().
 *
 * All arithmetic uses 64-bit unsigned integers without requiring
 * 128-bit support, division, or variable-length loops.
 *
 * @param[in] b  Base of the exponentiation.
 * @param[in] e  Exponent (unsigned 64-bit integer).
 * @param[in] m  Modulus (unsigned 64-bit integer).
 *
 * @return The result of (b^e) mod m, or 0 if @p m is zero.
 *
 * @note Supports the full 64-bit unsigned integer range.
 * @note For a fixed public modulus, the operation counts and
 *       memory access patterns are independent of @p b and @p e.
 * @warning The modulus must be nonzero for a valid computation.
 * @warning Constant-time behavior depends on the generated machine
 *          code and hardware. Verify the compiled target binary
 *          before using this function with secret exponents.
 */
uint64_t pow_mod(uint64_t b, uint64_t e, uint64_t m)
{
    if (m <= 1) {
        return 0;
    }

    uint64_t r0 = 1;
    uint64_t r1 = reduce_mod_ct(b, m);

    for (unsigned i = 0; i < 64; ++i) {
        uint64_t bit = e >> 63;
        e <<= 1;

        cswap_u64(&r0, &r1, bit);

        uint64_t next_r1 = mul_mod_ct(r0, r1, m);
        uint64_t next_r0 = mul_mod_ct(r0, r0, m);

        r0 = next_r0;
        r1 = next_r1;

        cswap_u64(&r0, &r1, bit);
    }

    return r0;
}

