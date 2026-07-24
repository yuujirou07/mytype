#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "font_binary.h"
#include "font_cmap.h"

static struct using_chr_encode_type using_chr_encode_type = {0};

int select_encoding(uint16_t platform_id,uint16_t encoding_id){
        using_chr_encode_type.platform_id = platform_id;
        using_chr_encode_type.encode_type = 
                (encoding_id != windows_encode) ? unicode_encode : windows_encode;
        using_chr_encode_type.encode_num = encoding_id;

        return 0;       
}

enum os_type check_os(){
        enum os_type os;
        #if defined(_WIN32)
                os = Windows;
        #elif defined(__APPLE__) && defined(__MACH__)
                os = Mac;
        #elif defined(__linux__)
                os = Linux;
        #else
                os = Other;
        #endif
        return os;
}
void check_available_record_type(
        struct can_use_encoding_type types[CMAP_ENCODING_TYPE_COUNT],
        enum os_type this_os){
        static const struct can_use_encoding_type windows_types[] = {
                {CMAP_PLATFORM_WINDOWS, CMAP_WINDOWS_UNICODE_FULL},
                {CMAP_PLATFORM_WINDOWS, CMAP_WINDOWS_UNICODE_BMP},
                {CMAP_PLATFORM_UNICODE, CMAP_UNICODE_FULL_REPERTOIRE},
                {CMAP_PLATFORM_UNICODE, CMAP_UNICODE_BMP}
        };
        static const struct can_use_encoding_type unicode_types[] = {
                {CMAP_PLATFORM_UNICODE, CMAP_UNICODE_FULL_REPERTOIRE},
                {CMAP_PLATFORM_UNICODE, CMAP_UNICODE_BMP},
                {CMAP_PLATFORM_WINDOWS, CMAP_WINDOWS_UNICODE_FULL},
                {CMAP_PLATFORM_WINDOWS, CMAP_WINDOWS_UNICODE_BMP}
        };

        if(types == NULL)return;

        const struct can_use_encoding_type *source = unicode_types;
        if(this_os == Windows)source = windows_types;

        memcpy(types,source,sizeof(windows_types));
}

/*エンコードテーブルと、この端末で使えるエンコードタイプをつかい、
ユーザーが指定したidが使用可能ならencoding_recordにデータを入れる*/
void set_cmap_encoding_record(int plat_form_id,int encoding_id,
        struct cmap_encoding_record_id_table *id_table,
        struct can_use_encoding_type *can_use_encoding_record,
        struct cmap_encoding_record **encoding_record){

        if(*encoding_record != NULL || id_table == NULL)return;
        struct cmap_encoding_record *tmp_record = NULL;
        for(int i = 0;i < id_table->table_size;i++){
                //ファイルで検出したデータ
                if(id_table[i].record_table.pid != plat_form_id)continue;
                if(id_table[i].record_table.eid != encoding_id)continue;
                //この端末で使えるidが入っているテーブル（ファイルにデータが入っていたわけではない）
                bool fond_can_use_id = false;
                for(int j = 0;j < CMAP_ENCODING_TYPE_COUNT;j++){
                        if(can_use_encoding_record[j].pid != plat_form_id)continue;
                        if(can_use_encoding_record[j].eid != encoding_id)continue;
                        fond_can_use_id = true;
                        break;
                }
                if(fond_can_use_id){
                        tmp_record = id_table[i].record_table.ptr;
                        break;
                }
                else return;
        }
        if(tmp_record == NULL)return;
        *encoding_record = calloc(1,sizeof(struct cmap_encoding_record));
        memcpy(*encoding_record,tmp_record,sizeof(struct cmap_encoding_record));
        return;
}
int  get_format_type(int ttf_fd,int *format_type){
        uint8_t data_strage[2];
        int result = read_exact(ttf_fd,data_strage,sizeof(data_strage));
        if(result != 0)return -1;
        uint16_t type = read_be16(data_strage);

        switch(type){
                case format4:
                        *format_type = 4;
                        break;
                case format12:
                        *format_type = 12;
                        break;
                default:
                        *format_type = 0;
                        return -1;
        }
        return 0;
}

