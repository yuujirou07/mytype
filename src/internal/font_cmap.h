#ifndef FONT_CMAP_H
#define FONT_CMAP_H

#include <stddef.h>
#include <stdint.h>

#define CMAP_ENCODING_TYPE_COUNT 4
#define format4_uint16_mem 6

struct cmap_encoding_record;

struct segment_data{
        int segment_num;
        uint16_t start;
        uint16_t end;
};

struct cmap_format4_segment{
        uint16_t chr;
        struct segment_data segment;
};

struct cmap_encoding_record_id_table{
        struct {
                uint16_t pid;
                uint16_t eid;
                struct cmap_encoding_record *ptr;
        } record_table;
        int table_size;
};

struct can_use_encoding_type{
        uint16_t pid;
        uint16_t eid;
};

enum os_type{
        Windows,
        Linux,
        Mac,
        Other,
};

enum support_format_type{
        format4 = 4,
        format12 = 12,
        none = 0,
};

enum encode_type{
        unicode_encode,
        windows_encode,
};

enum cmap_platform_id {
        CMAP_PLATFORM_UNICODE = 0,
        CMAP_PLATFORM_MACINTOSH = 1,
        CMAP_PLATFORM_ISO = 2,
        CMAP_PLATFORM_WINDOWS = 3,
        CMAP_PLATFORM_CUSTOM = 4,
};

enum cmap_unicode__platform_id {
        CMAP_UNICODE_1_0_DEPRECATED = 0,
        CMAP_UNICODE_1_1_DEPRECATED = 1,
        CMAP_UNICODE_ISO_10646_DEPRECATED = 2,
        CMAP_UNICODE_BMP = 3,
        CMAP_UNICODE_FULL_REPERTOIRE = 4,
        CMAP_UNICODE_VARIATION_SEQUENCES = 5,
        CMAP_UNICODE_LAST_RESORT = 6,
};

enum cmap_windows_encoding_id {
        CMAP_WINDOWS_SYMBOL = 0,
        CMAP_WINDOWS_UNICODE_BMP = 1,
        CMAP_WINDOWS_SHIFT_JIS = 2,
        CMAP_WINDOWS_PRC = 3,
        CMAP_WINDOWS_BIG5 = 4,
        CMAP_WINDOWS_WANSUNG = 5,
        CMAP_WINDOWS_JOHAB = 6,
        CMAP_WINDOWS_RESERVED_7 = 7,
        CMAP_WINDOWS_RESERVED_8 = 8,
        CMAP_WINDOWS_RESERVED_9 = 9,
        CMAP_WINDOWS_UNICODE_FULL = 10,
};

struct format4_data{
        uint16_t length;
        uint16_t language;
        uint16_t seg_count;
        uint16_t search_range;
        uint16_t entry_selector;
        uint16_t range_shift;
        uint16_t *end_code;
        uint16_t reserved_pad;
        uint16_t *start_code;
        int16_t *id_delta;
        uint16_t *id_range_offset;
        uint16_t *glyph_id_array;
        size_t glyph_id_count;
};

struct cmap_sub_table_data{
        int format_type;
};

struct cmap_encoding_record{
        uint16_t platform_id;
        uint16_t encoding_id;
        uint32_t offset;
};

struct using_chr_encode_type{
        enum cmap_platform_id platform_id;
        enum encode_type encode_type;
        uint16_t encode_num;
};

int select_encoding(uint16_t platform_id,uint16_t encoding_id);
enum os_type check_os(void);
void check_available_record_type(
        struct can_use_encoding_type types[CMAP_ENCODING_TYPE_COUNT],
        enum os_type this_os);
void set_cmap_encoding_record(
        int plat_form_id,
        int encoding_id,
        struct cmap_encoding_record_id_table *id_table,
        struct can_use_encoding_type *can_use_encoding_record,
        struct cmap_encoding_record **encoding_record);
int get_format_type(int ttf_fd,int *format_type);
int format4_data_parse(int ttf_fd,struct format4_data *f4_data);
int input_f4_data(
        struct format4_data *f4_data,
        uint16_t data,
        int f4_mem_counter);
struct cmap_format4_segment *found_input_chr_range(
        const struct format4_data *f4_data,
        uint16_t input_data);
int get_glyph_id(
        const struct format4_data *f4_data,
        const struct cmap_format4_segment *f4_seg_data,
        uint16_t *glyph_id);

#endif
