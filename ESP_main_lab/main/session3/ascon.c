#include <stdio.h>
#include <string.h>
#include "aead.h"
#include "hash.h"

void print_hex(const unsigned char *x, int n)
{
    for (int i = 0; i < n; i++)
        printf("%02X", x[i]);
    printf("\n");
}

void app_main()
{
    unsigned char key[16]   = {0};   /* 128-bit key (zeros = reproducible demo only) */
    unsigned char nonce[16] = {0};   /* 128-bit nonce, must never repeat per key */
    unsigned char ad[]      = "frame-header";   /* associated data: authenticated, NOT encrypted */
    unsigned char msg[]     = "Hello ESP32!";
    unsigned char cipher[64], tampered[64], plain[64], digest[32];

    unsigned long long clen, plen;
    int len    = strlen((char *)msg);
    int ad_len = strlen((char *)ad);

    /* ---------- AEAD128 encryption ---------- */
    ascon_aead_encrypt(cipher, &clen, msg, len, ad, ad_len, nonce, key);

    printf("--- ASCON-AEAD128 ---\n");
    printf("Plaintext : %s (%d bytes)\n", msg, len);
    printf("AD        : %s\n", ad);
    printf("Ciphertext: ");
    print_hex(cipher, clen);
    printf("Cipher len: %llu bytes (= %d message + 16 tag)\n", clen, len);

    /* ---------- Test 1: correct decryption ---------- */
    if (ascon_aead_decrypt(plain, &plen, cipher, clen, ad, ad_len, nonce, key) == 0) {
        plain[plen] = '\0';
        printf("Decrypted : %s\n", plain);
        printf("Status    : SUCCESS (tag verified)\n");
    } else {
        printf("Status    : FAILED (unexpected)\n");
    }

    /* ---------- Test 2: flip one ciphertext bit ---------- */
    memcpy(tampered, cipher, clen);
    tampered[0] ^= 0x01;
    if (ascon_aead_decrypt(plain, &plen, tampered, clen, ad, ad_len, nonce, key) != 0)
        printf("Tampered ciphertext : REJECTED (as expected)\n");
    else
        printf("Tampered ciphertext : ACCEPTED (BUG!)\n");

    /* ---------- Test 3: change the associated data ---------- */
    unsigned char bad_ad[] = "frame-headeX";
    if (ascon_aead_decrypt(plain, &plen, cipher, clen, bad_ad, ad_len, nonce, key) != 0)
        printf("Modified header (AD): REJECTED (as expected)\n");
    else
        printf("Modified header (AD): ACCEPTED (BUG!)\n");

    /* ---------- HASH256 ---------- */
    hash(digest, msg, len);

    printf("\n--- ASCON-HASH256 ---\n");
    printf("Message: %s\n", msg);
    printf("Hash   : ");
    print_hex(digest, 32);
}