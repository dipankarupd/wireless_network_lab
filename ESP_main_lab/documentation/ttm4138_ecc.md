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

### ecc_fieldNegationMod0

```c
void ecc_fieldNegationMod0(const uint32_t* a, uint32_t* result, uint8_t length);
```

**Purpose**  
Modular negation **modulo the group order** `n` (for secp256r1):

$$ \text{result} \equiv -a \pmod{n} \quad\text{(i.e., } \text{result} = (n - a) \bmod n\text{)} $$

Special case: if `a == 0`, the result is `0`.

**Parameters**

| Name     | Type               | Description |
|----------|--------------------|-------------|
| `a`      | `const uint32_t*`  | Input scalar (assumed reduced: `0 \le a < n`). |
| `result` | `uint32_t*`        | Output buffer (same limb length as `a`). |
| `length` | `uint8_t`          | Number of 32-bit words in `a` and `result`. |

**Returns**  
- `void`.

**Global State Modified**  
- None (reads the constant group order `ecc_order_m`; does not modify globals).

**Notes**  
- Name uses a trailing `0` (zero) in the header. The implementation you provided spells this as **`ecc_fieldNegationModO`** (letter **O** for “Order”). Keep the spelling consistent across header/implementation to avoid link errors.

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
| `result` | `uint32_t*`        | Output buffer (same limb length as inputs). |
| `length` | `uint8_t`          | Number of 32-bit words in `a`/`b`/`result`. |

**Returns**  
- `void`.

**Global State Modified**  
- None.

**See also**  
- [ecc_add](./ecc.md#ecc_add) — word-wise big-integer addition with carry (used internally).  
- [ecc_fieldModO](./ecc.md#ecc_fieldModO) — Barrett reduction modulo the group order.

