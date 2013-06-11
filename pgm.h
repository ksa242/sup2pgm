#ifndef PGM2PGM_PGM_H
#define PGM2PGM_PGM_H

void pgm_clear(unsigned char* img, size_t width, size_t height);

void pgm_clear_region(unsigned char* img, size_t width, size_t height,
                      size_t region_width, size_t region_height,
                      size_t region_x, size_t region_y);

int pgm_write(FILE* fd, unsigned char* img, size_t width, size_t height);

#endif  /* PGM2PGM_PGM_H */
