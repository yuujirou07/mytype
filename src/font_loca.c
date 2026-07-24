#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "font_binary.h"
#include "font_loca.h"

int parse_2byte_loca_table(
        int ttf_fd,
        struct table_record table_record,
        struct loca_table_2byte_format *loca_table_2byte_format,
        uint16_t numGlyph){

        if(loca_table_2byte_format == NULL)return -1;
        if(lseek(ttf_fd,table_record.offcet,SEEK_SET) < 0){
                printf("lseek error\n");
                return -1;
        }
        
        uint8_t ui8_data_storage[2] = {0}; 
        size_t ui8_data_storage_size = sizeof(ui8_data_storage);
        int allocate_num = numGlyph +1;
        int allocate_size = allocate_num * sizeof(uint16_t);

        if((uint32_t)allocate_size != table_record.length){
                printf("allocate_size and loca table length is not same\n");
                return -1;
        }

        loca_table_2byte_format->offsets = malloc(allocate_size);
        if(loca_table_2byte_format->offsets == NULL){
                printf("loca_table_2byte_format offsets can not allocate\n");
                return -1;
        }
        uint16_t *loca_offsets = loca_table_2byte_format->offsets;

        for(int i = 0;i < allocate_num; i++){
                if(read_exact(ttf_fd,ui8_data_storage,ui8_data_storage_size) != 0){
                        printf("can not read locateble\n");
                        free(loca_offsets);
                        loca_table_2byte_format->offsets = NULL;
                        loca_table_2byte_format->offset_count = 0;
                        return -1;
                }
                uint16_t ui16_data = read_be16(ui8_data_storage);
                loca_offsets[i] = ui16_data;
        }

        loca_table_2byte_format->offset_count = allocate_num;

        return 0;
}


int parse_4byte_loca_table(
        int ttf_fd,
        struct table_record table_record,
        struct loca_table_4byte_format *loca_table_4byte_format,
        uint16_t numGlyph){
                
        if(loca_table_4byte_format == NULL)return -1;

        if(lseek(ttf_fd,table_record.offcet,SEEK_SET) < 0){
                printf("lseek error\n");
                return -1;
        }
        
        int allocate_num = numGlyph + 1;
        int allocate_size = allocate_num * sizeof(uint32_t);

        if((uint32_t)allocate_size != table_record.length){
                printf("allocate_size and loca table length is not same\n");
                return -1;
        }

        loca_table_4byte_format->offsets = malloc(allocate_size);
        if(loca_table_4byte_format->offsets == NULL){
                printf("loca_table_4byte_format offset can not allocate\n");
                return -1;
        }
        uint32_t *offsets = loca_table_4byte_format->offsets; 
        for(int i = 0; i < allocate_num; i++){
                uint8_t ui8_data_storage[4];
                if(read_exact(ttf_fd,ui8_data_storage,sizeof(ui8_data_storage)) != 0){
                        printf("can not read locateble(parse_4byte_loca_table func)\n");
                        free(offsets);
                        loca_table_4byte_format->offsets = NULL;
                        loca_table_4byte_format->offset_count = 0;
                        return -1;
                }

                offsets[i] = read_be32(ui8_data_storage);
                              
        }
        loca_table_4byte_format->offset_count = (size_t)allocate_num;
        return 0;
}



uint16_t get_2b_loca_arry_num_data(struct loca_table_2byte_format loca_table_2b,int num){
        if(loca_table_2b.offsets == NULL){
                printf("loca_table_2b offsets is NULL\n");
                return 0;
        }
        if(num < 0 || (size_t)num >= loca_table_2b.offset_count){
                printf("set loca table arry num error\n");
                return 0;
        }
        
        return loca_table_2b.offsets[num];
}