int format4_data_parse(int ttf_fd,struct format4_data *f4_data){
        if(f4_data == NULL)return -1;
        uint8_t ui8_data[2];

        for(int i=0;i<format4_uint16_mem;i++){
                if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                uint16_t u16_data = read_be16(ui8_data);

                if(i == 2 && (u16_data == 0 || (u16_data & 1) != 0))goto error;
                if(input_f4_data(f4_data,u16_data,i) != 0)goto error;
        }

        uint16_t seg_count  = f4_data->seg_count;
        size_t fixed_and_segment_size = 16u + (size_t)seg_count * 8u;
        if((size_t)f4_data->length < fixed_and_segment_size)goto error;

        f4_data->end_code   = malloc(sizeof(*f4_data->end_code) * seg_count);
        f4_data->start_code = malloc(sizeof(*f4_data->start_code) * seg_count);
        f4_data->id_delta   = malloc(sizeof(*f4_data->id_delta) * seg_count);
        f4_data->id_range_offset =
                malloc(sizeof(*f4_data->id_range_offset) * seg_count);

        if(f4_data->end_code == NULL || f4_data->start_code == NULL ||
                f4_data->id_delta == NULL ||
                f4_data->id_range_offset == NULL)goto error;

        for(size_t i = 0;i < seg_count;i++){
                if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                f4_data->end_code[i] = read_be16(ui8_data);
        }

        if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
        f4_data->reserved_pad = read_be16(ui8_data);
        if(f4_data->reserved_pad != 0)goto error;

        for(size_t i = 0;i < seg_count;i++){
                if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                f4_data->start_code[i] = read_be16(ui8_data);
        }

        for(size_t i = 0;i < seg_count;i++){
                if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                f4_data->id_delta[i] = (int16_t)read_be16(ui8_data);
        }

        for(size_t i = 0;i < seg_count;i++){
                if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                f4_data->id_range_offset[i] = read_be16(ui8_data);
        }

        size_t glyph_id_bytes = (size_t)f4_data->length - fixed_and_segment_size;
        if((glyph_id_bytes & 1u) != 0)goto error;
        f4_data->glyph_id_count = glyph_id_bytes / sizeof(uint16_t);

        if(f4_data->glyph_id_count > 0){
                f4_data->glyph_id_array = malloc(
                        sizeof(*f4_data->glyph_id_array) * f4_data->glyph_id_count);
                if(f4_data->glyph_id_array == NULL)goto error;

                for(size_t i = 0;i < f4_data->glyph_id_count;i++){
                        if(read_exact(ttf_fd,ui8_data,sizeof(ui8_data)) != 0)goto error;
                        f4_data->glyph_id_array[i] = read_be16(ui8_data);
                }
        }

        return 0;

error:
        free(f4_data->end_code);
        free(f4_data->start_code);
        free(f4_data->id_delta);
        free(f4_data->id_range_offset);
        free(f4_data->glyph_id_array);
        f4_data->end_code = NULL;
        f4_data->start_code = NULL;
        f4_data->id_delta = NULL;
        f4_data->id_range_offset = NULL;
        f4_data->glyph_id_array = NULL;
        f4_data->glyph_id_count = 0;
        return -1;
}
int input_f4_data(struct format4_data *f4_data,uint16_t data,int f4_mem_counter){
        if(f4_data == NULL || f4_mem_counter < 0 ||
                f4_mem_counter >= format4_uint16_mem)return -1;

        switch(f4_mem_counter){
                case 0:
                        f4_data->length = data;
                        break;
                case 1:
                        f4_data->language = data;
                        break;
                case 2:
                        f4_data->seg_count = data/2;
                        break;
                case 3:
                        f4_data->search_range = data;
                        break;
                case 4:
                        f4_data->entry_selector = data;
                        break;
                case 5:
                        f4_data->range_shift = data;
                        break;
        }
        return 0;
}

struct cmap_format4_segment *found_input_chr_range(
        const struct format4_data *f4_data,uint16_t input_data){
        if(f4_data == NULL)return NULL;

        for(int i=0;i < f4_data->seg_count;i++){
                if(f4_data->start_code[i] > input_data ||
                        input_data > f4_data->end_code[i])continue;

                struct cmap_format4_segment *f4_format_seg =
                        malloc(sizeof(*f4_format_seg));
                if(f4_format_seg == NULL)return NULL;

                f4_format_seg->chr = input_data;
                f4_format_seg->segment.segment_num = i;
                f4_format_seg->segment.start = f4_data->start_code[i];
                f4_format_seg->segment.end = f4_data->end_code[i];
                return f4_format_seg;
        }
        return NULL;
}


int get_glyph_id(
        const struct format4_data *f4_data,
        const struct cmap_format4_segment *f4_seg_data,
        uint16_t *glyph_id){
        if(f4_data == NULL || f4_seg_data == NULL || glyph_id == NULL)return -1;

        int segment_num = f4_seg_data->segment.segment_num;
        if(segment_num < 0 || segment_num >= f4_data->seg_count)return -1;

        uint16_t range_offset = f4_data->id_range_offset[segment_num];
        if(range_offset == 0){
                *glyph_id = (uint16_t)(f4_seg_data->chr +
                        f4_data->id_delta[segment_num]);
                return 0;
        }

        if((range_offset & 1u) != 0 || f4_data->glyph_id_array == NULL)return -1;

        int64_t glyph_array_num = (int64_t)(range_offset / 2u) +
                (int64_t)(f4_seg_data->chr - f4_seg_data->segment.start) -
                ((int64_t)f4_data->seg_count - segment_num);
        if(glyph_array_num < 0 ||
                (uint64_t)glyph_array_num >= f4_data->glyph_id_count)return -1;

        uint16_t glyph_value = f4_data->glyph_id_array[glyph_array_num];
        if(glyph_value == 0){
                *glyph_id = 0;
                return 0;
        }

        *glyph_id = (uint16_t)(glyph_value + f4_data->id_delta[segment_num]);
        return 0;
}
