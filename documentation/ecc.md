# ECC (secp256r1) API — Arguments, Return Values, Purpose & Global Side Effects

The entries below document the functions declared in the ECC header.  
Each item includes a concise **purpose**, detailed **parameters**, **return values**, and any **global side effects** (typically none).  
Cross-references use `#` links for quick navigation.

---

### ecc_ec_mult

```c
void ecc_ec_mult(const uint32_t *px, const uint32_t *py, const uint32_t *secret, uint32_t *resultx, uint32_t *resulty);
```

**Purpose**  
Scalar multiplication on secp256r1:

$$ (resultx,\, resulty) = \text{secret} \cdot (px,\, py) $$

**Parameters**

| Name       | Type              | Description |
|------------|-------------------|-------------|
| `px`       | `const uint32_t*` | X-coordinate of the base point (8 limbs, little-endian). |
| `py`       | `const uint32_t*` | Y-coordinate of the base point (8 limbs). |
| `secret`   | `const uint32_t*` | Scalar multiplier (8 limbs). |
| `resultx`  | `uint32_t*`       | Output X-coordinate (8 limbs). |
| `resulty`  | `uint32_t*`       | Output Y-coordinate (8 limbs). |

**Returns**  
- `void` — writes the resulting point into `resultx`, `resulty`.

**Global State Modified**  
- None.

