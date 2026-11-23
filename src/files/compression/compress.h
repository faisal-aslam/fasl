#pragma once

#include <stdint.h>
#include <stdio.h>

/* Appends one framed block to `out`. Returns bytes appended, or 0 on error. */
long write_compressed_output(FILE *out, const uint8_t *block, uint32_t block_size);
