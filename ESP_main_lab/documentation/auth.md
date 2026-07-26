# ASCON Auth/PRF Documentation

Documentation for the suplied ASCON authentication library

---

## Definitely useful functions

Each of the following functions was used at least once in the staff solution.


---

### prf

```c
int prf(unsigned char* out, unsigned long long outlen, const unsigned char* in, unsigned long long inlen, const unsigned char* k);
```

**Purpose**  
ASCON-based **pseudorandom function (PRF)**. Derives a pseudorandom output from a secret key and input message using the ASCON permutation. Suitable as a building block for MACs, KDFs, or other primitives that require a fixed-length PRF output.

**Parameters**

| Name      | Type                   | Description |
|-----------|------------------------|-------------|
| `out`     | `unsigned char*`       | Output buffer to receive the PRF output. |
| `outlen`  | `unsigned long long`   | Number of bytes to generate. Must be ≤ `CRYPTO_BYTES`. |
| `in`      | `const unsigned char*` | Input message to absorb. |
| `inlen`   | `unsigned long long`   | Length of `in` in bytes. |
| `k`       | `const unsigned char*` | Secret key buffer (16 bytes / 128 bits). |

**Returns**  
- `int` — `0` on success; `-1` if `outlen > CRYPTO_BYTES`.

**Global State Modified**  
- None.

**Notes**  
- The caller must provide an output buffer `out` of at least `outlen` bytes.  
- Input and output buffers must not overlap.

---

## Possibly useful functions

Although not used in the staff solution, these functions might still be useful in your solutions.

---

### auth

```c
int auth(unsigned char *out, const unsigned char *in, unsigned long long inlen, const unsigned char *k);
```

**Purpose**  
Compute a fixed-length **authentication tag** for `in` using the ASCON-based PRF. This is a thin wrapper over [prf](#prf) that always produces `CRYPTO_BYTES` bytes.

**Parameters**

| Name    | Type                   | Description |
|---------|------------------------|-------------|
| `out`   | `unsigned char*`       | Output buffer for the authentication tag. Must have space for `CRYPTO_BYTES` bytes. |
| `in`    | `const unsigned char*` | Input message to authenticate. |
| `inlen` | `unsigned long long`   | Length of `in` in bytes. |
| `k`     | `const unsigned char*` | Secret key buffer (16 bytes / 128 bits). |

**Returns**  
- `int` — `0` on success; `-1` if the underlying [prf](#prf) call fails.

**Global State Modified**  
- None.

**See also**  
- [prf](#prf)

---

### auth_verify

```c
int auth_verify(const unsigned char *h, const unsigned char *in, unsigned long long inlen, const unsigned char *k);
```

**Purpose**  
Verify an authentication tag by recomputing the ASCON-based tag for `in` with key `k` and comparing it in **constant time** against `h`.

**Parameters**

| Name    | Type                   | Description |
|---------|------------------------|-------------|
| `h`     | `const unsigned char*` | Expected authentication tag (`CRYPTO_BYTES` bytes). |
| `in`    | `const unsigned char*` | Input message whose tag is being verified. |
| `inlen` | `unsigned long long`   | Length of `in` in bytes. |
| `k`     | `const unsigned char*` | Secret key buffer (16 bytes / 128 bits). |

**Returns**  
- `int` — `0` if the tag matches; `-1` if it does not.

**Global State Modified**  
- None.

**Notes**  
- Comparison is constant-time with respect to tag contents and length (`CRYPTO_BYTES`).  
- Both `h` and the recomputed tag are exactly `CRYPTO_BYTES` bytes.

