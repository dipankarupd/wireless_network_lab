#include <stdio.h>
#include <string.h>

// 1. Context Structure
typedef struct {
    unsigned char S[256];
    int i, j;
} rc4_ctx;
// 2. Key-Scheduling Algorithm (KSA)
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
// 3. Pseudo-Random Generation Algorithm (PRGA)
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
// 4. Test Vector Demonstration
void run_test(const char* key, const char* plaintext) {
    rc4_ctx ctx;
    unsigned char buffer[256];
    
    // Copy plaintext to a mutable buffer
    size_t len = strlen(plaintext);
    memcpy(buffer, plaintext, len);
    
    // Initialize RC4 with the key
    rc4_init(&ctx, (const unsigned char*)key, strlen(key));
    
    // Encrypt the buffer
    rc4_crypt(&ctx, buffer, len);
    
    // Print formatted results
    printf("Key:       \"%s\"\n", key);
    printf("Plaintext: \"%s\"\n", plaintext);
    printf("Result:    ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X", buffer[i]);
    }
    printf("\n\n");
}
void app_main() {
    printf("--- RC4 Wikipedia Test Vectors ---\n\n");
    // Test Vector 1
    run_test("Key", "Plaintext");
    // Test Vector 2
    run_test("Wiki", "pedia");
    // Test Vector 3
    run_test("Secret", "Attack at dawn");
    
}