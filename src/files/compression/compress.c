// compress.c - builds one block's stream, range-codes it, appends a framed block

#include "compress.h"
#include "best_path_view.h"
#include "bit_writer.h"
#include "code_map.h"
#include "compressed_body.h"
#include "compressed_header.h"
#include "fasl_format.h"
#include "range_coder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

/* Runs header+body writers into a temp file and returns the bytes (caller frees). */
static uint8_t *build_inner_stream(const uint8_t *block, size_t *out_len) {
    FILE *tmp = tmpfile();
    if (!tmp) {
        perror("tmpfile");
        return NULL;
    }
    uint8_t *scratch = malloc(BUFFER_SIZE);
    if (!scratch) {
        fclose(tmp);
        return NULL;
    }

    BitWriter writer;
    bitwriter_init(&writer, scratch, BUFFER_SIZE);
    populate_header(view, block, tmp, &writer);
    populate_body(view, block, tmp, &writer);
    bitwriter_reset(&writer);
    free(scratch);
    free_code_map(&code_map); /* was leaked once per block before */

    fflush(tmp);
    long sz = ftell(tmp);
    if (sz <= 0) {
        fclose(tmp);
        return NULL;
    }
    rewind(tmp);
    uint8_t *data = malloc((size_t)sz);
    if (!data || fread(data, 1, (size_t)sz, tmp) != (size_t)sz) {
        free(data);
        fclose(tmp);
        return NULL;
    }
    fclose(tmp);
    *out_len = (size_t)sz;
    return data;
}

long write_compressed_output(FILE *out, const uint8_t *block, uint32_t block_size) {
    if (!out || !block) {
        fprintf(stderr, "Error: Invalid inputs in write_compressed_output\n");
        return 0;
    }

    size_t inner_len = 0;
    uint8_t *inner = build_inner_stream(block, &inner_len);
    if (!inner) {
        fprintf(stderr, "Error: failed to build block stream\n");
        return 0;
    }

    /* Try both range-coder orders; keep whichever is smallest (or raw). */
    uint8_t mode = RC_MODE_RAW;
    const uint8_t *payload = inner;
    size_t payload_len = inner_len;
    uint8_t *best_buf = NULL;

    for (int order = 0; order <= 1; ++order) {
        uint8_t *buf = malloc(inner_len);
        if (!buf) continue;
        size_t n = rc_encode_bytes(inner, inner_len, order, buf, payload_len - 1);
        if (n > 0 && n < payload_len) {
            free(best_buf);
            best_buf = buf;
            payload = buf;
            payload_len = n;
            mode = (uint8_t)(order + 1);
        } else {
            free(buf);
        }
    }

    int ok = fasl_put_u32(out, block_size) && fasl_put_u32(out, (uint32_t)inner_len) &&
             fwrite(&mode, 1, 1, out) == 1 && fasl_put_u32(out, (uint32_t)payload_len) &&
             fwrite(payload, 1, payload_len, out) == payload_len;

    printf("Block: orig=%u bytes, fasl=%zu bytes, after range coding=%zu bytes (mode %u)\n", block_size, inner_len,
           payload_len, mode);

    free(best_buf);
    free(inner);
    if (!ok) {
        fprintf(stderr, "Error: failed writing block to output\n");
        return 0;
    }
    return (long)(4 + 4 + 1 + 4 + payload_len);
}
