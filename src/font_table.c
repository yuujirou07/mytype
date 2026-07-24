#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "font_binary.h"
#include "font_table.h"

struct ttf_table_dir_data table_dir_data = {0};

void parse_ttf_data(int ttf_fd){
        load_ttf_table_dir_data(ttf_fd);
}

int load_ttf_table_dir_data(int ttf_fd){
        if(ttf_fd < 0){
                printf("ttf_fd error\n");
                return -1;
        }
        
        int readed_byte = 0;
        uint8_t header[12];
        if(read_exact(ttf_fd, header, sizeof(header)) == -1)return -1;

        table_dir_data.sfntVersion   = read_be32(header);
        table_dir_data.numTables     = read_be16(header + 4);
        table_dir_data.searchRange   = read_be16(header + 6);
        table_dir_data.entrySelector = read_be16(header + 8);
        table_dir_data.rangeShift    = read_be16(header + 10);
        table_dir_data.tableRecords  = 
                calloc(table_dir_data.numTables,sizeof(struct table_record));

        //tagテーブル生成
        if(offset_table_ctl(NULL,NULL,NULL,TABLE_CREATE) < 0){
                printf("offset_table_ctl error");
                return -1;
        }
        
        if(table_dir_data.tableRecords == NULL)return -1;


        //テーブルレコード取得ループ
        uint8_t table_record_data[16] ={0};
        int table_record_data_size = sizeof(table_record_data);
                
        for(int i = 0; i < table_dir_data.numTables;i++){ 
                readed_byte = read(ttf_fd,table_record_data,table_record_data_size);
                if(readed_byte < table_record_data_size){
                        continue;
                }
                long now_file_pos = lseek(ttf_fd,0,SEEK_CUR);
                now_file_pos -= readed_byte;//tagの開始位置を取得したいのでreadedbyte引く

                int moved_size = sizeof(table_dir_data.tableRecords->tag);
                memmove(&table_dir_data.tableRecords[i].tag,table_record_data,moved_size);
                //タグ取得
                offset_table_ctl(&now_file_pos,table_dir_data.tableRecords[i].tag,&i,TABLE_SET);
                table_dir_data.tableRecords[i].checksum =
                        read_be32(table_record_data + 4);

                table_dir_data.tableRecords[i].offcet =
                        read_be32(table_record_data + 8);

                table_dir_data.tableRecords[i].length =
                        read_be32(table_record_data + 12);

                memset(table_record_data,0,table_record_data_size);
        }
        return 0;
}

int assignment_table_dir_mum(int counter,uint8_t *data){
        if(data == NULL)return -1;
        switch(counter){
                case 0:
                        table_dir_data.sfntVersion = read_be32(data);
                        return 0;
                case 1:
                        table_dir_data.numTables = read_be16(data);
                        return 0;
                case 2:
                        table_dir_data.searchRange = read_be16(data);
                        return 0;
                case 3:
                        table_dir_data.entrySelector = read_be16(data);
                        return 0;
                case 4:
                        table_dir_data.rangeShift = read_be16(data);
                        return 0;
                default:
                        return -1;
        }
        return -1;         
}


long find_offset(char *tag){
        if(tag == NULL || strlen(tag) > 4)return -1;
        return 0;       
}


long offset_table_ctl(long *offset,uint8_t *tag,int *table_num,int flags){
        if((flags != TABLE_CREATE && flags != TABLE_FREE && tag == NULL) || 
                ((flags == TABLE_SET) && table_num == NULL))return -1;

        static struct offset_tag_dic *offset_tag_dic = NULL;
        static int keeping_dic_count = 0;

        switch(flags){
                case TABLE_SET:
                        if(offset_tag_dic == NULL || keeping_dic_count < 0 ||
                                keeping_dic_count >= table_dir_data.numTables ||
                                offset_tag_dic == NULL || offset == NULL)return -1;

                        memcpy(&offset_tag_dic[keeping_dic_count].tag,
                                tag,
                                sizeof(char) * tag_name_size);

                        offset_tag_dic[keeping_dic_count].pos = *offset;
                        offset_tag_dic[keeping_dic_count].table_num = *table_num;
                        keeping_dic_count++;
                        return 0;
                
                case TABLE_GET:
                        if(offset_tag_dic == NULL)return -1;
                        for(int i = 0;i<keeping_dic_count;i++){
                                if(memcmp(&offset_tag_dic[i].tag,tag,tag_name_size) == 0){
                                        if(offset != NULL)*offset = offset_tag_dic[i].pos;
                                        *table_num = offset_tag_dic[i].table_num;
                                        return offset_tag_dic[i].pos;
                                }
                        }
                        return 0;
                
                case TABLE_CREATE:
                        if(offset_tag_dic != NULL)return -1;
                        offset_tag_dic = calloc(table_dir_data.numTables,
                                sizeof(struct offset_tag_dic));
                        if(offset_tag_dic == NULL)return -1;
                        return 0;

                case TABLE_FREE:
                        if(offset_tag_dic == NULL)return -1;
                        free(offset_tag_dic);
                        offset_tag_dic = NULL;
                        keeping_dic_count = 0;
                        return 0;
                
        }
        return 0;
}


struct table_record *found_tag_table(struct ttf_table_dir_data *dir_data,uint8_t tagname[tag_name_size]){
        if(dir_data == NULL || tagname == NULL) return NULL;

        for(uint16_t i = 0;i < dir_data->numTables; i++){
                if(memcmp(dir_data->tableRecords[i].tag,
                        tagname,
                        sizeof(uint8_t) * tag_name_size)  == 0){
                        
                        return &dir_data->tableRecords[i];
                }
        }
        return NULL;
}


//仮関数
uint16_t get_maxp_table_numGlyphs_data(long offset,int ttf_fd){
        off_t result = lseek(ttf_fd,offset+4,SEEK_SET);
        if(result <= 0)return 0;
        uint8_t data_strage[2];
        if(read_exact(ttf_fd,data_strage,sizeof(data_strage)) != 0)return 0;
        uint16_t ui16_data = read_be16(data_strage);
        return ui16_data;
}

bool numGlyph_check(uint16_t numGlyphs,uint16_t Glyph_id){
        if(numGlyphs <= Glyph_id || numGlyphs <= 0) return false;
        return true;
}
int16_t get_head_table_indexToLocFormat(long offset,int ttf_fd){
        off_t result = lseek(ttf_fd,offset + 50,SEEK_SET);
        if(result <= 0)return -1;
        int8_t i8_data_strage[2];
        int read_result = read_exact(ttf_fd,(uint8_t *)i8_data_strage,sizeof(i8_data_strage));
        if(read_result != 0)return -1;
        int16_t i16_data = (int16_t)read_be16((uint8_t *)i8_data_strage);
        return i16_data;
}

uint16_t get_head_table_units_per_em(long offset,int ttf_fd){
        if(lseek(ttf_fd,offset + 18,SEEK_SET) < 0)return 0;

        uint8_t data_storage[2];
        if(read_exact(ttf_fd,data_storage,sizeof(data_storage)) != 0)return 0;

        return read_be16(data_storage);
}
