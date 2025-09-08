#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "printstate.h"

/**
 * @brief Print a string to standard output.
 *
 * A thin wrapper around @c printf that writes the given NUL-terminated
 * string @p text without adding a newline or formatting.
 *
 * @param[in] text  NUL-terminated string to print.
 *
 * @note Unlike @c puts(), this function does not append a newline.
 * @note This function is typically used for debug output in the ASCON
 *       reference implementation.
 */
void print(const char* text) { printf("%s", text); }

/**
 * @brief Print a byte array in hexadecimal format with a label.
 *
 * Produces output of the form:
 *
 *   <text>[<len>] = {0x00, 0x11, 0x22, ...}
 *
 * where each byte is shown as two-digit lowercase hex with a @c 0x prefix
 * and separated by commas.
 *
 * @param[in] text  Label to prefix the output (e.g., "key", "msg").
 * @param[in] b     Pointer to the byte array to print.
 * @param[in] len   Number of bytes in @p b to print.
 *
 * @note A trailing newline is appended at the end of the output.
 * @note This function is intended for debugging and tracing internal state
 *       (e.g., keys, messages, tags) and should not be used in production
 *       builds where leaking secret material must be avoided.
 */
void printbytes(const char* text, const uint8_t* b, uint64_t len) {
  uint64_t i;
  printf(" %s[%" PRIu64 "]\t= {", text, len);
  for (i = 0; i < len; ++i) printf("0x%02x%s", b[i], i < len - 1 ? ", " : "");
  printf("}\n");
}

/**
 * @brief Print a single 64-bit word in hexadecimal format with a label.
 *
 * Produces output of the form:
 *
 *   <text>=0x0123456789abcdef
 *
 * where the 64-bit word is shown as 16 lowercase hexadecimal digits,
 * zero-padded as needed.
 *
 * @param[in] text  Label to prefix the output (e.g., "x0", "key0").
 * @param[in] x     64-bit word to print.
 *
 * @note No newline is appended automatically. If multiple words are
 *       printed in sequence, the caller may need to insert separators
 *       or newlines manually.
 * @note This function is intended for debugging and should not be used
 *       in production code that handles secret data.
 */
void printword(const char* text, const uint64_t x) {
  printf("%s=0x%016" PRIx64, text, x);
}

/**
 * @brief Print the full ASCON permutation state with a label.
 *
 * Displays the label @p text followed by the five 64-bit words of the
 * ASCON state in hexadecimal. Each word is printed using @ref printword()
 * in the format " xN=0x0123456789abcdef", aligned to make output easier
 * to read.
 *
 * Example output:
 * @code
 * initialization:   x0=0x80400c0600000000 x1=0x0000000000000000
 *                   x2=0x0000000000000000 x3=0x0000000000000000
 *                   x4=0x0000000000000000
 * @endcode
 *
 * @param[in] text  Label to print before the state (e.g., "initial value").
 * @param[in] s     Pointer to the ASCON state structure containing 5 words.
 *
 * @note A newline is appended at the end of the printed state.
 * @note This function is intended for debugging and tracing the algorithm’s
 *       internal state. It must not be used in production code, as printing
 *       sensitive intermediate values can leak secrets.
 */
void printstate(const char* text, const ascon_state_t* s) {
  int i;
  printf("%s:", text);
  for (i = (int) strlen(text); i < 17; ++i) printf(" ");
  printword(" x0", s->x[0]);
  printword(" x1", s->x[1]);
  printword(" x2", s->x[2]);
  printword(" x3", s->x[3]);
  printword(" x4", s->x[4]);
  printf("\n");
}


