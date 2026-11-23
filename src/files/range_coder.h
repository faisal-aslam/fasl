// range_coder.h - adaptive binary range coder (LZMA-style) over bytes
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RC_MODE_RAW 0    /* not range coded */
#define RC_MODE_ORDER0 1 /* rc order 0 */
#define RC_MODE_ORDER1 2 /* rc order 1 (context = previous byte) */

/* order: 0 or 1. Returns compressed size, or 0 if it did not fit in out_cap. */
size_t rc_encode_bytes(const uint8_t *in, size_t n, int order, uint8_t *out, size_t out_cap);

/* Decodes exactly n bytes. Returns false on corrupt/truncated input. */
bool rc_decode_bytes(const uint8_t *in, size_t in_len, int order, uint8_t *out, size_t n);
