// main.c

#include "constants.h"
#include "fasl_format.h"
#include "logic.h"
#include "timer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

long total_input_size;
char *output_file = NULL;
FILE *output_fp = NULL;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    init_timer();

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    output_file = malloc(strlen(argv[2]) + 1);
    if (!output_file) {
        perror("Failed to allocate memory for output filename");
        fclose(file);
        return 1;
    }
    strcpy(output_file, argv[2]);

    /* Open the output ONCE; blocks are appended. */
    output_fp = fopen(output_file, "wb");
    if (!output_fp) {
        perror("Failed to open output file");
        free(output_file);
        fclose(file);
        return 1;
    }
    uint8_t fhdr[FASL_FILE_HEADER_SIZE] = {'F', 'A', 'S', 'L', FASL_VERSION};
    if (fwrite(fhdr, 1, sizeof fhdr, output_fp) != sizeof fhdr) {
        perror("Failed to write file header");
        return 1;
    }

    fseek(file, 0L, SEEK_END);
    total_input_size = ftell(file);
    rewind(file);

    uint8_t *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Failed to allocate memory for block");
        return 1;
    }

    long total_processed = 0;
    int last_percent = -1;

    while (1) {
        size_t bytesRead = fread(block, 1, BLOCK_SIZE, file);
        if (bytesRead == 0) break;

        if (bytesRead < BLOCK_SIZE) memset(block + bytesRead, 0, BLOCK_SIZE - bytesRead);

        long size_of_block = process_block(block, (uint32_t)bytesRead);
        if (size_of_block == 0) {
            fprintf(stderr, "\nError: failed to compress block at input offset %ld\n", total_processed);
            fclose(output_fp);
            free(block);
            free(output_file);
            fclose(file);
            return 1;
        }

        total_processed += (long)bytesRead;
        if (total_input_size > 0) {
            int percent = (int)(((double)total_processed / total_input_size) * 100.0);
            if (percent != last_percent) {
                printf("\rTotal Progress: %3d%%", percent);
                fflush(stdout);
                last_percent = percent;
            }
        }
    }
    printf("\rTotal Progress: 100%%\n");

    long compressed_size = ftell(output_fp);
    fclose(output_fp);

    if (total_input_size == 0) {
        printf("Input file is empty.\n");
    } else {
        double saving = 1 - ((double)compressed_size / (double)total_input_size);
        printf("Original size:   %ld bytes\n", total_input_size);
        printf("Compressed size: %ld bytes\n", compressed_size);
        printf("Space saving %.2f%%\n", saving * 100);
    }

    free(block);
    free(output_file);
    fclose(file);
    return 0;
}
