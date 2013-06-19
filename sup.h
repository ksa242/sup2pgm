#ifndef SUP2PGM_SUP_H
#define SUP2PGM_SUP_H

#include <stdint.h>
#include <stdio.h>


#define SUP_PACKET_MARKER 0x5047  /* "PG" */
#define SUP_PACKET_MAX_SEGMENT_LEN 0xffff

#define SUP_SEGMENT_PCS 0x16    /* Composition info */
#define SUP_SEGMENT_PDS 0x14    /* Palette*/
#define SUP_SEGMENT_WDS 0x17    /* Windows info */
#define SUP_SEGMENT_ODS 0x15    /* Caption image */
#define SUP_SEGMENT_END 0x80    /* End of composition */
#define SUP_SEGMENT_UNKNOWN -1

#define SUP_PCS_STATE_NORMAL 0x00          /* Normal: doesn't have to be complete */
#define SUP_PCS_STATE_ACQU_POINT 0x40      /* Acquisition point */
#define SUP_PCS_STATE_EPOCH_START 0x80     /* Epoch start, clears the screen */
#define SUP_PCS_STATE_EPOCH_CONTINUE 0xc0  /* Epoch continue */
#define SUP_PCS_STATE_UNKNOWN -1

#define SUP_PCS_PALETTE_UPDATED 0x80
#define SUP_PCS_OBJ_FORCED 0x40
#define SUP_PCS_OBJ_CROPPED 0x80

#define SUP_ODS_FIRST 0x80
#define SUP_ODS_LAST 0x40

#define SUP_FPS_23_976 0x10  /* 23.976fps */
#define SUP_FPS_FILM 0x20    /* 24.000fps */
#define SUP_FPS_PAL 0x30     /* 25.000fps */
#define SUP_FPS_NTSC 0x40    /* 29.970fps */
#define SUP_FPS_PAL_I 0x60   /* 50.000fps interlaced */
#define SUP_FPS_NTSC_I 0x70  /* 59.940fps interlaced */
#define SUP_FPS_UNKNOWN -1

#define SUP_PTS_FREQ 90  /* 90 kHz */


struct sup_packet {
    uint16_t marker;          /* Packet marker: "PG" */
    uint32_t pts;             /* PTS - presentation time stamp */
    uint32_t dts;             /* DTS - decoding time stamp */
    uint8_t segment_type;     /* Segment type */
    uint16_t segment_len;  /* Segment length (bytes following until next PG) */
    void* segment;
};


/**
 * Segment type 0x14: palette info.
 */
struct sup_color {
    uint8_t idx;
    uint8_t y;
    uint8_t cr;
    uint8_t cb;
    uint8_t a;
    uint8_t gray;
};


struct sup_segment_pds {
    uint16_t palette_id;
    uint8_t num_of_colors;
    struct sup_color* colors;
};


/**
 * Segment type 0x15: caption image.
 */
struct sup_segment_ods {
    uint16_t obj_id;
    uint8_t obj_version;
    uint8_t obj_flag;
    uint32_t obj_data_len;
    uint16_t obj_width;
    uint16_t obj_height;
    void* raw_data;
    uint32_t raw_data_len;
};


/**
 * Segment type 0x16: composition info.
 */
struct sup_object {
    uint16_t obj_id;
    uint8_t win_id;
    uint8_t obj_flag;
    uint16_t obj_pos_x;
    uint16_t obj_pos_y;
};


struct sup_segment_pcs {
    uint32_t pts_msec;
    uint16_t video_width;
    uint16_t video_height;
    uint8_t frame_rate;
    uint16_t comp_id;
    uint8_t comp_state;
    uint8_t palette_flag;
    uint8_t palette_id;
    uint8_t num_of_objects;
    struct sup_object* objects;
};


/**
 * Segment type 0x17: window info.
 */
struct sup_window {
    uint8_t win_id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};


struct sup_segment_wds {
    uint8_t num_of_windows;
    struct sup_window* windows;
};


float sup_frame_rate_by_id(uint8_t frame_rate_id);
unsigned long sup_pts_to_ms(uint32_t pts);

int sup_init_packet(struct sup_packet* packet);
int sup_read_packet(FILE* fd, struct sup_packet* packet);

int sup_init_segment_pcs(struct sup_segment_pcs* pcs);
int sup_parse_segment_pcs(const struct sup_packet* packet, struct sup_segment_pcs* pcs);

int sup_init_segment_pds(struct sup_segment_pds* pds);
int sup_parse_segment_pds(const struct sup_packet* packet, struct sup_segment_pds* pds);

int sup_init_segment_wds(struct sup_segment_wds* wds);
int sup_parse_segment_wds(const struct sup_packet* packet, struct sup_segment_wds* wds);

int sup_init_segment_ods(struct sup_segment_ods* ods);
int sup_parse_segment_ods(const struct sup_packet* packet, struct sup_segment_ods* ods);

#endif  /* SUP2PGM_SUP_H */
