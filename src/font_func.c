

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
#define TABLE_FREE 4
#define CMAP_ENCODING_TYPE_COUNT 4

#define format4_uint16_mem 6

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
        CMAP_PLATFORM_UNICODE   = 0,
        CMAP_PLATFORM_MACINTOSH = 1,
        CMAP_PLATFORM_ISO       = 2,
        CMAP_PLATFORM_WINDOWS   = 3,
        CMAP_PLATFORM_CUSTOM    = 4,
};

/* platformID == CMAP_PLATFORM_UNICODE のときだけ使用する encodingID。 */
enum cmap_unicode__platform_id {
        CMAP_UNICODE_1_0_DEPRECATED      = 0,
        CMAP_UNICODE_1_1_DEPRECATED      = 1,
        CMAP_UNICODE_ISO_10646_DEPRECATED = 2,
        CMAP_UNICODE_BMP                 = 3, /* format 4 または 6 */
        CMAP_UNICODE_FULL_REPERTOIRE      = 4, /* format 10 または 12 */
        CMAP_UNICODE_VARIATION_SEQUENCES  = 5, /* format 14 */
        CMAP_UNICODE_LAST_RESORT          = 6, /* format 13 */
};

/* platformID == CMAP_PLATFORM_WINDOWS のときだけ使用する encodingID。 */
enum cmap_windows_encoding_id {
        CMAP_WINDOWS_SYMBOL        = 0,
        CMAP_WINDOWS_UNICODE_BMP   = 1,  /* format 4 */
        CMAP_WINDOWS_SHIFT_JIS     = 2,
        CMAP_WINDOWS_PRC           = 3,
        CMAP_WINDOWS_BIG5          = 4,
        CMAP_WINDOWS_WANSUNG       = 5,
        CMAP_WINDOWS_JOHAB         = 6,
        CMAP_WINDOWS_RESERVED_7    = 7,
        CMAP_WINDOWS_RESERVED_8    = 8,
        CMAP_WINDOWS_RESERVED_9    = 9,
        CMAP_WINDOWS_UNICODE_FULL  = 10, /* format 12 */
};


struct format4_data{
        /* format 4サブテーブル全体のバイト数。 */
        uint16_t length;
        /* サブテーブルの言語ID。通常は0。 */
        uint16_t language;
        /* 文字コード範囲（セグメント）の数。ファイル上のsegCountX2を2で割った値。 */
        uint16_t seg_count;
        /* 2の累乗に基づくセグメント検索用の値。 */
        uint16_t search_range;
        /* searchRangeから導かれる二分探索用の値。 */
        uint16_t entry_selector;
        /* segCountX2とsearchRangeの差から求められる検索用の値。 */
        uint16_t range_shift;
        /* 各セグメントが担当するUnicode範囲の終端値。要素数はseg_count。 */
        uint16_t *end_code;
        /* endCodeとstartCodeの間に置かれる予約値。仕様上は0。 */
        uint16_t reserved_pad;
        /* 各セグメントが担当するUnicode範囲の開始値。要素数はseg_count。 */
        uint16_t *start_code;
        /* Unicode値またはglyphIdArrayの値に加える符号付き補正値。 */
        int16_t *id_delta;
        /* glyphIdArrayを参照するときの、各要素自身を基準とした相対オフセット。 */
        uint16_t *id_range_offset;
        /* idRangeOffsetが0以外の場合に参照するglyph IDの配列。 */
        uint16_t *glyph_id_array;
        /* glyphIdArrayに格納されている要素数。 */
        size_t glyph_id_count;
};

struct cmap_sub_table_data{
        int format_type;
};

