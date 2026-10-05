#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct {
    unsigned char S[256];
    int i, j;
} rc4_ctx;

void rc4_init(rc4_ctx *ctx, const unsigned char *key, size_t key_len) {
    for (int i = 0; i < 256; i++) {
        ctx->S[i] = i;
    }
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + ctx->S[i] + key[i % key_len]) % 256;
        unsigned char tmp = ctx->S[i];
        ctx->S[i] = ctx->S[j];
        ctx->S[j] = tmp;
    }
    ctx->i = 0;
    ctx->j = 0;
}

void rc4_crypt(rc4_ctx *ctx, unsigned char *data, size_t len) {
    for (size_t k = 0; k < len; k++) {
        ctx->i = (ctx->i + 1) % 256;
        ctx->j = (ctx->j + ctx->S[ctx->i]) % 256;

        unsigned char tmp = ctx->S[ctx->i];
        ctx->S[ctx->i] = ctx->S[ctx->j];
        ctx->S[ctx->j] = tmp;

        unsigned char stream_byte = ctx->S[(ctx->S[ctx->i] + ctx->S[ctx->j]) % 256];
        data[k] ^= stream_byte;
    }
}

/* ---- Test vectors from the RC4 Wikipedia page ---- */

typedef struct {
    const char *key;
    const char *plaintext;
    const char *expected_hex;
} rc4_vector;

static const rc4_vector vectors[] = {
    {"Key",    "Plaintext",      "BBF316E8D940AF0AD3"},
    {"Wiki",   "pedia",          "1021BF0420"},
    {"Secret", "Attack at dawn", "45A01F645FC35B383552544B9BF5"},
};

/* Encrypts each vector, prints the ciphertext and checks it against the
 * expected value, then decrypts again to confirm the round trip.
 * Returns the number of failed vectors. */
int run_test_vectors(void) {
    int failures = 0;
    printf("RC4 test vectors (Wikipedia):\n");
    for (size_t v = 0; v < sizeof(vectors) / sizeof(vectors[0]); v++) {
        const rc4_vector *tv = &vectors[v];
        size_t len = strlen(tv->plaintext);
        unsigned char buf[64];
        char hex[2 * sizeof(buf) + 1];

        memcpy(buf, tv->plaintext, len);

        rc4_ctx ctx;
        rc4_init(&ctx, (const unsigned char *)tv->key, strlen(tv->key));
        rc4_crypt(&ctx, buf, len);

        for (size_t k = 0; k < len; k++) {
            sprintf(hex + 2 * k, "%02X", buf[k]);
        }
        int ok = strcmp(hex, tv->expected_hex) == 0;

        rc4_init(&ctx, (const unsigned char *)tv->key, strlen(tv->key));
        rc4_crypt(&ctx, buf, len);
        ok = ok && memcmp(buf, tv->plaintext, len) == 0;

        printf("  Key=%-7s Plaintext=%-15s -> %s  [%s]\n",
               tv->key, tv->plaintext, hex, ok ? "PASS" : "FAIL");
        if (!ok) {
            failures++;
        }
    }
    return failures;
}

/* ---- WEP-mode key construction: Frame_Key = IV || Master_Key ---- */

#define IV_LEN      3    /* WEP used a 24-bit (3-byte) IV */
#define MASTER_LEN  5    
#define WEP_KEY_LEN (IV_LEN + MASTER_LEN)

/* Builds the per-frame/per-file RC4 key: IV concatenated with the master key.
 * This is literally WEP's (broken) key construction — direct concatenation,
 * no KDF — kept this way on purpose so you can see/demonstrate the flaw
 * discussed in checkpoint 2(c), as opposed to your improved KDF-based design. */
void build_wep_key(unsigned char *out_key,
                    const unsigned char iv[IV_LEN],
                    const unsigned char master_key[MASTER_LEN]) {
    memcpy(out_key, iv, IV_LEN);
    memcpy(out_key + IV_LEN, master_key, MASTER_LEN);
}

#ifndef ESP_PLATFORM
/* ---- File helpers (host build only: the ESP32 has no filesystem mounted) ---- */

/* Reads the whole file into a malloc'd buffer. Returns NULL on failure.
 * *out_len receives the number of bytes read. */
