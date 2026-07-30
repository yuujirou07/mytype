# mytype: フォントのビットマップを取得する

`mytype`ライブラリを使って、TrueTypeフォント(.ttf)から1文字分の
ビットマップ(白黒の画素データ)を取得するまでの手順を説明する。

使うヘッダは2つ:

```c
#include "myfont.h"       /* 公開API */
#include "font_bitmap.h"  /* struct glyph_bitmap の中身を使うために必要 */
```

## 手順

### 1. フォントファイルを開く

```c
myfont_font *font = NULL;
myfont_result result = myfont_open("font_file/IBMPlexSansJP-Thin.ttf", &font);
if(result != MYFONT_SUCCESS){
        fprintf(stderr, "open failed: %s\n", myfont_result_string(result));
        return 1;
}
```

`myfont_open`は成功時のみ`*out_font`にハンドルを設定する。使い終わったら
必ず`myfont_close(font)`で閉じる。

### 2. 文字を1つ読み込む

Unicodeコードポイントを指定してグリフ(文字の形状データ)を読み込む。

```c
myfont_glyph *glyph = NULL;
result = myfont_load_glyph(font, 0x3042 /* 'あ' */, &glyph);
if(result != MYFONT_SUCCESS){
        fprintf(stderr, "load_glyph failed: %s\n", myfont_result_string(result));
        myfont_close(font);
        return 1;
}
```

同じ`glyph`ハンドルのまま別の文字に切り替えたい場合は、
`myfont_load_glyph`をもう一度呼ぶ代わりに以下を使うと、
既に読み込んだ文字は再利用され高速に切り替わる。

```c
result = myfont_glyph_set_codepoint(font, glyph, 0x3044 /* 'い' */);
```

### 3. ビットマップを作る

```c
struct glyph_bitmap bitmap = {0};
result = myfont_glyph_build_bitmap(glyph, &bitmap);
if(result != MYFONT_SUCCESS){
        fprintf(stderr, "build_bitmap failed: %s\n", myfont_result_string(result));
}
```

`struct glyph_bitmap`の中身(`font_bitmap.h`で定義):

```c
struct glyph_bitmap{
        uint32_t unicode_codepoint;
        int **bitmap;   /* bitmap[row][col]: 0=文字の外, 1=文字の内側 */
        int width;
        int height;
};
```

- `bitmap[row][col]`が`1`ならその画素は文字の内側(黒く塗る画素)。
- `row = 0`が上端、`col = 0`が左端。
- サイズは`width × height`で、拡大縮小は行われない
  (フォント自体の座標単位がそのままピクセル数になる)。
- 空白文字など何も描画するものが無い場合は
  `width = 0, height = 0, bitmap = NULL`になる(これはエラーではない)。

### 4. ビットマップを使う

例えば画素を走査して描画・保存するには次のようにする。

```c
for(int row = 0; row < bitmap.height; row++){
        for(int col = 0; col < bitmap.width; col++){
                if(bitmap.bitmap[row][col]){
                        /* この画素を黒く塗る、など */
                }
        }
}
```

### 5. 後片付け

```c
free_glyph_bitmap(&bitmap);   /* ビットマップごとに */
myfont_glyph_destroy(glyph);  /* 文字の読み込みが不要になったら */
myfont_close(font);           /* フォント全体が不要になったら */
```

## 補足で使える情報

ビットマップ以外にも、現在選択中の文字について以下を取得できる。

```c
uint16_t units_per_em = myfont_units_per_em(font);       /* フォントの基準スケール */
uint32_t codepoint    = myfont_glyph_codepoint(glyph);    /* 現在の文字のUnicode値 */
uint16_t glyph_id     = myfont_glyph_id(glyph);            /* フォント内部のグリフID */

int16_t x_min, y_min, x_max, y_max;
myfont_glyph_get_bounds(glyph, &x_min, &y_min, &x_max, &y_max); /* 文字の境界 */
```

## エラーコード

`myfont_result`は0(`MYFONT_SUCCESS`)以外は全てエラー。
`myfont_result_string(result)`で人が読めるメッセージに変換できる。
特に押さえておくとよいもの:

| エラー | 意味 |
|---|---|
| `MYFONT_ERROR_NOT_FOUND` | 指定したコードポイントがこのフォントに無い |
| `MYFONT_ERROR_NO_OUTLINE` | その文字は輪郭を持たない(空白など) — `myfont_load_glyph`の時点で発生 |
| `MYFONT_ERROR_IO` | ファイルが開けない・読み込めない |
| `MYFONT_ERROR_INVALID_FONT` | フォントファイルの内容が壊れている・非対応 |

## 最小サンプル全体

```c
#include <stdio.h>
#include "myfont.h"
#include "font_bitmap.h"

int main(void){
        myfont_font *font = NULL;
        if(myfont_open("font_file/IBMPlexSansJP-Thin.ttf", &font) != MYFONT_SUCCESS){
                return 1;
        }

        myfont_glyph *glyph = NULL;
        if(myfont_load_glyph(font, 0x3042, &glyph) != MYFONT_SUCCESS){
                myfont_close(font);
                return 1;
        }

        struct glyph_bitmap bitmap = {0};
        if(myfont_glyph_build_bitmap(glyph, &bitmap) == MYFONT_SUCCESS){
                printf("%dx%d\n", bitmap.width, bitmap.height);
                free_glyph_bitmap(&bitmap);
        }

        myfont_glyph_destroy(glyph);
        myfont_close(font);
        return 0;
}
```

より実践的な例は`src/font_func.c`の`main()`を参照
(標準入力から文字を読み、ビットマップをASCIIアートで表示している)。
