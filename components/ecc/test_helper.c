/*
 * Copyright (c) 2009 Chris K Cockrum <ckc@cockrum.net>
 *
 * Copyright (c) 2013 Jens Trillmann <jtrillma@tzi.de>
 * Copyright (c) 2013 Marc Müller-Weinhardt <muewei@tzi.de>
 * Copyright (c) 2013 Lars Schmertmann <lars@tzi.de>
 * Copyright (c) 2013 Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * This implementation is based in part on the paper Implementation of an
 * Elliptic Curve Cryptosystem on an 8-bit Microcontroller [0] by
 * Chris K Cockrum <ckc@cockrum.net>.
 *
 * [0]: http://cockrum.net/Implementation_of_ECC_on_an_8-bit_microcontroller.pdf
 *
 * This is a efficient ECC implementation on the secp256r1 curve for 32 Bit CPU
 * architectures. It provides basic operations on the secp256r1 curve and support
 * for ECDH and ECDSA.
 */
#include "test_helper.h"
#include "ecc.h"
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void ecc_printNumber(const uint32_t *x, int numberLength){ //here the values are turned to MSB!
	int n;

	for(n = numberLength - 1; n >= 0; n--){
		printf("%08x", (unsigned int)x[n]);
	}
	printf("\n");
}


static const uint32_t ORDER_N[8] = {
    0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD,
    0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF
};

static inline int ge_n(const uint32_t a[8]) {
    for (int i = 7; i >= 0; --i) {
        if (a[i] > ORDER_N[i]) return 1;
        if (a[i] < ORDER_N[i]) return 0;
    }
    return 1; // equal
}
static inline int is_zero_256(const uint32_t a[8]) {
    uint32_t acc = 0;
    for (int i = 0; i < 8; ++i) acc |= a[i];
    return acc == 0;
}

/**
 * @brief Generate a random valid 256-bit secret scalar for secp256r1.
 *
 * Fills @p secret with a uniformly random 256-bit integer suitable for use
 * as a private key in elliptic-curve operations. The candidate is rejected
 * and redrawn if it is zero or greater than or equal to the curve order n.
 *
 * @param[out] secret  Output buffer for the secret scalar (array of 8 × 32-bit
 *                     words in little-endian order).
 *
 * @note Uses @c esp_fill_random() to obtain 256 bits of entropy from the
 *       ESP32 hardware RNG.
 * @note This function loops until a valid scalar is drawn. In practice, the
 *       probability of rejection is negligible (~50% chance of redrawing if
 *       candidate ≥ n, and vanishingly small chance of all-zero).
 *
 * @warning The generated scalar is secret material and must be protected
 *          against leakage. Do not print or log its value in production.
 */
void ecc_setRandom(uint32_t *secret) {
    do {
        esp_fill_random(secret, 32);        // 256 bits of entropy
    } while (ge_n(secret) || is_zero_256(secret));  // reject if >= n or 0
}

const uint32_t ecc_prime_m[8] = {0xffffffff, 0xffffffff, 0xffffffff, 0x00000000,
				 0x00000000, 0x00000000, 0x00000001, 0xffffffff};

							
/* This is added after an static byte addition if the answer has a carry in MSB*/
const uint32_t ecc_prime_r[8] = {0x00000001, 0x00000000, 0x00000000, 0xffffffff,
				 0xffffffff, 0xffffffff, 0xfffffffe, 0x00000000};

