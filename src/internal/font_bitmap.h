#ifndef FONT_BITMAP_H
#define FONT_BITMAP_H

#include <stdint.h>

#include "myfont.h"
#include "font_glyph.h"

/* characterの輪郭(単純グリフ・複合グリフのどちらでも、既にcontoursへ
   展開済みのもの)を、nonzero winding ruleで走査変換してout_bitmapへ
   ラスタライズする。輪郭が無い、または境界が縮退している場合は
   width=0, height=0, bitmap=NULLとして成功扱いにする。
   失敗時は-1、成功時は0を返す。 */
int build_glyph_bitmap(
        const struct character_render_data *character,
        glyph_bitmap *out_bitmap);

#endif
