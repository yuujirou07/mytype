#ifndef FONT_GLYPH_H
#define FONT_GLYPH_H

#include <stddef.h>
#include <stdint.h>

struct point_pos{
        int16_t pos_x;
        int16_t pos_y;
        uint8_t flags;
};

struct contour_pos_data{
        struct point_pos *point_pos_data;
        size_t point_pos_data_arry_size;
};

struct contour_data{
        struct contour_pos_data *pos_data;
        uint16_t pos_data_arry_size;
};

struct point_data{
        struct {
                int16_t x_cordinates;
                uint8_t flags;
        } x_pos_data;
        struct {
                int16_t y_cordinates;
                uint8_t flags;
        } y_pos_data;
};

struct glyf_table{
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
        struct point_data *points;
};

struct chr_glyf_dic_table{
        struct glyf_table *glif_table;
        struct contour_data *contour_data;
        uint16_t chr;
};

int parse_glyph_data_table(
        long ttf_fd,
        uint32_t glyf_file_table_offset,
        struct glyf_table *glyf_table);
int get_countour_data(
        struct glyf_table *glif_table,
        struct contour_data *contour_data);
void free_countour_data(struct contour_data *contour_data);

#endif
