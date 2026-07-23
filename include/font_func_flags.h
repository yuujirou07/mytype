#ifndef FONT_FUNC_FLAGS_H
#define FONT_FUNC_FLAGS_H

/* 点が輪郭線上にある。0の場合は曲線の制御点。 */
#define ON_CURVE_POINT 0x01

/* X座標の差分を1バイトで読み取る。 */
#define X_SHORT_VECTOR 0x02

/* Y座標の差分を1バイトで読み取る。 */
#define Y_SHORT_VECTOR 0x04

/* 次の1バイトに、同じフラグの追加繰り返し回数がある。 */
#define REPEAT_FLAG 0x08

/* X_SHORT_VECTORがある場合は正の差分、ない場合は差分0。 */
#define X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR 0x10

/* Y_SHORT_VECTORがある場合は正の差分、ない場合は差分0。 */
#define Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR 0x20

/* 単純グリフの輪郭が重なる可能性がある。 */
#define OVERLAP_SIMPLE 0x40

/* 予約済みビット。通常は0。 */
#define SIMPLE_GLYPH_RESERVED_FLAG 0x80


#endif
