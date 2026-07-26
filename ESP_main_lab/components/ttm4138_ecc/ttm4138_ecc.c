#include <stdint.h>
#include <stdio.h>
#include "ecc.h"
#include "ttm4138_ecc.h"

/**
 * @brief Check if a multi-precision integer is equal to zero.
 *
 * Iterates over all @p length 32-bit words of @p A and returns whether
 * the value is exactly zero.
 *
 * @param[in] A       Input array of 32-bit words.
 * @param[in] length  Number of 32-bit words in @p A.
 *
 * @return
 *   - 1 if A == 0  
 *   - 0 otherwise
 *
 * @note Generalized version of isZero() that works for arbitrary operand
 *       sizes, not just 256-bit values.
 * @warning Not constant-time. Should not be used directly on secret values
 *          if resistance to timing side channels is required.
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
 * @param[in]  a       Input scalar (array of 32-bit words).
 * @param[out] result  Output array (same size as @p a).
 * @param[in]  length  Number of 32-bit words in @p a and @p result.
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
 * @param[out] result  Output array (same size as @p a and @p b).
 * @param[in]  length  Number of 32-bit words in @p a and @p b.
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