unsigned char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen (read)");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    unsigned char *buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }

    *out_len = (size_t)size;
    return buf;
}

int write_file(const char *path, const unsigned char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen (write)");
        return -1;
    }
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

/* ---- Demo: encrypt then decrypt a text file in WEP mode ---- */

int main(int argc, char **argv) {
    if (run_test_vectors() != 0) {
        return 1;
    }
    printf("\n");

    const char *in_path  = (argc > 1) ? argv[1] : "plaintext.txt";
    const char *enc_path = "ciphertext.wep";
    const char *dec_path = "decrypted.txt";

    /* Fixed for a reproducible demo. In a real WEP frame the 3-byte IV is
     * sent in cleartext with every packet so the receiver can rebuild the
     * same key; here we "transmit" it by hardcoding the same value on
     * both the encrypt and decrypt side. */
    unsigned char iv[IV_LEN]          = {0x01, 0x02, 0x03};
    unsigned char master_key[MASTER_LEN] = {'S', 'E', 'C', 'R', 'T'};
    unsigned char wep_key[WEP_KEY_LEN];

    build_wep_key(wep_key, iv, master_key);

    /* --- Read plaintext file --- */
    size_t len;
    unsigned char *data = read_file(in_path, &len);
    if (!data) {
        fprintf(stderr, "Could not read '%s'. Create a text file with that "
                        "name (or pass a path as argv[1]) and re-run.\n", in_path);
        return 1;
    }
    printf("Read %zu bytes from '%s'\n", len, in_path);

    /* --- Encrypt in place --- */
    rc4_ctx ctx;
    rc4_init(&ctx, wep_key, WEP_KEY_LEN);
    rc4_crypt(&ctx, data, len);   /* data now holds ciphertext */

    if (write_file(enc_path, data, len) != 0) {
        fprintf(stderr, "Failed writing '%s'\n", enc_path);
        free(data);
        return 1;
    }
    printf("Encrypted -> '%s' (%zu bytes)\n", enc_path, len);

    /* --- Decrypt: fresh RC4 state, same key, same starting IV/PRGA state ---
     * RC4 is symmetric under XOR, so running rc4_crypt again with a
     * freshly re-initialized context recovers the original plaintext. */
    rc4_init(&ctx, wep_key, WEP_KEY_LEN);
    rc4_crypt(&ctx, data, len);   /* data is back to plaintext */

    if (write_file(dec_path, data, len) != 0) {
        fprintf(stderr, "Failed writing '%s'\n", dec_path);
        free(data);
        return 1;
    }
    printf("Decrypted -> '%s' (%zu bytes)\n", dec_path, len);

    printf("\nCompare '%s' and '%s' (e.g. `diff %s %s`) to confirm the "
           "round trip is exact.\n", in_path, dec_path, in_path, dec_path);

    free(data);
    return 0;
}

#else /* ESP_PLATFORM */

/* On the ESP32 there is no file to read, so run the test vectors and a
 * WEP-mode round trip on an in-memory message instead. */
void app_main(void) {
    run_test_vectors();

    unsigned char iv[IV_LEN]             = {0x01, 0x02, 0x03};
    unsigned char master_key[MASTER_LEN] = {'S', 'E', 'C', 'R', 'T'};
    unsigned char wep_key[WEP_KEY_LEN];
    build_wep_key(wep_key, iv, master_key);

    const char *msg = "Hello from the ESP32, encrypted with RC4 in WEP mode!";
    size_t len = strlen(msg);
    unsigned char buf[128];
    memcpy(buf, msg, len);

    rc4_ctx ctx;
    rc4_init(&ctx, wep_key, WEP_KEY_LEN);
    rc4_crypt(&ctx, buf, len);
    printf("\nWEP ciphertext: ");
    for (size_t k = 0; k < len; k++) {
        printf("%02X", buf[k]);
    }
    printf("\n");

    rc4_init(&ctx, wep_key, WEP_KEY_LEN);
    rc4_crypt(&ctx, buf, len);
    printf("Decrypted:      %.*s [%s]\n", (int)len, buf,
           memcmp(buf, msg, len) == 0 ? "PASS" : "FAIL");
}

#endif /* ESP_PLATFORM */
