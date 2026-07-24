#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "myfont.h"

#include "font_binary.h"
#include "font_cmap.h"
#include "font_func_flags.h"
#include "font_glyph.h"
#include "font_loca.h"
#include "font_render.h"
#include "font_table.h"

struct myfont_font{
        int file_descriptor;
        struct ttf_table_dir_data table_directory;
        struct format4_data cmap_format4;
        struct loca_table_data_wrapper loca;
        struct table_record glyf_record;
        uint16_t glyph_count;
        uint16_t units_per_em;
};

struct myfont_glyph{
        uint16_t glyph_id;
        struct glyf_table glyf;
        struct contour_data contours;
};

static void free_format4(struct format4_data *format4){
        if(format4 == NULL)return;

        free(format4->end_code);
        free(format4->start_code);
        free(format4->id_delta);
        free(format4->id_range_offset);
        free(format4->glyph_id_array);
        memset(format4,0,sizeof(*format4));
}

static void free_loca(struct loca_table_data_wrapper *loca){
        if(loca == NULL)return;

        if(loca->loca_table_2byte_format != NULL){
                free(loca->loca_table_2byte_format->offsets);
                free(loca->loca_table_2byte_format);
        }
        if(loca->loca_table_4byte_format != NULL){
                free(loca->loca_table_4byte_format->offsets);
                free(loca->loca_table_4byte_format);
        }

        memset(loca,0,sizeof(*loca));
}

static void clear_table_loader_state(void){
        free(table_dir_data.tableRecords);
        memset(&table_dir_data,0,sizeof(table_dir_data));
        (void)offset_table_ctl(NULL,NULL,NULL,TABLE_FREE);
}

static int cmap_record_priority(
        const struct cmap_encoding_record *record,
        uint16_t format){

        if(record == NULL || format != format4)return -1;
        if(record->platform_id == CMAP_PLATFORM_WINDOWS &&
                record->encoding_id == CMAP_WINDOWS_UNICODE_BMP)return 100;
        if(record->platform_id == CMAP_PLATFORM_UNICODE &&
                record->encoding_id == CMAP_UNICODE_BMP)return 90;
        if(record->platform_id == CMAP_PLATFORM_UNICODE)return 80;
        return 10;
}

static myfont_result load_cmap_format4(
        myfont_font *font,
        const struct table_record *cmap_record){

        if(font == NULL || cmap_record == NULL)return MYFONT_ERROR_INVALID_ARGUMENT;
        if(cmap_record->length < 4)return MYFONT_ERROR_INVALID_FONT;
        if(lseek(font->file_descriptor,cmap_record->offcet,SEEK_SET) < 0){
                return MYFONT_ERROR_IO;
        }

        uint8_t cmap_header[4];
        if(read_exact(font->file_descriptor,cmap_header,sizeof(cmap_header)) != 0){
                return MYFONT_ERROR_IO;
        }

        uint16_t version = read_be16(cmap_header);
        uint16_t record_count = read_be16(cmap_header + 2);
        if(version != 0 || record_count == 0)return MYFONT_ERROR_INVALID_FONT;

        struct cmap_encoding_record *records =
                calloc(record_count,sizeof(*records));
        if(records == NULL)return MYFONT_ERROR_ALLOCATION;

        myfont_result result = MYFONT_ERROR_UNSUPPORTED;
        uint8_t record_data[8];

        for(uint16_t i = 0;i < record_count;i++){
                if(read_exact(
                        font->file_descriptor,
                        record_data,
                        sizeof(record_data)) != 0){
                        result = MYFONT_ERROR_IO;
                        goto cleanup;
                }
                records[i].platform_id = read_be16(record_data);
                records[i].encoding_id = read_be16(record_data + 2);
                records[i].offset = read_be32(record_data + 4);
        }

        int selected_record = -1;
        int selected_priority = -1;

        for(uint16_t i = 0;i < record_count;i++){
                if(records[i].offset > cmap_record->length - 2u)continue;
                if(records[i].offset > UINT32_MAX - cmap_record->offcet)continue;

                uint32_t subtable_position = cmap_record->offcet + records[i].offset;
                if(lseek(font->file_descriptor,subtable_position,SEEK_SET) < 0){
                        result = MYFONT_ERROR_IO;
                        goto cleanup;
                }

                uint8_t format_data[2];
                if(read_exact(
                        font->file_descriptor,
                        format_data,
                        sizeof(format_data)) != 0){
                        result = MYFONT_ERROR_IO;
                        goto cleanup;
                }

                int priority = cmap_record_priority(
                        &records[i],
                        read_be16(format_data));
                if(priority > selected_priority){
                        selected_record = i;
                        selected_priority = priority;
                }
        }

        if(selected_record < 0)goto cleanup;

        uint32_t selected_position =
                cmap_record->offcet + records[selected_record].offset;
        if(lseek(font->file_descriptor,selected_position,SEEK_SET) < 0){
                result = MYFONT_ERROR_IO;
                goto cleanup;
        }

        int selected_format = 0;
        if(get_format_type(font->file_descriptor,&selected_format) != 0 ||
                selected_format != format4){
                result = MYFONT_ERROR_UNSUPPORTED;
                goto cleanup;
        }
        if(format4_data_parse(
                font->file_descriptor,
                &font->cmap_format4) != 0){
                result = MYFONT_ERROR_INVALID_FONT;
                goto cleanup;
        }

        result = MYFONT_SUCCESS;

cleanup:
        free(records);
        return result;
}

