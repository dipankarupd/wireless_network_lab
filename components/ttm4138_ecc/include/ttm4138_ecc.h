#include <stdint.h>

int ecc_isZero(const uint32_t* A, uint8_t length);
void ecc_fieldNegationModO(const uint32_t* a, uint32_t* result, uint8_t length);
void ecc_fieldAddModO(const uint32_t* a, const uint32_t* b, uint32_t* result, uint8_t length);