uint32_t get_4b_loca_arry_num_data(struct loca_table_4byte_format loca_table_4b,int num){
         if(loca_table_4b.offsets == NULL){
                printf("loca_table_2b offsets is NULL\n");
                return 0;
        }
        if(num < 0 || (size_t)num >= loca_table_4b.offset_count){
                printf("set loca table arry num error\n");
                return 0;
        }
        
        return loca_table_4b.offsets[num];
}

int get_glyph_loca_pos(
        const struct loca_table_data_wrapper *loca_table_data_wrapp,
        uint16_t glyph_id,
        uint32_t *glyph_start_pos,
        uint32_t *glyph_end_pos){

        if(loca_table_data_wrapp == NULL || glyph_start_pos == NULL ||
                glyph_end_pos == NULL)return -1;

        if(loca_table_data_wrapp->loca_table_format_type == byte_4){
                if(loca_table_data_wrapp->loca_table_4byte_format == NULL)return -1;

                *glyph_start_pos = get_4b_loca_arry_num_data(
                        *loca_table_data_wrapp->loca_table_4byte_format,
                        glyph_id);
                *glyph_end_pos = get_4b_loca_arry_num_data(
                        *loca_table_data_wrapp->loca_table_4byte_format,
                        glyph_id + 1);
        }else if(loca_table_data_wrapp->loca_table_format_type == byte_2){
                if(loca_table_data_wrapp->loca_table_2byte_format == NULL)return -1;

                *glyph_start_pos = (uint32_t)get_2b_loca_arry_num_data(
                        *loca_table_data_wrapp->loca_table_2byte_format,
                        glyph_id) * 2;
                *glyph_end_pos = (uint32_t)get_2b_loca_arry_num_data(
                        *loca_table_data_wrapp->loca_table_2byte_format,
                        glyph_id + 1) * 2;
        }else{
                return -1;
        }

        return 0;
}

int loca_table_eazy_parse(
        int ttf_fd,int indexToLocFormat,
        struct table_record loca_table,
        uint16_t numGlyphs,
        struct loca_table_data_wrapper *loca_table_data_wrapper){


        if(loca_table_data_wrapper == NULL || loca_table_data_wrapper->loca_table_2byte_format != NULL 
                || loca_table_data_wrapper->loca_table_4byte_format != NULL)return -1;

        
        int loca_teble_parse_result;
        switch(indexToLocFormat){
                case 0:
                        loca_table_data_wrapper->loca_table_format_type = byte_2;
                        loca_table_data_wrapper->loca_table_2byte_format = 
                                malloc(sizeof(struct loca_table_2byte_format));
                        if(loca_table_data_wrapper->loca_table_2byte_format == NULL){
                                printf("loca_table_data_wrapper 2byte_format mem allocate error\n");
                                return -1;
                        }

                        loca_teble_parse_result = 
                                parse_2byte_loca_table(
                                        ttf_fd,
                                        loca_table,
                                        loca_table_data_wrapper->loca_table_2byte_format,
                                        numGlyphs);

                        if(loca_teble_parse_result < 0){
                                printf("loca_teble_2byte_parse_result error");
                                return -1;
                        }

                        break;
                case 1:
                        loca_table_data_wrapper->loca_table_format_type = byte_4;
                        loca_table_data_wrapper->loca_table_4byte_format =
                                malloc(sizeof(struct loca_table_4byte_format));

                         if(loca_table_data_wrapper->loca_table_4byte_format == NULL){
                                printf("loca_table_data_wrapper 2byte_format mem allocate error\n");
                                return -1;
                        }

                        loca_teble_parse_result =
                                parse_4byte_loca_table(
                                        ttf_fd,
                                        loca_table,
                                        loca_table_data_wrapper->loca_table_4byte_format,
                                        numGlyphs);
                                
                                if(loca_teble_parse_result < 0){
                                        printf("loca_teble_4byte_parse_result error");
                                        return -1;
                                }
                        break;
                default:
                        printf("this loca format is not support\n");
                        return -1;
        }

        return 0;        
}
