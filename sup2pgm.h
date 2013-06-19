#ifndef SUP2PGM_H
#define SUP2PGM_H

#include <stdio.h>


#define SUP2PGM_PROGRAM_NAME "sub2pgm"
#define SUP2PGM_VERSION "0.0.3"

/* Merge changes happening within 200 ms together. */
#define SUP2PGM_MERGE_THRESHOLD 200

#define DEBUG(...) fprintf(stdout, __VA_ARGS__)
#define ERROR(...) fprintf(stderr, __VA_ARGS__)


struct subimage {
    size_t max_len;
    size_t len;
    unsigned char* img;
};


#endif  /* SUP2PGM_H */
