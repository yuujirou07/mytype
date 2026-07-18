
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include<fcntl.h>
#include<stdbool.h>
#include<stdlib.h>
#include <string.h>

#define table_dir_int_mem 5
//ttf_table_dir_data内のメンバの最大容量(uint8_tの必要個数)
#define table_int_mem_max_size 4 
#define tag_name_size 4

#define TABLE_SET 0
#define TABLE_GET 1
#define TABLE_CREATE 3

struct offset_tag_dic{
        char tag[tag_name_size];
        long pos;
};

enum table_record_type{
       tabletag,
       checksum,
       offset,
       length,
};
struct table_record{
        uint8_t  tag[tag_name_size];
        uint32_t checksum;
        uint32_t offcet;
        uint32_t length;
};

struct ttf_table_dir_data{
        /*0x00010000 または 0x4F54544F ('OTTO') — 下記参照。*/
        uint32_t sfntVersion;
        /*テーブルの数。*/
        uint16_t numTables;
        /*numTables以下の2のべき乗の最大値に16を掛けた値（(2**floor(log2(numTables))) * 16、ここで「**」はべき乗演算子）。*/
        uint16_t searchRange;
        /*numTables以下の最大2乗のlog 2 （log 2（searchRange/16）、これはfloor(log 2（numTables）)に等しい）。*/
        uint16_t entrySelector;
        /*numTables × 16、- searchRange ((numTables * 16) - searchRange)。*/
        uint16_t rangeShift;
        /*テーブルレコード配列 ― フォント内の各トップレベルテーブルごとに1つ。*/
        struct table_record *tableRecords;
};

/*すべてのテーブルは4バイト境界から開始する必要があり、テーブル間の残りのスペースはゼロで埋める必要があります。
各テーブルの長さは、パディング後の長さではなく、実際のデータの長さでテーブルレコードに記録する必要があります。*/

static struct ttf_table_dir_data table_dir_data = {0};
static long fd_pos = 0;//lseek用

int load_ttf(char *path);
int load_ttf_table_dir_data(int ttf_fd);
void parse_ttf_data(int ttf_fd);
int assignment_table_dir_mum(int counter,uint8_t *data);
long offset_table_ctl(long *offset,char *tag,int flags);

static uint16_t read_be16(const uint8_t data[2]){
        return ((uint16_t)data[0] << 8) | data[1];
}

static uint32_t read_be32(const uint8_t data[4]){
        return ((uint32_t)data[0] << 24) |
               ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8)  |
               data[3];
}

static int read_exact(int fd, uint8_t *data, size_t size){
        size_t read_size = 0;

        while(read_size < size){
                int result = read(fd, data + read_size, size - read_size);
                if(result <= 0)return -1;
                read_size += result;
        }
        return 0;
}
uint16_t comb_ui8_2_ui16(const uint8_t high_byte,const uint8_t lower_byte);



int main(){
        int ttf_font_fd = load_ttf("font_file/IBMPlexSansJP-Thin.ttf");
        if(ttf_font_fd == -1)return 0;
        lseek(ttf_font_fd,fd_pos,SEEK_SET);
        int ttf_load_result = load_ttf_table_dir_data(ttf_font_fd);
        if(ttf_load_result == -1){
                printf("ttf load error");
                return 0;
        }
        struct table_record *cmp_table_data = NULL;
        offset_table_ctl(NULL,NULL,TABLE_CREATE);
        for(int i = 0;i< table_dir_data.numTables;i++){
                if(memcmp(table_dir_data.tableRecords[i].tag,"cmap",4)== 0){
                        cmp_table_data = &table_dir_data.tableRecords[i];
                        break;
                }
        }
        
        //cmapテーブル内のoffset値はcmapテーブル先頭からのバイト数で計算する
        


        free(table_dir_data.tableRecords);
        return 0;
}




//ttf fileの読み込み
int load_ttf(char *path){
        if(strstr(path,".ttf") == NULL)return -1;
        int ttf_font_fd = open(path, O_RDONLY | O_BINARY);
        return ttf_font_fd;
}

void parse_ttf_data(int ttf_fd){
        load_ttf_table_dir_data(ttf_fd);
}

int load_ttf_table_dir_data(int ttf_fd){
        int readed_byte = 0;
        uint8_t header[12];
        if(read_exact(ttf_fd, header, sizeof(header)) == -1)return -1;

        table_dir_data.sfntVersion = read_be32(header);
        table_dir_data.numTables = read_be16(header + 4);
        table_dir_data.searchRange = read_be16(header + 6);
        table_dir_data.entrySelector = read_be16(header + 8);
        table_dir_data.rangeShift = read_be16(header + 10);
        table_dir_data.tableRecords = calloc(table_dir_data.numTables,sizeof(struct table_record));
        if(table_dir_data.tableRecords == NULL)return -1;


        //テーブルレコード取得ループ
        uint8_t table_record_data[16] ={0};
        int table_record_data_size = sizeof(table_record_data);
        int table_record_data_read_counter = 0;

        for(int i = 0; i < table_dir_data.numTables;i++){ 
                readed_byte = read(ttf_fd,table_record_data,table_record_data_size);
                if(readed_byte < table_record_data_size){
                        table_record_data_read_counter += readed_byte;
                        continue;
                }
                table_record_data_read_counter = 0;

                int moved_size = sizeof(table_dir_data.tableRecords->tag);
                memmove(&table_dir_data.tableRecords[i].tag,table_record_data,moved_size);
                int move_size = sizeof(table_dir_data.tableRecords->checksum);
                memmove(&table_dir_data.tableRecords[i].checksum,table_record_data+moved_size,move_size);
                moved_size += move_size;
                move_size = sizeof(table_dir_data.tableRecords->offcet);
                memmove(&table_dir_data.tableRecords[i].offcet,table_record_data+moved_size,move_size);
                moved_size += move_size;
                move_size = sizeof(table_dir_data.tableRecords->length);
                memmove(&table_dir_data.tableRecords[i].offcet,table_record_data+moved_size,move_size);

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


long offset_table_ctl(long *offset,char *tag,int flags){
        if(flags != TABLE_CREATE && tag == NULL)return -1;

        static struct offset_tag_dic *offset_tag_dic = NULL;
        static int keeping_dic_count = 0;

        switch(flags){
                case TABLE_SET:{
                        if(offset_tag_dic == NULL || keeping_dic_count <= 0 ||
                                keeping_dic_count >= table_dir_data.numTables ||
                                offset_tag_dic == NULL || offset == NULL)return -1;

                        memcpy(&offset_tag_dic[keeping_dic_count].tag,
                                tag,
                                sizeof(char) * tag_name_size);

                        offset_tag_dic[keeping_dic_count].pos = *offset;
                        keeping_dic_count++;
                        return 0;
                }

                case TABLE_GET:{
                        
                }




        }
        return 0;
}