/*
 * Session 8, Task 6 — offline dictionary attack on the handshake capture.
 *
 * Recovers the password from Session8_Capture/Session_8_pcap.pcapng without any
 * device. It reuses the lab's own Ascon code (hash, prf, ascon_aead_decrypt).
 *
 * The key is derived only from the password plus values sent in clear (Fig.3):
 *     PMK       = Ascon-hash(password)[:16]
 *     ascon_key = prf(PMK, prf_seed),  prf_seed = S || A || ap_nonce || sta_nonce
 * So we guess <Norwegian name> + <common suffix>, rebuild the key, and check it
 * against a tag captured in message m2.  A match = the password.
 *
 * Build (one command, no Makefile), from this folder:
 *   cc -O2 -I../../components/auth/include -I../../components/aead/include \
 *      -I../../components/aead -I../../components/hash/include -o crack crack.c \
 *      ../../components/aead/aead.c ../../components/aead/printstate.c \
 *      ../../components/hash/hash.c ../../components/auth/prf.c
 * Run:  ./crack
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int hash(unsigned char *out, const unsigned char *in, unsigned long long inlen);
int prf(unsigned char *out, unsigned long long outlen,
        const unsigned char *in, unsigned long long inlen, const unsigned char *k);
int ascon_aead_decrypt(unsigned char *m, unsigned long long *mlen,
                       const unsigned char *c, unsigned long long clen,
                       const unsigned char *ad, unsigned long long adlen,
                       const unsigned char *npub, const unsigned char *k);

/* ---- cleartext values read out of the pcap (both MACs + both nonces) ---- */
static const uint8_t S[6]  = {0xf0,0xf5,0xbd,0x01,0x88,0x60};             /* station id   */
static const uint8_t A[6]  = {0xf0,0xf5,0xbd,0x01,0x7c,0x04};             /* AP id        */
static const uint8_t nA[8] = {0x86,0x4e,0xc3,0x29,0x4e,0x08,0x63,0x2a};   /* ap_nonce     */
static const uint8_t nS[8] = {0xe5,0x52,0xe4,0x0f,0x8e,0xfb,0x8d,0x47};   /* station_nonce*/
/* message m2: its authentication tag and nonce, also from the pcap */
static const uint8_t tag[16]  = {0x85,0x4e,0x8a,0x5c,0xd1,0x48,0xbf,0x3c,
                                 0xfc,0xad,0x09,0xc0,0x5b,0x00,0xf3,0xbf};
static const uint8_t npub[16] = {0x64,0x92,0xc5,0x5b,0x3f,0x35,0xc9,0xb7,
                                 0x7a,0xff,0x20,0x8b,0xd6,0x75,0x51,0xbb};

/* ~20 common Norwegian first names (male + female). I am not Norwegian, so these
 * were suggested by an AI as commonly known names. A real attack would use a full
 * name list (e.g. from Statistics Norway). */
static const char *names[] = {
    /* male   */ "Ole","Lars","Per","Nils","Erik","Bjorn","Knut","Arne","Svein","Jan",
                 "Tor","Geir","Odd","Kjell","Morten","Anders","Hans","Harald","Magnus","Olav",
    /* female */ "Kari","Anne","Ingrid","Astrid","Berit","Marit","Hanne","Nina","Liv","Randi",
                 "Solveig","Bente","Inger","Tove","Hilde","Else","Siri","Maria","Sofie","Nora",
};
/* common password suffixes */
static const char *suffixes[] = {
    "","1","12","123","1234","12345","123456","!","123!","2023","2024","2025","01","00","007",
};

int main(void) {
    uint8_t seed[28];                                   /* prf_seed = S||A||nA||nS */
    memcpy(seed,    S,  6); memcpy(seed+6,  A,  6);
    memcpy(seed+12, nA, 8); memcpy(seed+20, nS, 8);

    uint8_t ad[14];                                     /* m2 authenticated data = S||nS */
    memcpy(ad, S, 6); memcpy(ad+6, nS, 8);

    unsigned long tried = 0;
    for (int n = 0; n < (int)(sizeof names / sizeof *names); n++) {
        for (int s = 0; s < (int)(sizeof suffixes / sizeof *suffixes); s++) {
            char pw[64];
            snprintf(pw, sizeof pw, "%s%s", names[n], suffixes[s]);
            tried++;

            uint8_t h[32], pmk[16], key[16], out[16];
            unsigned long long olen;
            hash(h, (const uint8_t *)pw, strlen(pw) + 1);  /* +1: code hashes sizeof(password), incl. '\0' */
            memcpy(pmk, h, 16);
            prf(key, 16, seed, sizeof seed, pmk);

            if (ascon_aead_decrypt(out, &olen, tag, 16, ad, 14, npub, key) == 0) {
                printf("PASSWORD FOUND after %lu guesses: \"%s\"\n", tried, pw);
                printf("ascon_key = ");
                for (int i = 0; i < 16; i++) printf("%02x", key[i]);
                printf("\n");
                return 0;
            }
        }
    }
    printf("no match after %lu guesses\n", tried);
    return 1;
}
