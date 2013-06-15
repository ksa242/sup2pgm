#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "pgm.h"


void pgm_clear(unsigned char* img, size_t width, size_t height) {
    memset(img, 0x00, width * height);
}


void pgm_clear_region(unsigned char* img, size_t width, size_t height,
                      size_t region_width, size_t region_height,
                      size_t region_x, size_t region_y) {
    size_t x, y,
           max_x = region_x + region_width,
           max_y = region_y + region_height;
    for (y = region_y; y < max_y; y++) {
        for (x = region_x; x < max_x; x++) {
            img[y * width + x] = 0x00;
        }
    }
}


int pgm_write(FILE* fd, const unsigned char* img, size_t width, size_t height) {
    size_t i, n, saved;

    size_t img_len = width * height;

    unsigned char max_gray = 0x00;

    for (i = 0; i < img_len; i++) {
        if (img[i] > max_gray) {
            max_gray = img[i];
        }
    }

    fprintf(fd, "P5\n");
    fprintf(fd, "%lu %lu\n", width, height);
    fprintf(fd, "%u\n", max_gray);

    n = 0;
    saved = 0;

    while (saved < img_len) {
        n = fwrite(img + saved, 1, img_len - saved, fd);
        if (n <= 0) {
            perror("pgm_write()");
            break;
        }

        saved += n;
    }

    return saved == img_len ? 0 : -1;
}
