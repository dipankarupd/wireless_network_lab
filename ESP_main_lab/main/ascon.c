
#include <stdio.h>
#include <string.h>
#include "aead.h"
#include "hash.h"

void print_hex(unsigned char *x, int n)
{
    for (int i = 0; i < n; i++)
        printf("%02X", x[i]);
    printf("\n");
}

void app_main()
{
    unsigned char key[16] = {0};
    unsigned char nonce[16] = {0};
    unsigned char msg[] = "Hello ESP32!";
    unsigned char cipher[64], plain[64], digest[32];

    unsigned long long clen, plen;
    int len = strlen((char *)msg);

    // AEAD128 encryption
    ascon_aead_encrypt(cipher, &clen, msg, len,
                       NULL, 0, nonce, key);

    printf("--- ASCON-AEAD128 ---\n");
    printf("Plaintext : %s\n", msg);
    printf("Ciphertext: ");
    print_hex(cipher, clen);

    // AEAD128 decryption
    if (ascon_aead_decrypt(plain, &plen, cipher, clen,
                           NULL, 0, nonce, key) == 0) {
        plain[plen] = '\0';
        printf("Decrypted : %s\n", plain);
        printf("Status    : SUCCESS\n");
    }

    // HASH256
    hash(digest, msg, len);

    printf("\n--- ASCON-HASH256 ---\n");
    printf("Message: %s\n", msg);
    printf("Hash: ");
    print_hex(digest, 32);
}
