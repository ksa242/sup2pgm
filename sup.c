#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <errno.h>

#include "sup.h"


float sup_frame_rate_by_id(uint8_t frame_rate_id) {
    switch (frame_rate_id) {
    case SUP_FPS_23_976:
        return 24000.0 / 1001;

    case SUP_FPS_FILM:
        return 24.0;

    case SUP_FPS_PAL:
        return 25.0;

    case SUP_FPS_NTSC:
        return 30000.0 / 1001;

    case SUP_FPS_PAL_I:
        return 50.0;

    case SUP_FPS_NTSC_I:
        return 60000.0 / 1001;

    default:
        return 0.0;
    }
}


unsigned long sup_pts_to_ms(uint32_t pts) {
    return pts / SUP_PTS_FREQ;
}


int init_sup_packet(struct sup_packet* packet) {
    if (packet == NULL) {
        return -1;
    }

    packet->marker = SUP_PACKET_MARKER;

    packet->pts = 0x00000000;
    packet->dts = 0x00000000;

    packet->segment_type = 0x00;
    packet->segment_len = 0x0000;

    if (packet->segment == NULL) {
        packet->segment = calloc(SUP_PACKET_MAX_SEGMENT_LEN, sizeof(char));
        if (packet->segment == NULL) {
            perror("init_sup_packet(): calloc()");
            return -1;
        }
    }

    return 0;
}


int read_sup_packet(FILE* fd, struct sup_packet* packet) {
    size_t n, received;

    if (init_sup_packet(packet)) {
        return -1;
    }

    /* Check the packet marker. */
    if (fread(&(packet->marker), 2, 1, fd) != 1) {
        if (!feof(fd)) {
            perror("read_sup_packet(): fread(marker)");
        }
        return -1;
    } else {
        packet->marker = ntohs(packet->marker);
        if (packet->marker != SUP_PACKET_MARKER) {
            fprintf(stderr, "Invalid packet marker.\n");
            return -1;
        }
    }

    /* Read the rest of the header. */
    if (fread(&(packet->pts), 4, 1, fd) != 1) {
        perror("read_sup_packet(): fread(PTS)");
        return -1;
    } else {
        packet->pts = ntohl(packet->pts);
    }
    if (fread(&(packet->dts), 4, 1, fd) != 1) {
        perror("read_sup_packet(): fread(DTS)");
        return -1;
    } else {
        packet->dts = ntohl(packet->dts);
    }
    if (fread(&(packet->segment_type), 1, 1, fd) != 1) {
        perror("read_sup_packet(): fread(segment_type)");
        return -1;
    }
    if (fread(&(packet->segment_len), 2, 1, fd) != 1) {
        perror("read_sup_packet(): fread(segment_len)");
        return -1;
    } else {
        packet->segment_len = ntohs(packet->segment_len);
    }

    /* Read the segment */
    n = 0;
    received = 0;
    while (received < packet->segment_len) {
        n = fread(packet->segment + received, 1, packet->segment_len - received, fd);
        if (n <= 0) {
            if (feof(fd)) {
                fprintf(stderr, "Unexpected EOF.\n");
            } else {
                perror("read_sup_packet(): fread(segment)");
            }
            return -1;
        } else {
            received += n;
        }
    }

    return 0;
}


int init_sup_segment_pcs(struct sup_segment_pcs* pcs) {
    if (pcs == NULL) {
        return -1;
    }

    pcs->video_width = 0x0000;
    pcs->video_height = 0x0000;
    pcs->frame_rate = SUP_FPS_UNKNOWN;

    pcs->comp_id = 0x0000;
    pcs->comp_state = SUP_PCS_STATE_UNKNOWN;

    pcs->palette_flag = 0x00;
    pcs->palette_id = 0x00;

    pcs->num_of_objects = 0x00;
    if (pcs->objects == NULL) {
        pcs->objects = calloc(0xff, sizeof(struct sup_object));
        if (pcs->objects == NULL) {
            perror("init_sup_segment_pcs(): calloc()");
            return -1;
        }
    }

    return 0;
}


