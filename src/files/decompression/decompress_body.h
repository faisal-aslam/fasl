// decompress_body.h
#pragma once

#include "bit_reader.h"
#include <stdint.h>
#include <stdio.h>

/* Decodes one block's body, writing exactly `expected_bytes` bytes to `out`. */
void read_body_using_decoder_map(BitReader *reader, FILE *out, uint32_t expected_bytes);
