# ECC Utilities Documentation

This library provides some additional operations for big interger aritmetic

> [!WARNING]
> Do not use this library in production code, it is not constant time.

---

## Definitely useful functions

Each of the following functions was used at least once in the staff solution.

---

### ecc_isZero

```c
int ecc_isZero(const uint32_t* A, uint8_t length);
```

**Purpose**  
Check whether a multi-precision integer is exactly zero by scanning all `length` 32-bit limbs.

**Parameters**

| Name     | Type               | Description |
|----------|--------------------|-------------|
| `A`      | `const uint32_t*`  | Input array of 32-bit words to test. |
| `length` | `uint8_t`          | Number of 32-bit words in `A`. |

**Returns**  
- `int` — `1` if `A == 0`; `0` otherwise.

**Global State Modified**  
- None.

---

### ecc_fieldNegationModO

```c
void ecc_fieldNegationModO(const uint32_t* a, uint32_t* result, uint8_t length);
```

**Purpose**  
Modular negation **modulo the group order** `n` (for secp256r1):

$$ \text{result} \equiv -a \pmod{n} \quad\text{(i.e., } \text{result} = (n - a) \bmod n\text{)} $$

Special case: if `a == 0`, the result is `0`.

**Parameters**

| Name     | Type               | Description |
|----------|--------------------|-------------|
| `a`      | `const uint32_t*`  | Input scalar in an allocated 9-limb buffer with limb 8 set to zero; use `length = 8` (assumed reduced: `0 \le a < n`). |
| `result` | `uint32_t*`        | Output buffer with space for 9 limbs. For zero input, only the first 8 limbs are cleared. |
| `length` | `uint8_t`          | Scalar limb count; use 8 for secp256r1. The nonzero path accesses 9 limbs. |

**Returns**  
- `void`.

**Global State Modified**  
- None (reads the constant group order `ecc_order_m`; does not modify globals).

**Notes**  
- The name ends in the letter **O** for “Order” in both the header and implementation.

---

### ecc_fieldAddModO

```c
void ecc_fieldAddModO(const uint32_t* a, const uint32_t* b, uint32_t* result, uint8_t length);
```

**Purpose**  
Add two scalars **modulo the group order** `n` (for secp256r1):

$$ \text{result} \equiv a + b \pmod{n} $$

Performs a multi-precision addition (capturing the carry in an extra limb) and then reduces the extended sum **mod `n`** (see [ecc_fieldModO](./ecc.md#ecc_fieldModO)).

**Parameters**

| Name     | Type               | Description |
|----------|--------------------|-------------|
| `a`      | `const uint32_t*`  | First addend (assumed reduced: `0 \le a < n`). |
| `b`      | `const uint32_t*`  | Second addend (assumed reduced: `0 \le b < n`). |
| `result` | `uint32_t*`        | Output buffer with space for 9 limbs, including the high limb written by reduction. |
| `length` | `uint8_t`          | Number of input limbs; use 8 for secp256r1. The result requires 9 limbs. |

**Returns**  
- `void`.

**Global State Modified**  
- None.

**See also**  
- [ecc_add](./ecc.md#ecc_add) — word-wise big-integer addition with carry (used internally).  
- [ecc_fieldModO](./ecc.md#ecc_fieldModO) — Barrett reduction modulo the group order.

### pow_mod

```c
uint64_t pow_mod(uint64_t b, uint64_t e, uint64_t m);
```

**Purpose**  
Compute **modular exponentiation** using the Montgomery ladder:

$$
\text{result} \equiv b^e \pmod{m}
$$

Processes all 64 exponent bits, maintaining two intermediate residues and using masked conditional swaps to select the ladder operations without branching on exponent bits.

Modular multiplication is performed using fixed-iteration addition and doubling, preventing overflow without requiring 128-bit integer support. The base is reduced modulo `m` before exponentiation.

**Parameters**

| Name | Type | Description |
|------|------|-------------|
| `b` | `uint64_t` | Base of the exponentiation (need not be reduced modulo `m`). |
| `e` | `uint64_t` | Non-negative exponent; all 64 bits are processed. |
| `m` | `uint64_t` | Modulus (`m > 0` for a valid computation). Must be public when constant-time behavior is required. |

**Returns**  
- `uint64_t` — The result of `b^e mod m`.
- Returns `0` if `m == 0` (invalid modulus), or if `m == 1`.

**Global State Modified**  
- None.

**Timing and Security**  
- Performs a fixed 64-step Montgomery ladder, executing two modular multiplications per exponent bit.
- Uses masked conditional swaps and fixed-iteration modular multiplication to avoid secret-dependent branching in the C source.
- Does not require native 128-bit arithmetic, making it suitable for the ESP32-C6.
- Constant-time behavior must be verified in the compiled machine code; the C implementation alone does not guarantee it.
- The modulus `m` must be public for the early-return behavior to be independent of secrets.

**Example**

```c
uint64_t result = pow_mod(5, 13, 511699831991437345ULL);
// result == 1220703125ULL
```