**See also**  
- [ecc_ecdh](#ecc_ecdh) — thin ECDH wrapper over this function.  
- [ecc_gen_pub_key](#ecc_gen_pub_key) — computes `priv · G`.

---

### ecc_ecdh

```c
static inline void ecc_ecdh(const uint32_t *px, const uint32_t *py, const uint32_t *secret, uint32_t *resultx, uint32_t *resulty);
```

**Purpose**  
ECDH helper: computes a shared point as `secret · (px, py)` by calling [ecc_ec_mult](#ecc_ec_mult).

**Parameters**  
Same as [ecc_ec_mult](#ecc_ec_mult).

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_ecdsa_validate

```c
int ecc_ecdsa_validate(const uint32_t *x, const uint32_t *y, const uint32_t *e, const uint32_t *r, const uint32_t *s);
```

**Purpose**  
Verify an ECDSA signature `(r, s)` over hash `e` with public key `(x, y)` on secp256r1.

**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `x`  | `const uint32_t*` | Public key X (8 limbs). |
| `y`  | `const uint32_t*` | Public key Y (8 limbs). |
| `e`  | `const uint32_t*` | Message digest (8 limbs). |
| `r`  | `const uint32_t*` | Signature `r` (8 limbs). |
| `s`  | `const uint32_t*` | Signature `s` (8 limbs). |

**Returns**  
- `int` — `0` if the signature is valid; `-1` otherwise.

**Global State Modified**  
- None.

---

### ecc_ecdsa_sign

```c
int ecc_ecdsa_sign(const uint32_t *d, const uint32_t *e, const uint32_t *k, uint32_t *r, uint32_t *s);
```

**Purpose**  
Create an ECDSA signature `(r, s)` over hash `e` using private key `d` and per-message nonce `k` (must be unique and unpredictable).

**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `d`  | `const uint32_t*` | Private key (8 limbs). |
| `e`  | `const uint32_t*` | Message digest (8 limbs). |
| `k`  | `const uint32_t*` | Per-message nonce (8 limbs). |
| `r`  | `uint32_t*`       | Output signature `r` (8 limbs). |
| `s`  | `uint32_t*`       | Output signature `s` (8 limbs). |

**Returns**  
- `int` — `0` on success; `-1` on failure (e.g., invalid `k`, degenerate values).

**Global State Modified**  
- None.

---

### ecc_is_valid_key

```c
int ecc_is_valid_key(const uint32_t *priv_key);
```

**Purpose**  
Check whether `priv_key` is a valid secp256r1 private key (typically `0 < priv_key < n`, where `n` is the group order).

**Parameters**

| Name       | Type              | Description |
|------------|-------------------|-------------|
| `priv_key` | `const uint32_t*` | Candidate private key (8 limbs). |

**Returns**  
- `int` — nonzero if valid; `0` if invalid.

**Global State Modified**  
- None.

**See also**  
- [ecc_gen_pub_key](#ecc_gen_pub_key).

---

### ecc_gen_pub_key

```c
static inline void ecc_gen_pub_key(const uint32_t *priv_key, uint32_t *pub_x, uint32_t *pub_y);
```

**Purpose**  
Generate a public key from a private key using the fixed base point `G`:

$$ (pub\_x,\, pub\_y) = \text{priv\_key} \cdot G $$

**Parameters**

| Name       | Type              | Description |
|------------|-------------------|-------------|
| `priv_key` | `const uint32_t*` | Private key (8 limbs). |
| `pub_x`    | `uint32_t*`       | Output X (8 limbs). |
| `pub_y`    | `uint32_t*`       | Output Y (8 limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_ec_add

```c
void ecc_ec_add(const uint32_t *px, const uint32_t *py, const uint32_t *qx, const uint32_t *qy, uint32_t *Sx, uint32_t *Sy);
```

**Purpose**  
Elliptic-curve point addition:

$$ (Sx,\, Sy) = (px,\, py) + (qx,\, qy) $$


**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `px` | `const uint32_t*` | First point X (8 limbs). |
| `py` | `const uint32_t*` | First point Y (8 limbs). |
| `qx` | `const uint32_t*` | Second point X (8 limbs). |
| `qy` | `const uint32_t*` | Second point Y (8 limbs). |
| `Sx` | `uint32_t*`       | Output sum X (8 limbs). |
| `Sy` | `uint32_t*`       | Output sum Y (8 limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_ec_double

```c
void ecc_ec_double(const uint32_t *px, const uint32_t *py, uint32_t *Dx, uint32_t *Dy);
```

**Purpose**  
Elliptic-curve point doubling:

$$ (Dx,\, Dy) = 2 \cdot (px,\, py) $$

**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `px` | `const uint32_t*` | Point X (8 limbs). |
| `py` | `const uint32_t*` | Point Y (8 limbs). |
| `Dx` | `uint32_t*`       | Output doubled X (8 limbs). |
| `Dy` | `uint32_t*`       | Output doubled Y (8 limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_add

```c
uint32_t ecc_add(const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);
```

**Purpose**  
Add two multi-precision integers `x` and `y` (each `length` 32-bit limbs), producing `result` and a carry-out.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `x`      | `const uint32_t*` | First addend. |
| `y`      | `const uint32_t*` | Second addend. |
| `result` | `uint32_t*`       | Output sum (`length` limbs). |
| `length` | `uint8_t`         | Number of 32-bit limbs. |

**Returns**  
- `uint32_t` — carry-out from the most significant limb (`0` or `1`).

**Global State Modified**  
- None.

---

### ecc_sub

```c
uint32_t ecc_sub(const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);
```

**Purpose**  
Subtract two multi-precision integers, computing `result = x − y` over `length` limbs with borrow propagation.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `x`      | `const uint32_t*` | Minuend. |
| `y`      | `const uint32_t*` | Subtrahend. |
| `result` | `uint32_t*`       | Output difference (`length` limbs). |
| `length` | `uint8_t`         | Number of 32-bit limbs. |

**Returns**  
- `uint32_t` — final borrow (`0` if `x ≥ y`, `1` if `x < y`).

**Global State Modified**  
- None.

---

### ecc_fieldAdd

```c
int ecc_fieldAdd(const uint32_t *x, const uint32_t *y, const uint32_t *reducer, uint32_t *result);
```

**Purpose**  
Field addition with conditional correction: compute `result = x + y`; if there is a carry out, add `reducer` to fold the carry back into the field.

**Parameters**

| Name      | Type              | Description |
|-----------|-------------------|-------------|
| `x`       | `const uint32_t*` | First operand (arrayLength limbs). |
| `y`       | `const uint32_t*` | Second operand (arrayLength limbs). |
| `reducer` | `const uint32_t*` | Field-specific correction constant (arrayLength limbs). |
| `result`  | `uint32_t*`       | Output sum (arrayLength limbs). |

**Returns**  
- `int` — always `0`.

**Global State Modified**  
- None.

---

### ecc_fieldSub

```c
int ecc_fieldSub(const uint32_t *x, const uint32_t *y, const uint32_t *modulus, uint32_t *result);
```

**Purpose**  
Field subtraction with conditional correction: compute `result = x − y`; if the subtraction underflows, add `modulus` to re-normalize into the field.

**Parameters**

| Name      | Type              | Description |
|-----------|-------------------|-------------|
| `x`       | `const uint32_t*` | Minuend (arrayLength limbs). |
| `y`       | `const uint32_t*` | Subtrahend (arrayLength limbs). |
| `modulus` | `const uint32_t*` | Field modulus (arrayLength limbs). |
| `result`  | `uint32_t*`       | Output difference (arrayLength limbs). |

**Returns**  
- `int` — always `0`.

**Global State Modified**  
- None.

---

### ecc_fieldMult

```c
int ecc_fieldMult(const uint32_t *x, const uint32_t *y, uint32_t *result, uint8_t length);
```

**Purpose**  
Schoolbook big-integer multiplication: compute `result = x · y` for `length`-limb inputs; `result` has `2·length` limbs.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `x`      | `const uint32_t*` | Multiplicand (`length` limbs). |
| `y`      | `const uint32_t*` | Multiplier (`length` limbs). |
| `result` | `uint32_t*`       | Output product (`2·length` limbs). |
| `length` | `uint8_t`         | Operand limb count. |

**Returns**  
- `int` — always `0`.

**Global State Modified**  
- None.

---

### ecc_fieldModP

```c
void ecc_fieldModP(uint32_t *A, const uint32_t *B);
```

**Purpose**  
Reduce a large integer `B` modulo the secp256r1 prime `p`, writing the reduced value to `A`.

**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `A`  | `uint32_t*`       | Output reduced value (8 limbs). May alias `B`. |
| `B`  | `const uint32_t*` | Input value to reduce (up to 16 limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_fieldModO

```c
void ecc_fieldModO(const uint32_t *A, uint32_t *result, uint8_t length);
```

**Purpose**  
Reduce a large integer `A` modulo the group order `n` using Barrett reduction; write the result to `result`.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `A`      | `const uint32_t*` | Input value (up to `length` limbs). |
| `result` | `uint32_t*`       | Output `A mod n` (up to 9 limbs depending on use). |
| `length` | `uint8_t`         | Limb count of `A`. |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_fieldInv

```c
void ecc_fieldInv(const uint32_t *A, const uint32_t *modulus, const uint32_t *reducer, uint32_t *B);
```

**Purpose**  
Compute modular inverse via the binary extended Euclidean algorithm:

$$ B = A^{-1} \bmod \text{modulus} $$

**Parameters**

| Name      | Type              | Description |
|-----------|-------------------|-------------|
| `A`       | `const uint32_t*` | Input value (arrayLength limbs). |
| `modulus` | `const uint32_t*` | Field modulus (arrayLength limbs). |
| `reducer` | `const uint32_t*` | Reduction constant used during halving steps. |
| `B`       | `uint32_t*`       | Output inverse (arrayLength limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_copy

```c
void ecc_copy(const uint32_t *from, uint32_t *to, uint8_t length);
```

**Purpose**  
Copy `length` limbs from `from` to `to` (wrapper around `memcpy` for big numbers).

**Parameters**

| Name    | Type              | Description |
|---------|-------------------|-------------|
| `from`  | `const uint32_t*` | Source array. |
| `to`    | `uint32_t*`       | Destination array. |
| `length`| `uint8_t`         | Limb count. |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_isSame

```c
int ecc_isSame(const uint32_t *A, const uint32_t *B, uint8_t length);
```

**Purpose**  
Constant-**non**-time equality check for two big integers.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `A`      | `const uint32_t*` | First operand. |
| `B`      | `const uint32_t*` | Second operand. |
| `length` | `uint8_t`         | Limb count. |

**Returns**  
- `int` — `1` if equal; `0` otherwise.

**Global State Modified**  
- None.

---

### ecc_setZero

```c
void ecc_setZero(uint32_t *A, const int length);
```

**Purpose**  
Set all `length` limbs of `A` to zero.

**Parameters**

| Name     | Type        | Description |
|----------|-------------|-------------|
| `A`      | `uint32_t*` | Target array. |
| `length` | `const int` | Limb count. |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_isOne

```c
int ecc_isOne(const uint32_t *A);
```

**Purpose**  
Check whether a 256-bit big integer equals `1` (expects 8-limb input).

**Parameters**

| Name | Type              | Description |
|------|-------------------|-------------|
| `A`  | `const uint32_t*` | Input (8 limbs). |

**Returns**  
- `int` — `1` if `A == 1`; `0` otherwise.

**Global State Modified**  
- None.

---

### ecc_rshift

```c
void ecc_rshift(uint32_t *A);
```

**Purpose**  
Right-shift a 256-bit integer `A` by 1 bit (8 limbs; carries between limbs).

**Parameters**

| Name | Type        | Description |
|------|-------------|-------------|
| `A`  | `uint32_t*` | In-place operand (8 limbs). |

**Returns**  
- `void`.

**Global State Modified**  
- None.

---

### ecc_isGreater

```c
int ecc_isGreater(const uint32_t *A, const uint32_t *B, uint8_t length);
```

**Purpose**  
Compare two big integers.

**Parameters**

| Name     | Type              | Description |
|----------|-------------------|-------------|
| `A`      | `const uint32_t*` | First operand. |
| `B`      | `const uint32_t*` | Second operand. |
| `length` | `uint8_t`         | Limb count. |

**Returns**  
- `int` — `1` if `A > B`; `-1` if `A < B`; `0` if equal.

**Global State Modified**  
- None.

