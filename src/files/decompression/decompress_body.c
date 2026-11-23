// decompress_body.c

#include "decompress_body.h"
#include "bit_reader.h"
#include "code_classes.h"
#include "decoder_map.h"
#include "decompress_header.h"
#include <stdio.h>
#include <stdlib.h>

extern uint8_t global_class2_bits;
extern uint8_t global_rle_bits;
extern DecoderMap decoder_map;

static uint32_t rd(BitReader *r, uint8_t bits, const char *what) {
    uint32_t v = 0;
    if (!bitreader_read(r, &v, bits)) {
        fprintf(stderr, "Truncated stream: expected %s\n", what);
        exit(EXIT_FAILURE);
    }
    return v;
}

static void need_room(uint32_t total, uint32_t expected, uint32_t count) {
    if (count > expected - total) {
        fprintf(stderr, "Corrupt stream: block would exceed its declared size\n");
        exit(EXIT_FAILURE);
    }
}

void read_body_using_decoder_map(BitReader *reader, FILE *out, uint32_t expected) {
    uint32_t total = 0;

    while (total < expected) {
        if (rd(reader, 1, "prefix bit") == 0) { /* raw byte */
            fputc((int)rd(reader, 8, "raw byte"), out);
            total++;
            continue;
        }

        uint32_t cls = rd(reader, 2, "code class");

        if (cls == 3) { /* RLE */
            if (rd(reader, 1, "RLE flag") == 1) { /* uniform */
                uint32_t count = rd(reader, global_rle_bits > 0 ? global_rle_bits : 8, "RLE count");
                uint8_t sym = (uint8_t)rd(reader, 8, "RLE symbol");
                need_room(total, expected, count);
                for (uint32_t i = 0; i < count; i++) fputc(sym, out);
                total += count;
            } else { /* arithmetic +1 */
                uint32_t start = rd(reader, 8, "arith RLE start");
                uint32_t end = rd(reader, 8, "arith RLE end");
                if (end < start) {
                    fprintf(stderr, "Invalid arithmetic RLE range: %u..%u\n", start, end);
                    exit(EXIT_FAILURE);
                }
                need_room(total, expected, end - start + 1);
                for (uint32_t v = start; v <= end; v++) fputc((int)v, out);
                total += end - start + 1;
            }
        } else if (cls <= 2) {
            uint8_t bits = get_code_class_size((uint8_t)cls, global_class2_bits);
            uint32_t code = rd(reader, bits, "code");
            const uint8_t *seq = NULL;
            uint8_t length = 0;
            if (!decoder_map_get(&decoder_map, (uint16_t)code, (uint8_t)cls, &seq, &length)) {
                fprintf(stderr, "Failed to resolve code=%u class=%u\n", code, cls);
                exit(EXIT_FAILURE);
            }
            need_room(total, expected, length);
            fwrite(seq, 1, length, out);
            total += length;
        } else {
            fprintf(stderr, "Invalid code_class read: %u\n", cls);
            exit(EXIT_FAILURE);
        }
    }
}
