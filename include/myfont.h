#ifndef MYFONT_H
#define MYFONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 内部データを隠す不透明ハンドル。利用者はメンバへ直接アクセスしない。 */
typedef struct myfont_font myfont_font;
typedef struct myfont_glyph myfont_glyph;

typedef enum myfont_result{
        MYFONT_SUCCESS = 0,
        MYFONT_ERROR_INVALID_ARGUMENT = -1,
        MYFONT_ERROR_ALLOCATION = -2,
        MYFONT_ERROR_IO = -3,
        MYFONT_ERROR_INVALID_FONT = -4,
        MYFONT_ERROR_UNSUPPORTED = -5,
        MYFONT_ERROR_NOT_FOUND = -6,
        MYFONT_ERROR_NO_OUTLINE = -7,
} myfont_result;

/* フォントを開閉する。成功時だけ*out_fontへハンドルを返す。 */
myfont_result myfont_open(const char *ttf_path,myfont_font **out_font);
void myfont_close(myfont_font *font);

/* Unicode BMP文字を読み込む。現在はTrueType単純グリフとcmap format 4に対応。 */
myfont_result myfont_load_glyph(
        myfont_font *font,
        uint32_t codepoint,
        myfont_glyph **out_glyph);
myfont_result myfont_glyph_set_codepoint(
        myfont_font *font,
        myfont_glyph *glyph,
        uint32_t codepoint);
void myfont_glyph_destroy(myfont_glyph *glyph);

/* 内部配列を公開せず、必要な値だけ取得する。 */
uint16_t myfont_units_per_em(const myfont_font *font);
uint32_t myfont_glyph_codepoint(const myfont_glyph *glyph);
uint16_t myfont_glyph_id(const myfont_glyph *glyph);
size_t myfont_glyph_cached_count(const myfont_glyph *glyph);
size_t myfont_glyph_contour_count(const myfont_glyph *glyph);
size_t myfont_glyph_point_count(
        const myfont_glyph *glyph,
        size_t contour_index);
myfont_result myfont_glyph_get_bounds(
        const myfont_glyph *glyph,
        int16_t *x_min,
        int16_t *y_min,
        int16_t *x_max,
        int16_t *y_max);
myfont_result myfont_glyph_get_point(
        const myfont_glyph *glyph,
        size_t contour_index,
        size_t point_index,
        int16_t *x,
        int16_t *y,
        int *on_curve);

/* 既存のRaylib描画を不透明ハンドル経由で利用する。 */
myfont_result myfont_show_glyph(
        myfont_font *font,
        myfont_glyph *glyph);

const char *myfont_result_string(myfont_result result);

#ifdef __cplusplus
}
#endif

#endif
