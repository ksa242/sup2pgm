/**
 * SUP2PGM
 * Converts BluRay presentation graphics streams (SUP subtitles) to PGM
 * images one can later transcode or OCR into text.
 *
 * Copyright (c) 2013, Sergey Kolchin <ksa242@gmail.com>
 * All rights reserved.
 * Released under 3-clause BSD License.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "sup2pgm.h"
#include "srt.h"
#include "pgm.h"
#include "sup.h"


void print_usage_help(const char* bin) {
    printf("%s v%s\n", SUP2PGM_PROGRAM_NAME, SUP2PGM_VERSION);
    printf("Converts BluRay presentation graphics streams (SUP subtitles) to PGM images one can later transcode or OCR into text.\n\n");

    printf("%s [options]\n\n", bin);

    printf("%s takes BD-SUP subtitle stream and dumps the captions as PGM images complete with SRT timecodes.\n\n", SUP2PGM_PROGRAM_NAME);

    printf("Options:\n");
    printf("  -i <file_name>  Use file_name for input (default: stdin).\n");
    printf("  -o <base_name>  Use base_name for output files (default: movie_subtitle).\n");
    printf("  -v              Be verbose: dump parsed packets.\n");
}


void dump_segment_pcs(const struct sup_segment_pcs* pcs) {
    size_t i;

    DEBUG("PTS %u\n", pcs->pts_msec);

    if (pcs->comp_state == SUP_PCS_STATE_EPOCH_START) {
        DEBUG("PCS START");
    } else if (pcs->comp_state == SUP_PCS_STATE_NORMAL) {
        DEBUG("PCS NORMAL");
    } else if (pcs->comp_state == SUP_PCS_STATE_EPOCH_CONTINUE) {
        DEBUG("PCS CONT");
    } else if (pcs->comp_state == SUP_PCS_STATE_ACQU_POINT) {
        DEBUG("PCS ACQU");
    } else {
        DEBUG("PCS UNKNOWN\n");
        return;
    }

    DEBUG(" %ux%u @ %.3f fps:\n",
          pcs->video_width, pcs->video_height,
          sup_frame_rate_by_id(pcs->frame_rate));

    DEBUG("  Composition 0x%04x\n", pcs->comp_id);

    DEBUG("  Palette 0x%02x", pcs->palette_id);
    if (pcs->palette_flag == SUP_PCS_PALETTE_UPDATED) {
        DEBUG(", updated");
    }
    DEBUG("\n");

    if (pcs->num_of_objects > 0) {
        DEBUG("  Objects:\n");
        for (i = 0; i < pcs->num_of_objects; i++) {
            DEBUG("    0x%04x: window 0x%02x, offset %ux%u",
                  pcs->objects[i].obj_id,
                  pcs->objects[i].win_id,
                  pcs->objects[i].obj_pos_x,
                  pcs->objects[i].obj_pos_y);
            if (pcs->objects[i].obj_flag & SUP_PCS_OBJ_CROPPED) {
                DEBUG(", cropped");
            }
            if (pcs->objects[i].obj_flag & SUP_PCS_OBJ_FORCED) {
                DEBUG(", forced");
            }
            DEBUG("\n");
        }
    }
}


void dump_segment_pds(const struct sup_segment_pds* pds) {
    size_t i;
    DEBUG("PDS 0x%04x, %u color(s) in YCbCrA (grayscale):\n",
          pds->palette_id, pds->num_of_colors);
    for (i = 0; i < pds->num_of_colors; i++) {
        DEBUG("  0x%02x: #%02x%02x%02x%02x (0x%02x)\n",
              pds->colors[i].idx,
              pds->colors[i].y,
              pds->colors[i].cb,
              pds->colors[i].cr,
              pds->colors[i].a,
              pds->colors[i].gray);
    }
}


void dump_segment_wds(const struct sup_segment_wds* wds) {
    size_t i;
    DEBUG("WDS\n");
    if (wds->num_of_windows > 0) {
        for (i = 0; i < wds->num_of_windows; i++) {
            DEBUG("  0x%02x: %ux%u+%u+%u\n",
                  wds->windows[i].win_id,
                  wds->windows[i].width, wds->windows[i].height,
                  wds->windows[i].x, wds->windows[i].y);
        }
    }
}


void dump_segment_ods(const struct sup_segment_ods* ods) {
    DEBUG("ODS 0x%02x", ods->obj_id);
    if (ods->obj_flag & SUP_ODS_FIRST) {
        DEBUG(", %ux%u, %u (0x%06x) bytes compressed",
              ods->obj_width, ods->obj_height,
              ods->obj_data_len - 4,
              ods->obj_data_len - 4);

    } else if (ods->obj_flag & SUP_ODS_LAST) {
        DEBUG(", last part");
    }
    DEBUG("\n");
}


void dump_segment_end(const struct sup_packet* packet) {
    DEBUG("END\n\n");
}


int render_sup_image(unsigned char* dest, size_t dest_len,
                     const unsigned char* src, size_t src_len, uint16_t obj_id,
                     const struct sup_segment_pcs* pcs,
                     const struct sup_segment_wds* wds,
                     const struct sup_segment_pds* pds,
                     const struct sup_segment_ods* ods) {

    size_t video_width = pcs->video_width,
           video_height = pcs->video_height,
           obj_pos_x = 0,
           obj_pos_y = 0,
           window_pos_x = 0,
           window_pos_y = 0,
           window_width = 0,
           window_height = 0;

    size_t src_idx, dest_idx, i, j, n;

    unsigned char b;

    for (i = 0; i < pcs->num_of_objects; i++) {
        if (pcs->objects[i].obj_id == obj_id) {
            obj_pos_x = pcs->objects[i].obj_pos_x;
            obj_pos_y = pcs->objects[i].obj_pos_y;
            for (j = 0; j < wds->num_of_windows; j++) {
                if (wds->windows[j].win_id == pcs->objects[i].win_id) {
                    window_pos_x = wds->windows[j].x;
                    window_pos_y = wds->windows[j].y;
                    window_width = wds->windows[j].width;
                    window_height = wds->windows[j].height;
                    break;
                }
            }
        }
    }

    if (window_width == 0 ||
        window_height == 0 ||
        obj_pos_x < window_pos_x ||
        obj_pos_y < window_pos_y) {

        ERROR("SUP object or window not found.\n");
        return -1;
    }

    pgm_clear_region(dest, video_width, video_height,
                     window_width, window_height,
                     window_pos_x, window_pos_y);

    src_idx = 0;
    dest_idx = obj_pos_y * video_width + obj_pos_x;

    while (src_idx < src_len && dest_idx < dest_len) {
        b = src[src_idx++];
        if (b == 0x00) {
            b = src[src_idx++];
            if (b == 0x00) {
                /* 00 00 eq. new line. */
                obj_pos_y++;
                dest_idx = obj_pos_y * video_width + obj_pos_x;

            } else {
                if ((b & 0xc0) == 0x40) {
                    /* 00 4x xx -> xxx zeroes. */
                    n = ((b - 0x40) << 8) + src[src_idx++];
                    for (i = 0; i < n; i++) {
                        dest[dest_idx++] = 0x00;
                    }

                } else if ((b & 0xC0) == 0x80) {
                    /* 00 8x yy -> x times value y. */
                    n = (b - 0x80);
                    b = pds->colors[src[src_idx++]].gray;
                    for (i = 0; i < n; i++) {
                        dest[dest_idx++] = b;
                    }

                } else if ((b & 0xc0) != 0) {
                    /* 00 cx yy zz -> xyy times value z. */
                    n = ((b - 0xc0) << 8) + src[src_idx++];
                    b = pds->colors[src[src_idx++]].gray;
                    for (i = 0; i < n; i++) {
                        dest[dest_idx++] = b;
                    }
                } else {
                    /* 00 xx -> xx times 0. */
                    for (i = 0; i < b; i++) {
                        dest[dest_idx++] = 0x00;
                    }
                }
            }
        } else {
            b = pds->colors[b].gray;
            dest[dest_idx++] = b;
        }
    }

    return 0;
}


