#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myfont.h"
#include"font_bitmap.h"
/* myfont_result のエラーをoperation名付きでstderrへ出力し、常に1を返す。 */
static int report_error(const char *operation,myfont_result result){
        fprintf(stderr,"%s: %s\n",operation,myfont_result_string(result));
        return 1;
}

/* first_byteから始まるUTF-8のバイト列をstdinから読み進め、Unicode
   コードポイントへデコードする。継続バイトが不正、またはEOFに達した
   場合は-1を返す(呼び出し側はそのまま読み捨てて次へ進む想定)。 */
static int decode_utf8_codepoint(int first_byte,uint32_t *out_codepoint){
        uint32_t codepoint = 0;
        int continuation_count = 0;

        if((first_byte & 0x80) == 0x00){
                codepoint = (uint32_t)first_byte;
                continuation_count = 0;
        }else if((first_byte & 0xE0) == 0xC0){
                codepoint = (uint32_t)(first_byte & 0x1F);
                continuation_count = 1;
        }else if((first_byte & 0xF0) == 0xE0){
                codepoint = (uint32_t)(first_byte & 0x0F);
                continuation_count = 2;
        }else if((first_byte & 0xF8) == 0xF0){
                codepoint = (uint32_t)(first_byte & 0x07);
                continuation_count = 3;
        }else{
                return -1;
        }

        for(int i = 0;i < continuation_count;i++){
                int continuation_byte = _getchar_nolock();
                if(continuation_byte == EOF ||
                        (continuation_byte & 0xC0) != 0x80){
                        return -1;
                }
                codepoint = (codepoint << 6) |
                        (uint32_t)(continuation_byte & 0x3F);
        }

        *out_codepoint = codepoint;
        return 0;
}

/* ビットマップをターミナル幅に収まるよう間引きながら、#(輪郭内)と
   .(輪郭外)でASCIIアート表示する。間引きブロック内に1つでも輪郭内画素が
   あれば#にする。 */
static void print_glyph_bitmap(const struct glyph_bitmap *bitmap){
        const int max_columns = 60;
        const int max_rows = 30;

        if(bitmap->bitmap == NULL || bitmap->width <= 0 || bitmap->height <= 0){
                printf("(空白のグリフです)\n");
                return;
        }

        int column_step = (bitmap->width + max_columns - 1) / max_columns;
        int row_step = (bitmap->height + max_rows - 1) / max_rows;
        if(column_step < 1)column_step = 1;
        if(row_step < 1)row_step = 1;

        for(int row = 0;row < bitmap->height;row += row_step){
                for(int column = 0;column < bitmap->width;column += column_step){
                        int filled = 0;

                        for(int sub_row = row;
                                sub_row < row + row_step &&
                                        sub_row < bitmap->height && !filled;
                                sub_row++){
                                for(int sub_column = column;
                                        sub_column < column + column_step &&
                                                sub_column < bitmap->width;
                                        sub_column++){
                                        if(bitmap->bitmap[sub_row][sub_column]){
                                                filled = 1;
                                                break;
                                        }
                                }
                        }

                        putchar(filled ? '#' : '.');
                }
                putchar('\n');
        }
}

/* サンプルmain。フォントを開いて1文字読み込み、境界と輪郭情報を表示する。
   --no-windowを付けない限り、raylibビューアも起動する。
   引数: [フォントパス] [コードポイント] [--no-window] */
int main(int argc,char **argv){
        const char *font_path = argc > 1
                ? argv[1]
                : "font_file/IBMPlexSansJP-Thin.ttf";

        int key = 'i';

        uint32_t codepoint = argc > 2
                ? (uint32_t)strtoul(argv[2],NULL,0)
                : (uint32_t)key;


        myfont_font *font = NULL;
        myfont_glyph *glyph = NULL;

        myfont_result result = myfont_open(font_path,&font);
        if(result != MYFONT_SUCCESS)return report_error("myfont_open",result);

        result = myfont_load_glyph(font,codepoint,&glyph);
        if(result != MYFONT_SUCCESS){
                myfont_close(font);
                return report_error("myfont_load_glyph",result);
        }

        int16_t x_min = 0;
        int16_t y_min = 0;
        int16_t x_max = 0;
        int16_t y_max = 0;
        (void)myfont_glyph_get_bounds(
                glyph,
                &x_min,
                &y_min,
                &x_max,
                &y_max);

        size_t contour_count = myfont_glyph_contour_count(glyph);
        size_t total_points = 0;
        for(size_t i = 0;i < contour_count;i++){
                total_points += myfont_glyph_point_count(glyph,i);
        }

        printf(
                "codepoint: U+%04X\n",
                (unsigned int)myfont_glyph_codepoint(glyph));
        printf("glyph ID: %u\n",myfont_glyph_id(glyph));
        printf("units per em: %u\n",myfont_units_per_em(font));
        printf("bounds: (%d, %d) - (%d, %d)\n",x_min,y_min,x_max,y_max);
        printf("contours: %zu, points: %zu\n",contour_count,total_points);

        int exit_code = 0;

        printf("\n文字を入力するとビットマップを表示します(ESCで終了):\n");

        int keep_running = 1;
        while(keep_running){
                int first_byte = _getchar_nolock();
                if(first_byte == EOF)break;
                if(first_byte == 0x1B){ /* ESC */
                        keep_running = 0;
                        continue;
                }
                if(first_byte == '\r' || first_byte == '\n')continue;

                uint32_t input_codepoint = 0;
                if(decode_utf8_codepoint(first_byte,&input_codepoint) != 0)continue;

                myfont_result set_result = myfont_glyph_set_codepoint(
                        font,glyph,input_codepoint);
                if(set_result != MYFONT_SUCCESS){
                        printf(
                                "U+%04X: %s\n",
                                (unsigned int)input_codepoint,
                                myfont_result_string(set_result));
                        continue;
                }

                struct glyph_bitmap bitmap = {0};
                if(myfont_glyph_build_bitmap(glyph,&bitmap) != MYFONT_SUCCESS){
                        printf(
                                "U+%04X: bitmap生成に失敗しました\n",
                                (unsigned int)input_codepoint);
                        continue;
                }

                printf(
                        "\nU+%04X (%dx%d)\n",
                        (unsigned int)input_codepoint,
                        bitmap.width,
                        bitmap.height);
                print_glyph_bitmap(&bitmap);
                free_glyph_bitmap(&bitmap);
        }

        myfont_glyph_destroy(glyph);
        myfont_close(font);
        return exit_code;
}
