#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myfont.h"

static int report_error(const char *operation,myfont_result result){
        fprintf(stderr,"%s: %s\n",operation,myfont_result_string(result));
        return 1;
}

int main(int argc,char **argv){
        const char *font_path = argc > 1
                ? argv[1]
                : "font_file/IBMPlexSansJP-Thin.ttf";

        int key = 'i';

        uint32_t codepoint = argc > 2
                ? (uint32_t)strtoul(argv[2],NULL,0)
                : (uint32_t)key;

        int show_window = !(argc > 3 && strcmp(argv[3],"--no-window") == 0);

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
        if(show_window){
                result = myfont_show_glyph(font,glyph);
                if(result != MYFONT_SUCCESS){
                        exit_code = report_error("myfont_show_glyph",result);
                }
        }

        myfont_glyph_destroy(glyph);
        myfont_close(font);
        return exit_code;
}