struct table_record;
struct offset_tag_dic{
        uint8_t tag[tag_name_size];
        long pos;
        int table_num;//テーブルレコードの何番目か
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
        /*0x00010000 または 0x4F54544F ('OTTO') */
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

/*
 * 以下の構造体は、解析したテーブルの値を保持するためのもの。
 * 構造体のパディングとファイルのビッグエンディアン表現があるため、
 * ファイルから構造体全体へ直接readせず、各メンバを個別に読み込む。
 */
struct maxp_table_data{
        /* maxpのバージョン。TrueType輪郭では通常0x00010000。 */
        uint32_t version;
        /* フォントに含まれるグリフの総数。locaの要素数はこの値+1。 */
        uint16_t num_glyphs;
        /* Version 1.0専用。単純グリフ1個に含まれる最大点数。 */
        uint16_t max_points;
        /* 単純グリフ1個に含まれる最大輪郭数。 */
        uint16_t max_contours;
        /* 複合グリフ1個を展開したときの最大点数。 */
        uint16_t max_composite_points;
        /* 複合グリフ1個を展開したときの最大輪郭数。 */
        uint16_t max_composite_contours;
        /* TrueType命令で使用するゾーン数。通常は1または2。 */
        uint16_t max_zones;
        /* Twilight Zoneで使用する最大点数。 */
        uint16_t max_twilight_points;
        /* TrueType命令が使用するStorage Areaの最大要素数。 */
        uint16_t max_storage;
        /* fpgmテーブルで定義される関数の最大数。 */
        uint16_t max_function_defs;
        /* TrueType命令で定義される命令定義の最大数。 */
        uint16_t max_instruction_defs;
        /* TrueType命令実行時に必要なスタックの最大要素数。 */
        uint16_t max_stack_elements;
        /* 1グリフに格納されるヒンティング命令の最大バイト数。 */
        uint16_t max_size_of_instructions;
        /* 複合グリフが直接参照する部品グリフの最大数。 */
        uint16_t max_component_elements;
        /* 複合グリフ参照の最大階層数。 */
        uint16_t max_component_depth;
};

struct head_table_data{
        /* headテーブルのメジャーバージョン。通常は1。 */
        uint16_t major_version;
        /* headテーブルのマイナーバージョン。通常は0。 */
        uint16_t minor_version;
        /* フォント製作者が設定する16.16固定小数点形式の改訂番号。 */
        int32_t font_revision;
        /* フォント全体のチェックサムを調整する値。 */
        uint32_t checksum_adjustment;
        /* headテーブル確認用の固定値0x5F0F3CF5。 */
        uint32_t magic_number;
        /* ベースラインやヒンティング特性などを示すビットフラグ。 */
        uint16_t flags;
        /* 輪郭座標で1emを表すフォントデザイン単位数。 */
        uint16_t units_per_em;
        /* 1904年1月1日を基準とするフォント作成日時。 */
        int64_t created;
        /* 1904年1月1日を基準とする最終更新日時。 */
        int64_t modified;
        /* フォント内の全輪郭点における最小X座標。 */
        int16_t x_min;
        /* フォント内の全輪郭点における最小Y座標。 */
        int16_t y_min;
        /* フォント内の全輪郭点における最大X座標。 */
        int16_t x_max;
        /* フォント内の全輪郭点における最大Y座標。 */
        int16_t y_max;
        /* 太字、斜体、下線などのスタイルを示すビットフラグ。 */
        uint16_t mac_style;
        /* 読みやすさが保たれる最小の推奨ピクセルサイズ。 */
        uint16_t lowest_rec_ppem;
        /* 非推奨の文字方向ヒント。現在の仕様では通常2。 */
        int16_t font_direction_hint;
        /* locaの形式。0はuint16、1はuint32のオフセット配列。 */
        int16_t index_to_loc_format;
        /* glyfデータ形式。現在の仕様では0。 */
        int16_t glyph_data_format;
};

/* glyfテーブル全体ではなく、各グリフ先頭の共通10バイトを保持する。 */
struct glyf_table{
        /* 0以上なら単純グリフの輪郭数、負数なら複合グリフ。 */
        int16_t number_of_contours;
        /* このグリフの全輪郭点における最小X座標。 */
        int16_t x_min;
        /* このグリフの全輪郭点における最小Y座標。 */
        int16_t y_min;
        /* このグリフの全輪郭点における最大X座標。 */
        int16_t x_max;
        /* このグリフの全輪郭点における最大Y座標。 */
        int16_t y_max;
};


/*すべてのテーブルは4バイト境界から開始する必要があり、テーブル間の残りのスペースはゼロで埋める必要があります。
各テーブルの長さは、パディング後の長さではなく、実際のデータの長さでテーブルレコードに記録する必要があります。*/

static struct ttf_table_dir_data table_dir_data = {0};
static struct cmap_encoding_record *cmap_encoding_record = NULL;
static struct using_chr_encode_type using_chr_encode_type ={0};
static long fd_pos = 0;//lseek用

int load_ttf(char *path);
int load_ttf_table_dir_data(int ttf_fd);
void parse_ttf_data(int ttf_fd);
int assignment_table_dir_mum(int counter,uint8_t *data);
long offset_table_ctl(long *offset,uint8_t *tag,int *table_num,int flags);
void check_available_record_type(
        struct can_use_encoding_type types[CMAP_ENCODING_TYPE_COUNT],
        enum os_type this_os);
enum os_type check_os();
void set_cmap_encoding_record(int plat_form_id,int encoding_id,
        struct cmap_encoding_record_id_table *id_table,
         struct can_use_encoding_type *can_use_encoding_record,
         struct cmap_encoding_record **encoding_record);
int  get_format_type(int ttf_fd,int *format_type);
static uint16_t read_be16(const uint8_t data[2]){
        return ((uint16_t)data[0] << 8) | data[1];
}
struct table_record *found_tag_table(struct ttf_table_dir_data *dir_data,uint8_t tagname[tag_name_size]);
static uint32_t read_be32(const uint8_t data[4]){
        return ((uint32_t)data[0] << 24) |
               ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8)  |
               data[3];
}
//正常なら0
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
int input_f4_data(struct format4_data *f4_data,uint16_t data,int f4_mem_counter);
int format4_data_parse(int ttf_fd,struct format4_data *f4_data);
struct cmap_format4_segment *found_input_chr_range(
        const struct format4_data *f4_data,uint16_t input_data);


