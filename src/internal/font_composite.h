#ifndef FONT_COMPOSITE_H
#define FONT_COMPOSITE_H

#include <stddef.h>
#include <stdint.h>

#include "font_glyph.h"
#include "font_loca.h"
#include "font_table.h"

/* 複合グリフのコンポーネントフラグ。単純グリフの点フラグとは別物なので、
   font_func_flags.h ではなくこちらに置く。 */
#define ARG_1_AND_2_ARE_WORDS     0x0001
#define ARGS_ARE_XY_VALUES        0x0002
#define ROUND_XY_TO_GRID          0x0004
#define WE_HAVE_A_SCALE           0x0008
#define MORE_COMPONENTS           0x0020
#define WE_HAVE_AN_X_AND_Y_SCALE  0x0040
#define WE_HAVE_A_TWO_BY_TWO      0x0080
#define WE_HAVE_INSTRUCTIONS      0x0100
#define USE_MY_METRICS            0x0200
#define OVERLAP_COMPOUND          0x0400
#define SCALED_COMPONENT_OFFSET   0x0800
#define UNSCALED_COMPONENT_OFFSET 0x1000

/* 入れ子の深さと規模の上限。壊れたフォントで暴走しないようにする。 */
#define COMPOSITE_MAX_DEPTH          8
#define COMPOSITE_MAX_COMPONENTS     256
#define COMPOSITE_MAX_TOTAL_CONTOURS 4096
#define COMPOSITE_MAX_TOTAL_POINTS   100000

enum glyph_outline_result{
        GLYPH_OUTLINE_OK = 0,
        GLYPH_OUTLINE_ERROR_INVALID = -1,
        GLYPH_OUTLINE_ERROR_UNSUPPORTED = -2,
};

/* 単純グリフと複合グリフのどちらでも、描画用の輪郭と境界を作る。
   characterのcontoursとx_min..y_maxだけを書き換える。
   contoursが未使用（pos_dataがNULL）であることを前提とする。 */
int load_glyph_outline(
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t glyph_id,
        struct character_render_data *character);

#endif
