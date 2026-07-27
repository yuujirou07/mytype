#ifndef FONT_BITMAP_H
#define FONT_BITMAP_H

#include <stdint.h>

#include "font_glyph.h"

/* グリフの輪郭を走査変換した2値ビットマップ。
   bitmap[row][col]は0(輪郭外)か1(輪郭内)。
   row=0が上端(y_max側)、col=0が左端(x_min側)にあたる。
   widthとheightは、characterの境界(x_max-x_min, y_max-y_min)からそのまま
   決まる(1フォント単位=1ピクセル、拡大縮小はしない)。 */
struct glyph_bitmap{
        uint32_t unicode_codepoint;
        int **bitmap;
        int width;
        int height;
};

/* characterの輪郭(単純グリフ・複合グリフのどちらでも、既にcontoursへ
   展開済みのもの)を、nonzero winding ruleで走査変換してout_bitmapへ
   ラスタライズする。輪郭が無い、または境界が縮退している場合は
   width=0, height=0, bitmap=NULLとして成功扱いにする。
   失敗時は-1、成功時は0を返す。 */
int build_glyph_bitmap(
        const struct character_render_data *character,
        struct glyph_bitmap *out_bitmap);

/* build_glyph_bitmapが確保したbitmap配列を解放し、ゼロ初期化に戻す。 */
void free_glyph_bitmap(struct glyph_bitmap *bitmap);

#endif
