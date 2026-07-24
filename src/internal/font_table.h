#ifndef FONT_TABLE_H
#define FONT_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#define table_dir_int_mem 5
#define table_int_mem_max_size 4
#define tag_name_size 4

#define TABLE_SET 0
#define TABLE_GET 1
#define TABLE_CREATE 3
#define TABLE_FREE 4

struct offset_tag_dic{
        uint8_t tag[tag_name_size];
        long pos;
        int table_num;
};

enum table_record_type{
        tabletag,
        checksum,
        offset,
        length,
};

struct table_record{
        uint8_t tag[tag_name_size];
        uint32_t checksum;
        uint32_t offcet;
        uint32_t length;
};

struct ttf_table_dir_data{
        uint32_t sfntVersion;
        uint16_t numTables;
        uint16_t searchRange;
        uint16_t entrySelector;
        uint16_t rangeShift;
        struct table_record *tableRecords;
};

struct maxp_table_data{
        uint32_t version;
        uint16_t num_glyphs;
        uint16_t max_points;
        uint16_t max_contours;
        uint16_t max_composite_points;
        uint16_t max_composite_contours;
        uint16_t max_zones;
        uint16_t max_twilight_points;
        uint16_t max_storage;
        uint16_t max_function_defs;
        uint16_t max_instruction_defs;
        uint16_t max_stack_elements;
        uint16_t max_size_of_instructions;
        uint16_t max_component_elements;
        uint16_t max_component_depth;
};

struct head_table_data{
        uint16_t major_version;
        uint16_t minor_version;
        int32_t font_revision;
        uint32_t checksum_adjustment;
        uint32_t magic_number;
        uint16_t flags;
        uint16_t units_per_em;
        int64_t created;
        int64_t modified;
        int16_t x_min;
        int16_t y_min;
        int16_t x_max;
        int16_t y_max;
        uint16_t mac_style;
        uint16_t lowest_rec_ppem;
        int16_t font_direction_hint;
        int16_t index_to_loc_format;
        int16_t glyph_data_format;
};

extern struct ttf_table_dir_data table_dir_data;

int load_ttf_table_dir_data(int ttf_fd);
void parse_ttf_data(int ttf_fd);
int assignment_table_dir_mum(int counter,uint8_t *data);
long find_offset(char *tag);
long offset_table_ctl(long *offset,uint8_t *tag,int *table_num,int flags);
struct table_record *found_tag_table(
        struct ttf_table_dir_data *dir_data,
        uint8_t tagname[tag_name_size]);
uint16_t get_maxp_table_numGlyphs_data(long offset,int ttf_fd);
bool numGlyph_check(uint16_t numGlyphs,uint16_t Glyph_id);
int16_t get_head_table_indexToLocFormat(long offset,int ttf_fd);
uint16_t get_head_table_units_per_em(long offset,int ttf_fd);

#endif
