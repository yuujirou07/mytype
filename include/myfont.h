#ifndef MYFONT_H
#define MYFONT_H

#include <stddef.h>
#include <stdint.h>

#define MYFONT_ON_CURVE_POINT 0x01
#define MYFONT_X_SHORT_VECTOR 0x02
#define MYFONT_Y_SHORT_VECTOR 0x04
#define MYFONT_REPEAT_FLAG 0x08
#define MYFONT_X_IS_SAME_OR_POSITIVE 0x10
#define MYFONT_Y_IS_SAME_OR_POSITIVE 0x20

/* 1つの単純グリフを解析した結果。 */
struct myfont_glyph_data{
        int16_t number_of_contours;
        int16_t x_min;
        int16_t y_min;
        int16_t x_max;
        int16_t y_max;

        uint16_t *end_pts_of_contours;
        size_t end_pts_of_contours_count;

        uint16_t instruction_length;
        uint8_t *instructions;

        size_t point_count;
        uint8_t *flags;
        size_t flags_count;

        int16_t *x_coordinates;
        int16_t *y_coordinates;
};

void myfont_glyph_data_init(struct myfont_glyph_data *glyph_data);
void myfont_glyph_data_free(struct myfont_glyph_data *glyph_data);

/* parse前にinit、使い終わった後にfreeを呼ぶ。 */

int myfont_parse_glyph_header(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_end_points(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_instructions(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_flags(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_x_coordinates(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_y_coordinates(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data);

int myfont_parse_simple_glyph(
        int ttf_fd,
        uint32_t glyph_file_pos,
        uint32_t glyph_data_length,
        struct myfont_glyph_data *glyph_data);

int myfont_get_contour_range(
        const struct myfont_glyph_data *glyph_data,
        size_t contour_num,
        size_t *start_point,
        size_t *end_point);

#endif
