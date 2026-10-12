/*
 * Session 4 — Random Number Generation
 * Combined file: Task 3 (128-bit key), Task 4 (uniform sampling mod p),
 * Task 5 (NIST SP 800-22 Monobit test).
 *
 * Drop this file in main/ as "sess4.c", add it to main/CMakeLists.txt
 * SRCS, and make sure REQUIRES includes esp_hw_support (see notes below).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "esp_random.h"

/* Optional: lets you get true random numbers even without Wi-Fi/BT up.
 * Requires REQUIRES bootloader_support in CMakeLists.txt.
 * Comment out both the #include and the calls in app_main() if you'd
 * rather not add that dependency. */
#include "bootloader_random.h"

/* ------------------------------------------------------------------ */
/* Task 3: random 128-bit key                                          */
/* ------------------------------------------------------------------ */

#define KEY_LEN_BYTES 16   /* 128 bits */

void generate_random_key128(uint8_t key[KEY_LEN_BYTES])
{
    esp_fill_random(key, KEY_LEN_BYTES);
}

static void print_key128(const uint8_t key[KEY_LEN_BYTES])
{
    printf("128-bit key: ");
    for (int i = 0; i < KEY_LEN_BYTES; i++) {
        printf("%02x", key[i]);
    }
    printf("\n");
}

void task3_demo(void)
{
    printf("\n=== Task 3: random 128-bit key ===\n");

    uint8_t key[KEY_LEN_BYTES];
    generate_random_key128(key);
    print_key128(key);

    uint8_t key2[KEY_LEN_BYTES];
    generate_random_key128(key2);
    print_key128(key2);

    if (memcmp(key, key2, KEY_LEN_BYTES) == 0) {
        printf("WARNING: two successive keys were identical - check RNG source!\n");
    } else {
        printf("OK: successive keys differ, as expected.\n");
    }
}

/* ------------------------------------------------------------------ */
/* Task 4: uniform sampling in {0, ..., p-1}                           */
/* p = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff */
/* (the 78-decimal-digit prime from the lab text = NIST P-256 field prime)*/
/* ------------------------------------------------------------------ */

#define P256_LEN_BYTES 32   /* 256 bits */

static const uint8_t P256_PRIME_BE[P256_LEN_BYTES] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* Returns 1 if a >= b, comparing two 32-byte big-endian integers. */
static int be_geq(const uint8_t a[P256_LEN_BYTES], const uint8_t b[P256_LEN_BYTES])
{
    for (int i = 0; i < P256_LEN_BYTES; i++) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return 0;
    }
    return 1; /* equal */
}

void sample_uniform_mod_p(uint8_t out[P256_LEN_BYTES], unsigned *rejections_out)
{
    unsigned rejections = 0;
    do {
        esp_fill_random(out, P256_LEN_BYTES);
        if (!be_geq(out, P256_PRIME_BE)) {
            break; /* accepted: out < p */
        }
        rejections++;
    } while (1);

    if (rejections_out) {
        *rejections_out = rejections;
    }
}

static void be_bytes_to_be_words(const uint8_t be[P256_LEN_BYTES], uint32_t words[8])
{
    for (int w = 0; w < 8; w++) {
        words[w] = ((uint32_t)be[4 * w + 0] << 24) |
                   ((uint32_t)be[4 * w + 1] << 16) |
                   ((uint32_t)be[4 * w + 2] << 8)  |
                   ((uint32_t)be[4 * w + 3]);
    }
}

static void print_be(const uint8_t buf[P256_LEN_BYTES])
{
    for (int i = 0; i < P256_LEN_BYTES; i++) printf("%02x", buf[i]);
    printf("\n");
}

void task4_demo(void)
{
    printf("\n=== Task 4: uniform sample in {0,...,p-1} ===\n");

    uint8_t sample[P256_LEN_BYTES];
    uint32_t words[8];
    unsigned rejections;

    sample_uniform_mod_p(sample, &rejections);

    printf("Sample (hex, big-endian): ");
    print_be(sample);
    printf("Rejected draws before acceptance: %u\n", rejections);

    be_bytes_to_be_words(sample, words);
    printf("As 8 x uint32_t words (word0 = most significant):\n");
    for (int i = 0; i < 8; i++) {
        printf("  word[%d] = 0x%08" PRIx32 "\n", i, words[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Task 5: NIST SP 800-22/1a Frequency (Monobit) Test                  */
/* ------------------------------------------------------------------ */

#define NUM_BYTES  20000            /* 20,000 bytes = 160,000 bits */
#define NUM_BITS   (NUM_BYTES * 8)

static int popcount8(uint8_t b)
{
    int c = 0;
    while (b) {
        c += b & 1;
        b >>= 1;
    }
    return c;
}

double run_monobit_test(long *ones_out, long *zeros_out, double *s_obs_out)
{
    static uint8_t buf[NUM_BYTES]; /* static: too big for the stack */
    esp_fill_random(buf, NUM_BYTES);

    long ones = 0;
    for (int i = 0; i < NUM_BYTES; i++) {
        ones += popcount8(buf[i]);
    }
    long zeros = NUM_BITS - ones;
    long S_n = ones - zeros;

    double s_obs = fabs((double)S_n) / sqrt((double)NUM_BITS);
    double p_value = erfc(s_obs / sqrt(2.0));

    if (ones_out)  *ones_out  = ones;
    if (zeros_out) *zeros_out = zeros;
    if (s_obs_out) *s_obs_out = s_obs;

    return p_value;
}

void task5_demo(void)
{
    printf("\n=== Task 5: Frequency (Monobit) Test, NIST SP 800-22/1a ===\n");

    long ones, zeros;
    double s_obs;
    double p_value = run_monobit_test(&ones, &zeros, &s_obs);

    printf("  n (bits tested)   = %d\n", NUM_BITS);
    printf("  ones              = %ld\n", ones);
    printf("  zeros             = %ld\n", zeros);
    printf("  s_obs             = %f\n", s_obs);
    printf("  P-value           = %f\n", p_value);
    printf("  Result            = %s (threshold 0.01)\n",
           (p_value >= 0.01) ? "PASS" : "FAIL");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    /* Ensures esp_random()/esp_fill_random() return true random data
     * even if Wi-Fi/BT are never started in this app. Comment out (and
     * remove the #include above) if you don't want the extra
     * bootloader_support dependency. */
    bootloader_random_enable();

    task3_demo();
    task4_demo();
    task5_demo();

    bootloader_random_disable();
}