#pragma once

#include <stdint.h>
#include <stdio.h>

extern char* output_file;
extern FILE* output_fp;

long process_block(const uint8_t *block, uint32_t block_size);
