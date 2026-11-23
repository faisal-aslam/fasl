// decompress.c

#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress.h"
#include "decompress_body.h"
#include "decompress_header.h"
#include "fasl_format.h"
#include "range_coder.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }
    printf("Decompressing %s to %s...\n", argv[1], argv[2]);
    read_compressed_file(argv[1], argv[2]);
    printf("Done Decompression.\n");
    return 0;
}

/* returns 1 = block decoded, 0 = clean EOF, -1 = error */
static int decode_one_block(FILE *fin, FILE *fout) {
    uint32_t orig_len = 0, inner_len = 0, payload_len = 0;
    uint8_t mode = 0;

    int r = fasl_get_u32(fin, &orig_len);
    if (r == 0) return 0;
    if (r < 0 || fasl_get_u32(fin, &inner_len) != 1 || fread(&mode, 1, 1, fin) != 1 ||
        fasl_get_u32(fin, &payload_len) != 1) {
        fprintf(stderr, "Truncated block header\n");
        return -1;
    }
    if (orig_len > FASL_MAX_BLOCK_BYTES || inner_len > FASL_MAX_BLOCK_BYTES || payload_len > FASL_MAX_BLOCK_BYTES) {
        fprintf(stderr, "Corrupt block header (sizes out of range)\n");
        return -1;
    }

    uint8_t *payload = malloc(payload_len ? payload_len : 1);
    if (!payload || fread(payload, 1, payload_len, fin) != payload_len) {
        fprintf(stderr, "Truncated block payload\n");
        free(payload);
        return -1;
    }

    uint8_t *inner = NULL;
    int inner_owned = 0;
    if (mode == RC_MODE_RAW) {
        if (payload_len != inner_len) {
            fprintf(stderr, "Corrupt raw block\n");
            free(payload);
            return -1;
        }
        inner = payload;
    } else if (mode == RC_MODE_ORDER0 || mode == RC_MODE_ORDER1) {
        inner = malloc(inner_len ? inner_len : 1);
        inner_owned = 1;
        if (!inner || !rc_decode_bytes(payload, payload_len, mode - 1, inner, inner_len)) {
            fprintf(stderr, "Range decoder failed\n");
            free(inner);
            free(payload);
            return -1;
        }
    } else {
        fprintf(stderr, "Unknown block mode %u\n", mode);
        free(payload);
        return -1;
    }

    BitReader reader = {0};
    bitreader_init(&reader, inner, inner_len);
    global_class2_bits = 0;
    global_rle_bits = 0;

    read_header_and_create_decoder_map(&reader);
    read_body_using_decoder_map(&reader, fout, orig_len);

    bitreader_close(&reader);
    free_decoder_map(&decoder_map);
    free_decoder_codebook_pool();

    if (inner_owned) free(inner);
    free(payload);
    return 1;
}

void read_compressed_file(const char *input_file_name, const char *output_file_name) {
    if (!input_file_name || !output_file_name) {
        fprintf(stderr, "Error: Invalid file names\n");
        return;
    }
    FILE *fin = fopen(input_file_name, "rb");
    if (!fin) {
        perror("Failed to open input file");
        return;
    }
    uint8_t hdr[FASL_FILE_HEADER_SIZE];
    if (fread(hdr, 1, sizeof hdr, fin) != sizeof hdr || memcmp(hdr, FASL_MAGIC, 4) != 0 ||
        hdr[4] != FASL_VERSION) {
        fprintf(stderr, "Not a FASL file (bad magic/version)\n");
        fclose(fin);
        return;
    }
    FILE *fout = fopen(output_file_name, "wb");
    if (!fout) {
        perror("Failed to open output file");
        fclose(fin);
        return;
    }

    int r, blocks = 0;
    while ((r = decode_one_block(fin, fout)) == 1) blocks++;
    if (r < 0) fprintf(stderr, "Decompression failed after %d block(s)\n", blocks);
    else printf("Decompressed %d block(s)\n", blocks);

    fclose(fout);
    fclose(fin);
}
