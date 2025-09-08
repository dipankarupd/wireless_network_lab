# Hash API — Arguments, Return Values, Purpose & Global Side Effects

### hash

```c
int hash(unsigned char *out, const unsigned char *in, unsigned long long inlen);
```

**Purpose**  
Compute the ASCON hash (HASH256) of the input message and write the fixed-size digest to `out`.

**Parameters**

| Name     | Type                     | Description |
|----------|--------------------------|-------------|
| `out`    | `unsigned char*`         | Output buffer for the hash digest. Must have space for `ASCON_HASH_SIZE` bytes. The function writes exactly that many bytes. |
| `in`     | `const unsigned char*`   | Pointer to the input message bytes to be hashed. |
| `inlen`  | `unsigned long long`     | Length of the input message in bytes. |

**Returns**  
- `int` — `0` on success.

**Global State Modified**  
- None (operates on local state only; does not allocate or free global resources).

**Notes**  
- The digest length is fixed at compile time by the library constant `ASCON_HASH_SIZE`.  
- The function does not allocate the output buffer; the caller must provide `out` with sufficient space.