myfont_result myfont_open(const char *ttf_path,myfont_font **out_font){
        if(ttf_path == NULL || out_font == NULL){
                return MYFONT_ERROR_INVALID_ARGUMENT;
        }
        *out_font = NULL;

        myfont_font *font = calloc(1,sizeof(*font));
        if(font == NULL)return MYFONT_ERROR_ALLOCATION;
        font->file_descriptor = -1;

        font->file_descriptor = open(ttf_path,O_RDONLY | O_BINARY);
        if(font->file_descriptor < 0){
                myfont_close(font);
                return MYFONT_ERROR_IO;
        }

        clear_table_loader_state();
        if(load_ttf_table_dir_data(font->file_descriptor) != 0){
                clear_table_loader_state();
                myfont_close(font);
                return MYFONT_ERROR_INVALID_FONT;
        }

        font->table_directory = table_dir_data;
        memset(&table_dir_data,0,sizeof(table_dir_data));
        (void)offset_table_ctl(NULL,NULL,NULL,TABLE_FREE);

        struct table_record *cmap_record = found_tag_table(
                &font->table_directory,
                (uint8_t *)"cmap");
        struct table_record *head_record = found_tag_table(
                &font->table_directory,
                (uint8_t *)"head");
        struct table_record *maxp_record = found_tag_table(
                &font->table_directory,
                (uint8_t *)"maxp");
        struct table_record *loca_record = found_tag_table(
                &font->table_directory,
                (uint8_t *)"loca");
        struct table_record *glyf_record = found_tag_table(
                &font->table_directory,
                (uint8_t *)"glyf");

        if(cmap_record == NULL || head_record == NULL || maxp_record == NULL ||
                loca_record == NULL || glyf_record == NULL){
                myfont_close(font);
                return MYFONT_ERROR_INVALID_FONT;
        }

        myfont_result result = load_cmap_format4(font,cmap_record);
        if(result != MYFONT_SUCCESS){
                myfont_close(font);
                return result;
        }

        font->glyph_count = get_maxp_table_numGlyphs_data(
                maxp_record->offcet,
                font->file_descriptor);
        font->units_per_em = get_head_table_units_per_em(
                head_record->offcet,
                font->file_descriptor);
        int16_t loca_format = get_head_table_indexToLocFormat(
                head_record->offcet,
                font->file_descriptor);

        if(font->glyph_count == 0 || font->units_per_em == 0 ||
                (loca_format != 0 && loca_format != 1)){
                myfont_close(font);
                return MYFONT_ERROR_INVALID_FONT;
        }

        if(loca_table_eazy_parse(
                font->file_descriptor,
                loca_format,
                *loca_record,
                font->glyph_count,
                &font->loca) != 0){
                myfont_close(font);
                return MYFONT_ERROR_INVALID_FONT;
        }

        font->glyf_record = *glyf_record;
        *out_font = font;
        return MYFONT_SUCCESS;
}

void myfont_close(myfont_font *font){
        if(font == NULL)return;

        if(font->file_descriptor >= 0)close(font->file_descriptor);
        free(font->table_directory.tableRecords);
        free_format4(&font->cmap_format4);
        free_loca(&font->loca);
        free(font);
}

myfont_result myfont_load_glyph(
        myfont_font *font,
        uint32_t codepoint,
        myfont_glyph **out_glyph){

        if(font == NULL || out_glyph == NULL){
                return MYFONT_ERROR_INVALID_ARGUMENT;
        }
        *out_glyph = NULL;
        if(codepoint > UINT16_MAX)return MYFONT_ERROR_UNSUPPORTED;

        struct cmap_format4_segment *segment = found_input_chr_range(
                &font->cmap_format4,
                (uint16_t)codepoint);
        if(segment == NULL)return MYFONT_ERROR_NOT_FOUND;

        uint16_t glyph_id = 0;
        int glyph_id_result = get_glyph_id(
                &font->cmap_format4,
                segment,
                &glyph_id);
        free(segment);

        if(glyph_id_result != 0)return MYFONT_ERROR_INVALID_FONT;
        if(glyph_id == 0 && codepoint != 0)return MYFONT_ERROR_NOT_FOUND;
        if(glyph_id >= font->glyph_count)return MYFONT_ERROR_INVALID_FONT;

        uint32_t glyph_start = 0;
        uint32_t glyph_end = 0;
        if(get_glyph_loca_pos(
                &font->loca,
                glyph_id,
                &glyph_start,
                &glyph_end) != 0){
                return MYFONT_ERROR_INVALID_FONT;
        }
        if(glyph_start > glyph_end || glyph_end > font->glyf_record.length){
                return MYFONT_ERROR_INVALID_FONT;
        }
        if(glyph_start == glyph_end)return MYFONT_ERROR_NO_OUTLINE;

        myfont_glyph *glyph = calloc(1,sizeof(*glyph));
        if(glyph == NULL)return MYFONT_ERROR_ALLOCATION;
        glyph->glyph_id = glyph_id;

        if(glyph_start > UINT32_MAX - font->glyf_record.offcet){
                myfont_glyph_destroy(glyph);
                return MYFONT_ERROR_INVALID_FONT;
        }
        uint32_t glyph_file_position = font->glyf_record.offcet + glyph_start;
        if(parse_glyph_data_table(
                font->file_descriptor,
                glyph_file_position,
                &glyph->glyf) != 0){
                myfont_glyph_destroy(glyph);
                return MYFONT_ERROR_UNSUPPORTED;
        }
        if(get_countour_data(&glyph->glyf,&glyph->contours) != 0){
                myfont_glyph_destroy(glyph);
                return MYFONT_ERROR_INVALID_FONT;
        }

        *out_glyph = glyph;
        return MYFONT_SUCCESS;
}

