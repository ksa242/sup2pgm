#include <stdio.h>
#include <string.h>

#include "srt.h"


int srt_render_time(unsigned long ms, char* buf) {
    unsigned int hour = ms / (60 * 60 * 1000);
    unsigned short min = (ms / (60 * 1000)) % 60;
    unsigned short sec = (ms / 1000) % 60;
    ms = ms - 1000 * (ms / 1000);

    if (hour > 99) {
        fprintf(stderr, "PTS should be less than 99 hours.\n");
        return -1;
    }

    memset(buf, 0x00, SRT_TIMECODE_LEN + 1);

    if (sprintf(buf, "%02u:%02u:%02u,%03lu",
                hour, min, sec, ms) != SRT_TIMECODE_LEN) {
        return -1;
    }

    return 0;
}
