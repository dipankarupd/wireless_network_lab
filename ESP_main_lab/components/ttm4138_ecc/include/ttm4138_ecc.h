#include <stdint.h>

int ecc_isZero(const uint32_t* A, uint8_t length);
void ecc_fieldNegationModO(const uint32_t* a, uint32_t* result, uint8_t length);
void ecc_fieldAddModO(const uint32_t* a, const uint32_t* b, uint32_t* result, uint8_t length);
uint64_t pow_mod(uint64_t b, uint64_t e, uint64_t m);
