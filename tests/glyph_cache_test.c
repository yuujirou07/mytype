#include <stdint.h>
#include <stdio.h>

#include "myfont.h"

static int expect_character(
        const myfont_glyph *glyph,
        uint32_t expected_codepoint,
        size_t expected_cache_count){

        uint32_t actual_codepoint = myfont_glyph_codepoint(glyph);
        size_t actual_cache_count = myfont_glyph_cached_count(glyph);
        if(actual_codepoint == expected_codepoint &&
                actual_cache_count == expected_cache_count)return 0;

        fprintf(
                stderr,
                "expected U+%04X/cache=%zu, got U+%04X/cache=%zu\n",
                (unsigned int)expected_codepoint,
                expected_cache_count,
                (unsigned int)actual_codepoint,
                actual_cache_count);
        return -1;
}

int main(int argc,char **argv){
        if(argc != 2){
                fprintf(stderr,"usage: glyph_cache_test FONT_FILE\n");
                return 2;
        }

        myfont_font *font = NULL;
        myfont_glyph *glyph = NULL;
        myfont_result result = myfont_open(argv[1],&font);
        if(result != MYFONT_SUCCESS)return 3;

        result = myfont_load_glyph(font,0x0041,&glyph);
        if(result != MYFONT_SUCCESS || expect_character(glyph,0x0041,1) != 0){
                myfont_close(font);
                return 4;
        }

        result = myfont_glyph_set_codepoint(font,glyph,0x3042);
        if(result != MYFONT_SUCCESS || expect_character(glyph,0x3042,2) != 0){
                myfont_glyph_destroy(glyph);
                myfont_close(font);
                return 5;
        }

        /* U+0041 はキャッシュ済みなので、件数を増やさず再利用する。 */
        result = myfont_glyph_set_codepoint(font,glyph,0x0041);
        if(result != MYFONT_SUCCESS || expect_character(glyph,0x0041,2) != 0){
                myfont_glyph_destroy(glyph);
                myfont_close(font);
                return 6;
        }

        /* 未対応入力では、現在表示中のキャッシュを壊さない。 */
        result = myfont_glyph_set_codepoint(font,glyph,0x1F600);
        if(result != MYFONT_ERROR_UNSUPPORTED ||
                expect_character(glyph,0x0041,2) != 0){
                myfont_glyph_destroy(glyph);
                myfont_close(font);
                return 7;
        }

        myfont_glyph_destroy(glyph);
        myfont_close(font);
        return 0;
}
