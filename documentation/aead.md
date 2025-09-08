# ASCON-128 AEAD API — Arguments, Return Values, Purpose & Global Side Effects

Each entry provides a brief **purpose**, the **arguments** and **return values**, and any **global state** the function modifies (none for these routines). Cross-references use `#` links for quick navigation.

---

### ascon_aead_encrypt

```c
int ascon_aead_encrypt(unsigned char *c, unsigned long long *clen, const unsigned char *m, unsigned long long mlen, const unsigned char *ad, unsigned long long adlen, const unsigned char *npub, const unsigned char *k);
```

**Purpose**  
Encrypt plaintext `m` with key `k` and public nonce `npub`, authenticate associated data `ad`, and output `c` = ciphertext ‖ tag. Implements **ASCON-128 AEAD** (12/8 round variant).

**Parameters**

| Name    | Type                     | Description |
|---------|--------------------------|-------------|
| `c`     | `unsigned char*`         | Output buffer for ciphertext followed by the authentication tag. Must have space for `mlen + CRYPTO_ABYTES` bytes. |
| `clen`  | `unsigned long long*`    | Output length pointer. Set on return to `mlen + CRYPTO_ABYTES`. |
| `m`     | `const unsigned char*`   | Input plaintext to encrypt. |
| `mlen`  | `unsigned long long`     | Length of `m` in bytes. |
| `ad`    | `const unsigned char*`   | Associated data to authenticate (not encrypted). May be `NULL` if `adlen == 0`. |
| `adlen` | `unsigned long long`     | Length of `ad` in bytes. |
| `npub`  | `const unsigned char*`   | Public nonce (`CRYPTO_NPUBBYTES` bytes; typically 16). **Must be unique per key.** |
| `k`     | `const unsigned char*`   | Secret key (`CRYPTO_KEYBYTES` bytes; typically 16). |

**Returns**  
- `int` — `0` on success.

**Global State Modified**  
- None.

**Notes**  
- `c` must not overlap with `m` or `ad`.  
- The tag size is `CRYPTO_ABYTES` (typically 16 for ASCON-128).  
- Never reuse the same `(k, npub)` pair; nonce reuse breaks AEAD security.  
- See [ascon_aead_decrypt](#ascon_aead_decrypt) for the inverse operation.

---

### ascon_aead_decrypt

```c
int ascon_aead_decrypt(unsigned char *m, unsigned long long *mlen, const unsigned char *c, unsigned long long clen, const unsigned char *ad, unsigned long long adlen, const unsigned char *npub, const unsigned char *k);
```

**Purpose**  
Decrypt `c` (ciphertext ‖ tag) under key `k` and nonce `npub`, authenticate `ad`, and, if the tag verifies, write the recovered plaintext to `m`.

**Parameters**

| Name    | Type                     | Description |
|---------|--------------------------|-------------|
| `m`     | `unsigned char*`         | Output buffer for recovered plaintext. Must have space for `clen - CRYPTO_ABYTES` bytes. |
| `mlen`  | `unsigned long long*`    | Output length pointer. On success, set to `clen - CRYPTO_ABYTES`. |
| `c`     | `const unsigned char*`   | Input ciphertext including the trailing tag. |
| `clen`  | `unsigned long long`     | Length of `c` in bytes (ciphertext + tag). Must be ≥ `CRYPTO_ABYTES`. |
| `ad`    | `const unsigned char*`   | Associated data to authenticate (not decrypted). May be `NULL` if `adlen == 0`. |
| `adlen` | `unsigned long long`     | Length of `ad` in bytes. |
| `npub`  | `const unsigned char*`   | Public nonce (`CRYPTO_NPUBBYTES` bytes; typically 16). Must match the one used at encryption. |
| `k`     | `const unsigned char*`   | Secret key (`CRYPTO_KEYBYTES` bytes; typically 16). |

**Returns**  
- `int` — `0` if decryption and tag verification succeed; `-1` otherwise (e.g., `clen < CRYPTO_ABYTES` or tag mismatch).

**Global State Modified**  
- None.

**Notes**  
- `m` must not overlap with `c` or `ad`.  
- Tag verification is implemented in constant time with respect to the tag contents.  
- On failure (`-1`), the contents of `m` are undefined and must not be used.  
- Pair with [ascon_aead_encrypt](#ascon_aead_encrypt).
|
