#ifndef FONT_LOCA_H
#define FONT_LOCA_H

#include <stddef.h>
#include <stdint.h>

#include "font_table.h"

struct loca_table_2byte_format{
        uint16_t *offsets;
        size_t offset_count;
};

struct loca_table_4byte_format{
        uint32_t *offsets;
        size_t offset_count;
};

enum loca_table_format_type{
        byte_4,
        byte_2,
};

struct loca_table_data_wrapper{
        enum loca_table_format_type loca_table_format_type;
        struct loca_table_2byte_format *loca_table_2byte_format;
        struct loca_table_4byte_format *loca_table_4byte_format;
};

int parse_2byte_loca_table(
        int ttf_fd,
        struct table_record table_record,
        struct loca_table_2byte_format *loca_table_2byte_format,
        uint16_t numGlyph);
int parse_4byte_loca_table(
        int ttf_fd,
        struct table_record table_record,
        struct loca_table_4byte_format *loca_table_4byte_format,
        uint16_t numGlyph);
uint16_t get_2b_loca_arry_num_data(
        struct loca_table_2byte_format loca_table_2b,
        int num);
uint32_t get_4b_loca_arry_num_data(
        struct loca_table_4byte_format loca_table_4b,
        int num);
int get_glyph_loca_pos(
        const struct loca_table_data_wrapper *loca_table_data_wrapp,
        uint16_t glyph_id,
        uint32_t *glyph_start_pos,
        uint32_t *glyph_end_pos);
int loca_table_eazy_parse(
        int ttf_fd,
        int indexToLocFormat,
        struct table_record loca_table,
        uint16_t numGlyphs,
        struct loca_table_data_wrapper *loca_table_data_wrapper);

#endif