//仮関数群////////////////
uint16_t get_maxp_table_numGlyphs_data(long offset,int ttf_fd);
int16_t get_head_table_indexToLocFormat(long offset,int ttf_fd);


////////////////////

bool numGlyph_check(uint16_t numGlyphs,uint16_t Glyph_id);

int get_glyph_id(
        const struct format4_data *f4_data,
        const struct cmap_format4_segment *f4_seg_data,
        uint16_t *glyph_id);

uint16_t parse_maxp_table(long offset,int ttf_fd);
int main(){
        //osチェック
        enum os_type this_os = check_os();
        if(this_os == Other){
                printf("This OS is not officially supported. Issues may occur.");
        }


        int ttf_font_fd = load_ttf("font_file/IBMPlexSansJP-Thin.ttf");
        if(ttf_font_fd == -1)return 0;
        lseek(ttf_font_fd,fd_pos,SEEK_SET);
        int ttf_load_result = load_ttf_table_dir_data(ttf_font_fd);
        if(ttf_load_result == -1){
                printf("ttf load error");
                return 0;
        }
        long cmap_record_pos = 0;
        int table_num = 0;
        if (offset_table_ctl(
                &cmap_record_pos,
                (uint8_t *)"cmap",
                &table_num,
                TABLE_GET) < 0){

                return 0;
        }

        
        if(cmap_record_pos < 0)return 0;
        uint32_t cmap_offset = table_dir_data.tableRecords[table_num].offcet;//cmapテーブルオフセット

        if(lseek(ttf_font_fd,cmap_offset,SEEK_SET) == -1)return 0;

        uint8_t cmap_header[4] = {0};
        if(read_exact(ttf_font_fd,cmap_header,sizeof(cmap_header)) < 0)return 0;

        uint16_t version    = read_be16(cmap_header);
        uint16_t num_tables = read_be16(cmap_header+2);

        printf("ver = %u\nnumtable = %u\n", version, num_tables);



        cmap_encoding_record = calloc(num_tables,sizeof(struct cmap_encoding_record));
        if(cmap_encoding_record == NULL)return 0;


        uint8_t encoding_data[8];
        
        struct cmap_encoding_record_id_table id_table[num_tables];
        id_table->table_size = num_tables;

        for(int i = 0;i<num_tables;i++){
                read_exact(ttf_font_fd,encoding_data,sizeof(encoding_data));
                cmap_encoding_record[i].platform_id = read_be16(encoding_data);
                cmap_encoding_record[i].encoding_id = read_be16(encoding_data + 2);
                cmap_encoding_record[i].offset      = read_be32(encoding_data + 4);
                //テーブル生成
                id_table[i].record_table.pid = cmap_encoding_record[i].platform_id;
                id_table[i].record_table.eid = cmap_encoding_record[i].encoding_id;
                id_table[i].record_table.ptr = &cmap_encoding_record[i];
        }

        //このOSで使えるエンコードタイプが入ってる配列
        struct can_use_encoding_type can_use_encoding_types[CMAP_ENCODING_TYPE_COUNT];
        check_available_record_type(can_use_encoding_types,this_os);

        int plid = 3;//本来ユーザーが決めるが、テストなので固定する
        int eid = 1;//
        struct cmap_encoding_record *selected_encoding_record = NULL;
        set_cmap_encoding_record(
                plid,
                eid,id_table,
                can_use_encoding_types,
                &selected_encoding_record);


        if(selected_encoding_record == NULL){
                printf("selected encoding record allocate error");
                return 0;
        }
        printf("pid = %d ",selected_encoding_record->platform_id);
        printf("eid = %d\n",selected_encoding_record->encoding_id);

        //cmap_offsetから選択したエンコードテーブルん位あるoffsetぶん移動する
        long jmp_range = cmap_offset + selected_encoding_record->offset;
        off_t lseek_result = lseek(ttf_font_fd,jmp_range,SEEK_SET);
        if(lseek_result == -1){
                printf("lseek error\n");
                return 0;
        }

        int format_type = 0;
        int result = get_format_type(ttf_font_fd,&format_type);
        if(result != 0)return 1;
        printf("format type = %u\n",format_type);

        struct format4_data *f4_data = NULL;
        if(format_type == format4){
                f4_data = calloc(1,sizeof(struct format4_data));
                if(f4_data == NULL)return 1;
                if(format4_data_parse(ttf_font_fd,f4_data) != 0){
                        printf("format 4 parse error\n");
                        free(f4_data);
                        return 1;
                }
        }
        else {
                printf("this is not format 4 i support only format 4\n");
                return 0;
        }
        
        uint16_t chr = 'A';
      
        struct cmap_format4_segment *segment_data = 
                found_input_chr_range(f4_data,chr);
        int main_result = 0;
        if(segment_data == NULL){
                printf("character segment not found\n");
                main_result = 1;
                goto cleanup;
        }

        printf("range st = %u ed = %u segmentnum = %d\n",segment_data->segment.start,
                segment_data->segment.end,segment_data->segment.segment_num);

        uint16_t glyph_id = 0;
        if(get_glyph_id(f4_data,segment_data,&glyph_id) != 0){
                printf("glyph ID lookup error\n");
                main_result = 1;
                goto cleanup;
        }
        printf("glyph id = %u\n",glyph_id);

        

        struct table_record *loca_table  = found_tag_table(&table_dir_data,(uint8_t *)"loca");
        struct table_record *head_table  = found_tag_table(&table_dir_data,(uint8_t *)"head");
        struct table_record *maxp_table  = found_tag_table(&table_dir_data,(uint8_t *)"maxp");

        bool cannot_allocate = false;
        if(loca_table == NULL ){
                printf("can not found loca table\n");
                cannot_allocate = true;
        }
        if(head_table == NULL){
                printf("can not found head table\n");
                cannot_allocate = true;
        }
        if(maxp_table == NULL ){
                printf("can not found maxp table\n");
                cannot_allocate = true;
        }
        if(cannot_allocate){
                goto cleanup;
        }

        uint16_t numGlyphs = get_maxp_table_numGlyphs_data(maxp_table->offcet,ttf_font_fd);
        if(numGlyphs == 0){
                printf("numGlyphs not found\n");
                goto cleanup;
        }
        
        bool is_numGlypg_ok = numGlyph_check(numGlyphs,glyph_id);
        if(is_numGlypg_ok == false){
                printf("numGlyph id error\n");
                goto cleanup;
        }
        printf("numGlyph = %u\n",numGlyphs);
        int16_t indexToLocFormat = 
                get_head_table_indexToLocFormat(head_table->offcet,ttf_font_fd);
        if(indexToLocFormat < 0){
                printf("can not get indexToLocFormat\n");
                goto cleanup;
        }
        printf("%d\n",indexToLocFormat);
        
        int loca_data_format_type;
        switch(indexToLocFormat){
                case 0:
                        loca_data_format_type = 2;
                        break;
                case 1:
                        loca_data_format_type = 4;
                        break;
                default:
                        printf("loca format error\n");
                        goto cleanup;
        }

        
        

        



        /*
         * 再開メモ（2026-07-22）: glyph IDからglyfデータの位置を求める。
         *
         * 1. Table Directoryから"head"、"loca"、"glyf"、"maxp"を探し、
         *    各テーブルのoffsetとlengthを取得する。
         * 2. maxp.numGlyphsを読み、glyph_idがnumGlyphs未満か確認する。
         * 3. headテーブル先頭から50バイト目のindexToLocFormatを読む。
         *    indexToLocFormat == 0: locaはuint16_t配列（Short形式）。
         *    indexToLocFormat == 1: locaはuint32_t配列（Long形式）。
         * 4. loca[glyph_id]とloca[glyph_id + 1]をビッグエンディアンで読む。
         *    Short形式では、読み取った各値を2倍してglyf内相対offsetにする。
         *    Long形式では、読み取った値をそのまま相対offsetとして使う。
         * 5. glyph開始位置 = glyf_offset + loca[glyph_id]、
         *    glyph終了位置 = glyf_offset + loca[glyph_id + 1]として計算する。
         * 6. 開始位置 <= 終了位置、終了相対offset <= glyf.lengthを確認する。
         *    開始位置 == 終了位置なら輪郭を持たないglyphとして扱う。
         * 7. glyph開始位置へ移動し、glyfヘッダーの10バイトを読む。
         *    numberOfContours、xMin、yMin、xMax、yMaxはint16_tとして扱う。
         *    numberOfContours >= 0なら単純glyph、負数なら複合glyphを解析する。
         * 8. read_exactとlseekの戻り値を毎回確認し、途中失敗時も確保済み
         *    メモリとファイルディスクリプタを解放できるようにする。
         */
      

        
        









        


cleanup:
        free(segment_data);
        free(table_dir_data.tableRecords);
        free(cmap_encoding_record);
        free(selected_encoding_record);
        free(f4_data->end_code);
        free(f4_data->start_code);
        free(f4_data->id_delta);
        free(f4_data->id_range_offset);
        free(f4_data->glyph_id_array);
        free(f4_data);
        return main_result;
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

        //tagテーブル生成
        if(offset_table_ctl(NULL,NULL,NULL,TABLE_CREATE) < 0){
                printf("offset_table_ctl error");
                exit(1);
        }
        
        if(table_dir_data.tableRecords == NULL)return -1;


        //テーブルレコード取得ループ
        uint8_t table_record_data[16] ={0};
        int table_record_data_size = sizeof(table_record_data);
        int table_record_data_read_counter = 0;

        long tag_offset = 0;
        
        
        for(int i = 0; i < table_dir_data.numTables;i++){ 
                readed_byte = read(ttf_fd,table_record_data,table_record_data_size);
                if(readed_byte < table_record_data_size){
                        table_record_data_read_counter += readed_byte;
                        continue;
                }
                long now_file_pos = lseek(ttf_fd,0,SEEK_CUR);
                now_file_pos -= readed_byte;//tagの開始位置を取得したいのでreadedbyte引く

                table_record_data_read_counter = 0;
                int moved_size = sizeof(table_dir_data.tableRecords->tag);
                memmove(&table_dir_data.tableRecords[i].tag,table_record_data,moved_size);
                //タグ取得
                offset_table_ctl(&now_file_pos,table_dir_data.tableRecords[i].tag,&i,TABLE_SET);
                int move_size = sizeof(table_dir_data.tableRecords->checksum);
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
        if((flags != TABLE_CREATE && tag == NULL) || 
                (flags == TABLE_SET) && table_num == NULL)return -1;

        static struct offset_tag_dic *offset_tag_dic = NULL;
        static int keeping_dic_count = 0;

        switch(flags){
                case TABLE_SET:{
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
                }

                case TABLE_GET:{
                        if(offset_tag_dic == NULL)return -1;
                        for(int i = 0;i<keeping_dic_count;i++){
                                if(memcmp(&offset_tag_dic[i].tag,tag,tag_name_size) == 0){
                                        if(offset != NULL)*offset = offset_tag_dic[i].pos;
                                        *table_num = offset_tag_dic[i].table_num;
                                        return offset_tag_dic[i].pos;
                                }
                        }
                }
                case TABLE_CREATE:{
                        if(offset_tag_dic != NULL)return -1;
                        offset_tag_dic = calloc(table_dir_data.numTables,
                                sizeof(struct offset_tag_dic));
                        if(offset_tag_dic == NULL)exit(1);
                        return 0;
                }
                case TABLE_FREE:{
                        if(offset_tag_dic == NULL)return -1;
                        free(offset_tag_dic);
                        return 0;
                }
        }
        return 0;
}


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

struct table_record *found_tag_table(struct ttf_table_dir_data *dir_data,uint8_t tagname[tag_name_size]){
        if(dir_data == NULL || tagname == NULL) return NULL;

        for(int i = 0;i < dir_data->numTables; i++){
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
        if(numGlyphs < Glyph_id || numGlyphs <= 0) return false;
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