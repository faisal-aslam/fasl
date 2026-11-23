// fasl_format.h - container format shared by compressor and decompressor
#pragma once

#include <stdint.h>
#include <stdio.h>

#define FASL_MAGIC "FASL"
#define FASL_VERSION 1
#define FASL_FILE_HEADER_SIZE 5
#define FASL_MAX_BLOCK_BYTES (1u << 24) /* decoder sanity cap */

static inline int fasl_put_u32(FILE *f, uint32_t v) {
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    return fwrite(b, 1, 4, f) == 4;
}

/* returns 1 = ok, 0 = clean EOF (no bytes read), -1 = truncated/error */
static inline int fasl_get_u32(FILE *f, uint32_t *v) {
    uint8_t b[4];
    size_t n = fread(b, 1, 4, f);
    if (n == 0 && feof(f)) return 0;
    if (n != 4) return -1;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}