void myfont_glyph_destroy(myfont_glyph *glyph){
        if(glyph == NULL)return;

        free_countour_data(&glyph->contours);
        free(glyph->glyf.end_pts_of_contours);
        free(glyph->glyf.instructions);
        free(glyph->glyf.flags);
        free(glyph->glyf.points);
        free(glyph);
}

uint16_t myfont_units_per_em(const myfont_font *font){
        return font == NULL ? 0 : font->units_per_em;
}

uint16_t myfont_glyph_id(const myfont_glyph *glyph){
        return glyph == NULL ? 0 : glyph->glyph_id;
}

size_t myfont_glyph_contour_count(const myfont_glyph *glyph){
        return glyph == NULL ? 0 : glyph->contours.pos_data_arry_size;
}

size_t myfont_glyph_point_count(
        const myfont_glyph *glyph,
        size_t contour_index){

        if(glyph == NULL ||
                contour_index >= glyph->contours.pos_data_arry_size)return 0;
        return glyph->contours.pos_data[contour_index].point_pos_data_arry_size;
}

myfont_result myfont_glyph_get_bounds(
        const myfont_glyph *glyph,
        int16_t *x_min,
        int16_t *y_min,
        int16_t *x_max,
        int16_t *y_max){

        if(glyph == NULL)return MYFONT_ERROR_INVALID_ARGUMENT;
        if(x_min != NULL)*x_min = glyph->glyf.x_min;
        if(y_min != NULL)*y_min = glyph->glyf.y_min;
        if(x_max != NULL)*x_max = glyph->glyf.x_max;
        if(y_max != NULL)*y_max = glyph->glyf.y_max;
        return MYFONT_SUCCESS;
}

myfont_result myfont_glyph_get_point(
        const myfont_glyph *glyph,
        size_t contour_index,
        size_t point_index,
        int16_t *x,
        int16_t *y,
        int *on_curve){

        if(glyph == NULL || x == NULL || y == NULL || on_curve == NULL){
                return MYFONT_ERROR_INVALID_ARGUMENT;
        }
        if(contour_index >= glyph->contours.pos_data_arry_size){
                return MYFONT_ERROR_NOT_FOUND;
        }

        const struct contour_pos_data *contour =
                &glyph->contours.pos_data[contour_index];
        if(point_index >= contour->point_pos_data_arry_size){
                return MYFONT_ERROR_NOT_FOUND;
        }

        const struct point_pos *point = &contour->point_pos_data[point_index];
        *x = point->pos_x;
        *y = point->pos_y;
        *on_curve = (point->flags & ON_CURVE_POINT) != 0;
        return MYFONT_SUCCESS;
}

myfont_result myfont_show_glyph(
        const myfont_font *font,
        const myfont_glyph *glyph){

        if(font == NULL || glyph == NULL)return MYFONT_ERROR_INVALID_ARGUMENT;
        if(show_glyph_with_raylib(
                &glyph->contours,
                &glyph->glyf,
                font->units_per_em) != 0){
                return MYFONT_ERROR_IO;
        }
        return MYFONT_SUCCESS;
}

const char *myfont_result_string(myfont_result result){
        switch(result){
                case MYFONT_SUCCESS:
                        return "success";
                case MYFONT_ERROR_INVALID_ARGUMENT:
                        return "invalid argument";
                case MYFONT_ERROR_ALLOCATION:
                        return "memory allocation failed";
                case MYFONT_ERROR_IO:
                        return "file or display I/O failed";
                case MYFONT_ERROR_INVALID_FONT:
                        return "invalid TrueType font data";
                case MYFONT_ERROR_UNSUPPORTED:
                        return "unsupported font feature";
                case MYFONT_ERROR_NOT_FOUND:
                        return "character or data not found";
                case MYFONT_ERROR_NO_OUTLINE:
                        return "glyph has no outline";
                default:
                        return "unknown myfont error";
        }
}