int parse_sup_segment_pcs(const struct sup_packet* packet, struct sup_segment_pcs* pcs) {
    size_t i, offset = 0;

    if (packet == NULL) {
        return -1;
    } else if (init_sup_segment_pcs(pcs)) {
        return -1;
    }

    if (packet->segment_len < 11) {
        fprintf(stderr, "PCS packet is too short.\n");
        return -1;
    }

    pcs->pts_msec = sup_pts_to_ms(packet->pts);

    memcpy(&(pcs->video_width), packet->segment + offset, 2);
    offset += 2;
    memcpy(&(pcs->video_height), packet->segment + offset, 2);
    offset += 2;
    memcpy(&(pcs->frame_rate), packet->segment + offset, 1);
    offset++;

    memcpy(&(pcs->comp_id), packet->segment + offset, 2);
    offset += 2;
    memcpy(&(pcs->comp_state), packet->segment + offset, 1);
    offset++;

    memcpy(&(pcs->palette_flag), packet->segment + offset, 1);
    offset++;
    memcpy(&(pcs->palette_id), packet->segment + offset, 1);
    offset++;

    pcs->video_width = ntohs(pcs->video_width);
    pcs->video_height = ntohs(pcs->video_height);
    pcs->comp_id = ntohs(pcs->comp_id);

    memcpy(&(pcs->num_of_objects), packet->segment + offset, 1);
    offset++;

    if (packet->segment_len != pcs->num_of_objects * 8 + offset) {
        fprintf(stderr, "Invalid PCS segment length.\n");
        return -1;
    }

    for (i = 0; i < pcs->num_of_objects; i++) {
        memcpy(&(pcs->objects[i].obj_id), packet->segment + offset, 2);
        offset += 2;

        memcpy(&(pcs->objects[i].win_id), packet->segment + offset, 1);
        offset++;
        memcpy(&(pcs->objects[i].obj_flag), packet->segment + offset, 1);
        offset++;

        memcpy(&(pcs->objects[i].obj_pos_x), packet->segment + offset, 2);
        offset += 2;
        memcpy(&(pcs->objects[i].obj_pos_y), packet->segment + offset, 2);
        offset += 2;

        pcs->objects[i].obj_id = ntohs(pcs->objects[i].obj_id);
        pcs->objects[i].obj_pos_x = ntohs(pcs->objects[i].obj_pos_x);
        pcs->objects[i].obj_pos_y = ntohs(pcs->objects[i].obj_pos_y);
    }

    return 0;
}


int init_sup_segment_pds(struct sup_segment_pds* pds) {
    if (pds == NULL) {
        return -1;
    }

    pds->palette_id = 0x0000;
    pds->num_of_colors = 0x00;
    if (pds->colors == NULL) {
        pds->colors = calloc(0xff, sizeof(struct sup_color));
        if (pds->colors == NULL) {
            perror("init_sup_segment_pds(): calloc()");
            return -1;
        }
    }

    return 0;
}


int parse_sup_segment_pds(const struct sup_packet* packet, struct sup_segment_pds* pds) {
    size_t i, offset = 0;

    if (packet == NULL) {
        return -1;
    } else if (init_sup_segment_pds(pds)) {
        return -1;
    }

    if (packet->segment_len < 2) {
        fprintf(stderr, "PDS packet is too short.\n");
        return -1;
    }

    memcpy(&(pds->palette_id), packet->segment + offset, 2);
    offset += 2;

    pds->palette_id = ntohs(pds->palette_id);

    pds->num_of_colors = (packet->segment_len - 2) / 5;
    for (i = 0; i < pds->num_of_colors; i++) {
        memcpy(&(pds->colors[i].idx), packet->segment + offset, 1);
        offset++;
        memcpy(&(pds->colors[i].y), packet->segment + offset, 1);
        offset++;
        memcpy(&(pds->colors[i].cr), packet->segment + offset, 1);
        offset++;
        memcpy(&(pds->colors[i].cb), packet->segment + offset, 1);
        offset++;
        memcpy(&(pds->colors[i].a), packet->segment + offset, 1);
        offset++;

        pds->colors[i].gray =
            (uint8_t) (pds->colors[i].y * pds->colors[i].a / ((double) 0xff));
    }

    return 0;
}