int save_sup_image(FILE* srt_file,
                   size_t subtitle_num,
                   uint32_t start_time, uint32_t end_time, char* timecode_buf,
                   const char* img_base_filename, char* img_filename_buf,
                   const unsigned char* img, size_t img_width, size_t img_height) {
    int result = -1;
    FILE* img_file;

    if (img == NULL) {
        return result;
    }

    sprintf(img_filename_buf, "%s%05lu.pgm", img_base_filename, subtitle_num);
    if ((img_file = fopen(img_filename_buf, "wb")) == NULL) {
        perror("main(): fopen(PGM)");
        return result;
    }

    if (!pgm_write(img_file, img, img_width, img_height)) {
        result = 0;

        DEBUG("Saving image %lu.\n\n", subtitle_num);

        fprintf(srt_file, "%lu\n", subtitle_num + 1);

        srt_render_time(start_time, timecode_buf);
        fprintf(srt_file, "%s --> ", timecode_buf);
        srt_render_time(end_time, timecode_buf);
        fprintf(srt_file, "%s\n", timecode_buf);

        fprintf(srt_file, "%s\n", img_filename_buf);
        fprintf(srt_file, "\n");
    }

    fclose(img_file);
    return result;
}


int main(int argc, char* argv[]) {
    size_t i = 0, j = 0;

    uint8_t verbose = 0;

    FILE* sup_file = stdin;
    char* sup_filename = NULL;

    FILE* srt_file = NULL;
    char* srt_filename = NULL;
    uint32_t srt_start_time = 0,
             srt_end_time = 0;
    char* srt_timecode = NULL;

    size_t pgm_file_num = 0;
    char* pgm_base_filename = "movie_subtitle";
    char* pgm_filename = NULL;

    unsigned char* canvas = NULL;
    size_t canvas_len = 0,
           canvas_width = 0,
           canvas_height = 0;

    size_t subimgs_cnt = 0;
    struct subimage** subimgs;
    struct subimage* subimg = NULL;

    size_t packet_num = 0;
    struct sup_packet* packet = NULL;
    struct sup_segment_pcs* pcs = NULL;
    struct sup_segment_pds* pds = NULL;
    struct sup_segment_wds* wds = NULL;
    struct sup_segment_ods* ods = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
            print_usage_help(argv[0]);
            return EXIT_SUCCESS;
        } else if (!strcmp(argv[i], "-v")) {
            verbose = 1;
        } else if (!strcmp(argv[i], "-i")) {
            i++;
            if (i == argc || strlen(argv[i]) == 0) {
                ERROR("Please specify an input file.\n");
                return EXIT_FAILURE;
            } else {
                sup_filename = argv[i];
            }
        } else if (!strcmp(argv[i], "-o")) {
            i++;
            if (i == argc || strlen(argv[i]) == 0) {
                ERROR("Please specify the base name for PGM images.\n");
                return EXIT_FAILURE;
            } else {
                pgm_base_filename = argv[i];
            }
        }
    }

    if (sup_filename != NULL) {
        if ((sup_file = fopen(sup_filename, "rb")) == NULL) {
            ERROR("Failed opening SUP file %s.\n", sup_filename);
            return EXIT_FAILURE;
        }
    }

    pgm_filename = calloc(strlen(pgm_base_filename) + 10, sizeof(char));
    if (pgm_filename == NULL) {
        perror("main(): calloc(PGM_FILENAME)");
        fclose(sup_file);
        return EXIT_FAILURE;
    }

    srt_filename = calloc(strlen(pgm_base_filename) + 6, sizeof(char));
    if (srt_filename == NULL) {
        perror("main(): calloc(SRT_FILENAME)");
        fclose(sup_file);
        return EXIT_FAILURE;
    } else {
        sprintf(srt_filename, "%s.srtx", pgm_base_filename);
    }
    if ((srt_file = fopen(srt_filename, "w")) == NULL) {
        ERROR("Failed opening SRT file %s.\n", srt_filename);
        free(srt_filename);
        fclose(sup_file);
        return EXIT_FAILURE;
    }
    srt_timecode = calloc(SRT_TIMECODE_LEN + 1, sizeof(char));
    if (srt_timecode == NULL) {
        perror("main(): calloc(SRT_TIMESTAMP)");
        free(srt_filename);
        fclose(srt_file);
        fclose(sup_file);
        return EXIT_FAILURE;
    }

    subimgs = NULL;

    packet = calloc(1, sizeof(struct sup_packet));
    pcs = calloc(1, sizeof(struct sup_segment_pcs));
    pds = calloc(1, sizeof(struct sup_segment_pds));
    wds = calloc(1, sizeof(struct sup_segment_wds));
    ods = calloc(1, sizeof(struct sup_segment_ods));
    if (sup_init_packet(packet) ||
        sup_init_segment_pcs(pcs) ||
        sup_init_segment_pds(pds) ||
        sup_init_segment_wds(wds) ||
        sup_init_segment_ods(ods)) {

        ERROR("SUP placeholders' initialization failed.\n");

        free(ods);
        if (wds != NULL) {
            free(wds->windows);
            free(wds);
        }
        if (pds != NULL) {
            free(pds->colors);
            free(pds);
        }
        if (pcs != NULL) {
            free(pcs->objects);
            free(pcs);
        }
        if (packet != NULL) {
            free(packet->segment);
            free(packet);
        }

        free(srt_timecode);
        free(srt_filename);

        fclose(srt_file);
        fclose(sup_file);

        return EXIT_FAILURE;
    }

    for (; !feof(sup_file); packet_num++) {
        if (sup_read_packet(sup_file, packet)) {
            continue;
        }

        if (packet->segment_type == SUP_SEGMENT_PCS) {
            /* Set up composition. */
            if (sup_parse_segment_pcs(packet, pcs)) {
                ERROR("Bad PCS %lu.\n", packet_num);
                continue;
            } else if (verbose) {
                dump_segment_pcs(pcs);
            }

            if (pcs->comp_state == SUP_PCS_STATE_EPOCH_START) {
                /**
                 * Start a new composition: clear the image buffer,
                 * reset the timecodes.
                 */
                if (canvas_width != pcs->video_width || canvas_height != pcs->video_height) {
                    if (canvas != NULL) {
                        free(canvas);
                    }

                    canvas_width = pcs->video_width;
                    canvas_height = pcs->video_height;
                    canvas_len = canvas_width * canvas_height;
                    if ((canvas = malloc(canvas_len)) == NULL) {
                        perror("main(): malloc(CANVAS)");
                        break;
                    }
                }

                srt_start_time = pcs->pts_msec;
                srt_end_time = 0;

            } else if (pcs->pts_msec >= srt_start_time + SUP2PGM_MERGE_THRESHOLD) {
                /* Save the previously rendered composition. */
                srt_end_time = pcs->pts_msec;

                if (!save_sup_image(srt_file,
                                    pgm_file_num,
                                    srt_start_time, srt_end_time, srt_timecode,
                                    pgm_base_filename, pgm_filename,
                                    canvas, canvas_width, canvas_height)) {
                    pgm_file_num++;
                }

                srt_start_time = pcs->pts_msec;
                srt_end_time = 0;
            }

            pgm_clear(canvas, canvas_width, canvas_height);

        } else if (packet->segment_type == SUP_SEGMENT_PDS) {
            /* Extract palette. */
            if (sup_parse_segment_pds(packet, pds)) {
                ERROR("Bad PDS %lu.\n", packet_num);
                continue;
            } else if (verbose) {
                dump_segment_pds(pds);
            }

        } else if (packet->segment_type == SUP_SEGMENT_WDS) {
            /* Extract windows info. */
            if (sup_parse_segment_wds(packet, wds)) {
                ERROR("Bad WDS %lu.\n", packet_num);
                continue;
            } else if (verbose) {
                dump_segment_wds(wds);
            }

        } else if (packet->segment_type == SUP_SEGMENT_ODS) {
            /* Decode and render caption image. */
            if (sup_parse_segment_ods(packet, ods)) {
                ERROR("Bad ODS %lu.\n", packet_num);
                continue;
            } else if (verbose) {
                dump_segment_ods(ods);
            }

            if (subimgs_cnt < ods->obj_id + 1) {
                subimgs = realloc(subimgs, (ods->obj_id + 1) * sizeof(struct subimage*));
                if (subimgs == NULL) {
                    perror("main(): realloc(SUBIMGS)");
                    break;
                } else {
                    for (i = subimgs_cnt; i <= ods->obj_id; i++) {
                        subimgs[i] = malloc(sizeof(struct subimage));
                        if (subimgs[i] == NULL) {
                            perror("main(): malloc(SUBIMG)");
                        } else {
                            subimgs[i]->max_len = 0;
                            subimgs[i]->len = 0;
                            subimgs[i]->img = NULL;
                        }
                    }
                    subimgs_cnt = ods->obj_id + 1;
                }
            }

            subimg = subimgs[ods->obj_id];
            if (subimg == NULL) {
                break;
            }

            if (subimg->img == NULL) {
                subimg->max_len = SUP_PACKET_MAX_SEGMENT_LEN;
                if ((subimg->img = malloc(subimg->max_len)) == NULL) {
                    perror("main(): malloc(SUBIMG)");
                    break;
                }
            }

            if (ods->obj_flag & SUP_ODS_FIRST) {
                subimg->len = 0;
            }

            if (subimg->len + ods->raw_data_len > subimg->max_len) {
                subimg->max_len = subimg->len + ods->raw_data_len;
                if ((subimg->img = realloc(subimg->img, subimg->max_len)) == NULL) {
                    perror("main(): realloc(SUBIMG)");
                    break;
                }
            }

            memcpy(subimg->img + subimg->len, ods->raw_data, ods->raw_data_len);
            subimg->len += ods->raw_data_len;

        } else if (packet->segment_type == SUP_SEGMENT_END) {
            /* Render composition. */

            if (verbose) {
                dump_segment_end(packet);
            }

            if (pcs->num_of_objects > 0) {
                for (i = 0; i < pcs->num_of_objects; i++) {
                    if (pcs->objects[i].obj_id < subimgs_cnt) {
                        subimg = subimgs[pcs->objects[i].obj_id];
                        render_sup_image(canvas, canvas_len,
                                         subimg->img, subimg->len, pcs->objects[i].obj_id,
                                         pcs, wds, pds, ods);
                    }
                }
            }

            /* Reset composition placeholders. */
            sup_init_segment_pcs(pcs);
            sup_init_segment_pds(pds);
            sup_init_segment_wds(wds);
            sup_init_segment_ods(ods);
        } else {
            ERROR("Unknown segment type 0x%02x for packet %lu.\n",
                  packet->segment_type, packet_num);
        }
    }

    DEBUG("%lu packets parsed, %lu images saved.\n", packet_num, pgm_file_num);

    free(ods);
    free(wds->windows);
    free(wds);
    free(pds->colors);
    free(pds);
    free(pcs->objects);
    free(pcs);
    free(packet->segment);
    free(packet);

    for (i = 0; i < subimgs_cnt; i++) {
        free(subimgs[i]->img);
        free(subimgs[i]);
    }
    free(subimgs);

    free(canvas);

    free(pgm_filename);

    free(srt_timecode);
    free(srt_filename);
    fclose(srt_file);

    fclose(sup_file);

    return EXIT_SUCCESS;
}