int init_sup_segment_wds(struct sup_segment_wds* wds) {
    if (wds == NULL) {
        return -1;
    }

    wds->num_of_windows = 0x00;
    if (wds->windows == NULL) {
        wds->windows = calloc(0xff, sizeof(struct sup_window));
        if (wds->windows == NULL) {
            perror("init_sup_segment_wds(): calloc()");
            return -1;
        }
    }

    return 0;
}


int parse_sup_segment_wds(const struct sup_packet* packet, struct sup_segment_wds* wds) {
    size_t i, offset = 0;

    if (packet == NULL) {
        return -1;
    } else if (init_sup_segment_wds(wds)) {
        return -1;
    }

    if (packet->segment_len < 1) {
        fprintf(stderr, "WDS packet is too short.\n");
        return -1;
    }

    memcpy(&(wds->num_of_windows), packet->segment + offset, 1);
    offset++;

    if (packet->segment_len != wds->num_of_windows * 9 + offset) {
        fprintf(stderr, "Invalid WDS segment length.\n");
        return -1;
    }

    for (i = 0; i < wds->num_of_windows; i++) {
        memcpy(&(wds->windows[i].win_id), packet->segment + offset, 1);
        offset++;

        memcpy(&(wds->windows[i].x), packet->segment + offset, 2);
        offset += 2;
        memcpy(&(wds->windows[i].y), packet->segment + offset, 2);
        offset += 2;

        memcpy(&(wds->windows[i].width), packet->segment + offset, 2);
        offset += 2;
        memcpy(&(wds->windows[i].height), packet->segment + offset, 2);
        offset += 2;

        wds->windows[i].x = ntohs(wds->windows[i].x);
        wds->windows[i].y = ntohs(wds->windows[i].y);
        wds->windows[i].width = ntohs(wds->windows[i].width);
        wds->windows[i].height = ntohs(wds->windows[i].height);
    }

    return 0;
}


int init_sup_segment_ods(struct sup_segment_ods* ods) {
    if (ods == NULL) {
        return -1;
    }

    ods->obj_id = 0x0000;
    ods->obj_version = 0;
    ods->obj_flag = 0x00;
    ods->obj_data_len = 0x000000;
    ods->obj_width = 0;
    ods->obj_height = 0;

    ods->raw_data = NULL;  /* A pointer inside sup_packet.segment, do not free(). */
    ods->raw_data_len = 0;

    return 0;
}


int parse_sup_segment_ods(const struct sup_packet* packet, struct sup_segment_ods* ods) {
    uint8_t b;
    size_t offset = 0;

    if (packet == NULL) {
        return -1;
    } else if (init_sup_segment_ods(ods)) {
        return -1;
    }

    if (packet->segment_len < 4) {
        fprintf(stderr, "ODS packet is too short.\n");
        return -1;
    }

    memcpy(&(ods->obj_id), packet->segment + offset, 2);
    offset += 2;
    memcpy(&(ods->obj_version), packet->segment + offset, 1);
    offset++;
    memcpy(&(ods->obj_flag), packet->segment + offset, 1);
    offset++;

    ods->obj_id = ntohs(ods->obj_id);

    if (ods->obj_flag & SUP_ODS_FIRST) {
        if (packet->segment_len < 11) {
            fprintf(stderr, "First ODS packet is too short.\n");
            return -1;
        }

        memcpy(&b, packet->segment + offset, 1);
        offset++;
        ods->obj_data_len = b << 16;
        memcpy(&b, packet->segment + offset, 1);
        offset++;
        ods->obj_data_len += b << 8;
        memcpy(&b, packet->segment + offset, 1);
        offset++;
        ods->obj_data_len += b;

        memcpy(&(ods->obj_width), packet->segment + offset, 2);
        ods->obj_width = ntohs(ods->obj_width);
        offset += 2;

        memcpy(&(ods->obj_height), packet->segment + offset, 2);
        ods->obj_height = ntohs(ods->obj_height);
        offset += 2;

    } else {
        if (packet->segment_len < 5) {
            fprintf(stderr, "ODS packet is too short.\n");
            return -1;
        }
    }

    ods->raw_data = packet->segment + offset;
    ods->raw_data_len = packet->segment_len - offset;

    return 0;
}
