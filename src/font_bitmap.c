#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font_bitmap.h"
#include "font_func_flags.h"
#include "font_glyph.h"

/* 2次ベジエ曲線を分割する線分の数。font_render.cの描画と同じ値を使う。 */
#define BEZIER_SPLIT_COUNT 24

/* 輪郭を平坦化した後の1点。float座標で、フォント単位系のまま持つ
   (拡大縮小・y反転はしない。それらはピクセルへの変換時に行う)。 */
struct polygon_point{
        float x;
        float y;
};

/* 全輪郭の点を1つの配列にまとめて積み上げるための可変長バッファ。 */
struct polygon{
        struct polygon_point *points;
        size_t count;
        size_t capacity;
};

/* polygon配列中で、1つの輪郭が占める範囲。辺は輪郭ごとに閉じるため、
   この範囲内でだけ次の点へ回り込む。 */
struct contour_range{
        size_t start;
        size_t count;
};

/* 走査線と輪郭の辺との交点。direction は辺がyの増える向きなら+1、
   減る向きなら-1(nonzero winding ruleの巻き数計算に使う)。 */
struct edge_crossing{
        float x;
        int direction;
};


/* polygonの末尾に点を1つ追加する。必要なら配列を2倍に拡張する。 */
static int polygon_push(struct polygon *polygon,float x,float y){
        if(polygon->count == polygon->capacity){
                size_t new_capacity = polygon->capacity == 0
                        ? 16
                        : polygon->capacity * 2;

                struct polygon_point *new_points = realloc(
                        polygon->points,
                        sizeof(*new_points) * new_capacity);
                if(new_points == NULL){
                        printf("glyph_bitmap polygon allocate error\n");
                        return -1;
                }

                polygon->points = new_points;
                polygon->capacity = new_capacity;
        }

        polygon->points[polygon->count].x = x;
        polygon->points[polygon->count].y = y;
        polygon->count++;
        return 0;
}

/* start→control→endの2次ベジエ曲線を、BEZIER_SPLIT_COUNT個の線分に分割して
   polygonへ追加する(startそのものは呼び出し側が既に追加済みの前提)。 */
static int push_quadratic_curve(
        struct polygon *polygon,
        float start_x,float start_y,
        float control_x,float control_y,
        float end_x,float end_y){

        for(int i = 1;i <= BEZIER_SPLIT_COUNT;i++){
                float t = (float)i / BEZIER_SPLIT_COUNT;
                float reverse_t = 1.0f - t;

                float x =
                        reverse_t * reverse_t * start_x +
                        2.0f * reverse_t * t * control_x +
                        t * t * end_x;
                float y =
                        reverse_t * reverse_t * start_y +
                        2.0f * reverse_t * t * control_y +
                        t * t * end_y;

                if(polygon_push(polygon,x,y) != 0)return -1;
        }

        return 0;
}

/* 輪郭1つをポリゴン(直線近似された点列)へ平坦化してpolygonへ追加する。
   オンカーブ/オフカーブ点の扱いはfont_render.cのdraw_one_countourと同じ
   規則(先頭がオンカーブならそこから、そうでなければ末尾かオンカーブ点間の
   中点から始める)に従う。 */
static int flatten_contour(
        const struct contour_pos_data *contour,
        struct polygon *polygon){

        if(contour == NULL || contour->point_pos_data == NULL ||
                contour->point_pos_data_arry_size == 0)return 0;

        size_t point_count = contour->point_pos_data_arry_size;
        const struct point_pos *first_point = &contour->point_pos_data[0];
        const struct point_pos *last_point =
                &contour->point_pos_data[point_count - 1];

        float start_x;
        float start_y;
        size_t point_num;

        if(first_point->flags & ON_CURVE_POINT){
                start_x = first_point->pos_x;
                start_y = first_point->pos_y;
                point_num = 1;
        }else if(last_point->flags & ON_CURVE_POINT){
                start_x = last_point->pos_x;
                start_y = last_point->pos_y;
                point_num = 0;
        }else{
                start_x = (first_point->pos_x + last_point->pos_x) / 2.0f;
                start_y = (first_point->pos_y + last_point->pos_y) / 2.0f;
                point_num = 0;
        }

        if(polygon_push(polygon,start_x,start_y) != 0)return -1;

        float current_x = start_x;
        float current_y = start_y;

        while(point_num < point_count){
                const struct point_pos *point =
                        &contour->point_pos_data[point_num];
                float point_x = point->pos_x;
                float point_y = point->pos_y;

                if(point->flags & ON_CURVE_POINT){
                        if(polygon_push(polygon,point_x,point_y) != 0)return -1;
                        current_x = point_x;
                        current_y = point_y;
                        point_num++;
                        continue;
                }

                if(point_num + 1 < point_count){
                        const struct point_pos *next_point =
                                &contour->point_pos_data[point_num + 1];
                        float next_x = next_point->pos_x;
                        float next_y = next_point->pos_y;

                        if(next_point->flags & ON_CURVE_POINT){
                                if(push_quadratic_curve(
                                        polygon,
                                        current_x,current_y,
                                        point_x,point_y,
                                        next_x,next_y) != 0)return -1;
                                current_x = next_x;
                                current_y = next_y;
                                point_num += 2;
                        }else{
                                float mid_x = (point_x + next_x) / 2.0f;
                                float mid_y = (point_y + next_y) / 2.0f;

                                if(push_quadratic_curve(
                                        polygon,
                                        current_x,current_y,
                                        point_x,point_y,
                                        mid_x,mid_y) != 0)return -1;
                                current_x = mid_x;
                                current_y = mid_y;
                                point_num++;
                        }
                }else{
                        if(push_quadratic_curve(
                                polygon,
                                current_x,current_y,
                                point_x,point_y,
                                start_x,start_y) != 0)return -1;
                        current_x = start_x;
                        current_y = start_y;
                        point_num++;
                }
        }

        return 0;
}

/* qsort用の比較関数。edge_crossingをx昇順に並べる。 */
static int compare_edge_crossing(const void *a,const void *b){
        const struct edge_crossing *edge_a = a;
        const struct edge_crossing *edge_b = b;

        if(edge_a->x < edge_b->x)return -1;
        if(edge_a->x > edge_b->x)return 1;
        return 0;
}

/* height行分のint配列を確保し、全てcalloc(0埋め)する。途中で失敗した場合は
   確保済みの行を解放してNULLを返す。 */
static int **allocate_bitmap_rows(int width,int height){
        int **bitmap = calloc((size_t)height,sizeof(*bitmap));
        if(bitmap == NULL){
                printf("glyph_bitmap bitmap rows allocate error\n");
                return NULL;
        }

        for(int row = 0;row < height;row++){
                bitmap[row] = calloc((size_t)width,sizeof(*bitmap[row]));
                if(bitmap[row] == NULL){
                        printf("glyph_bitmap bitmap row allocate error\n");
                        for(int i = 0;i < row;i++)free(bitmap[i]);
                        free(bitmap);
                        return NULL;
                }
        }

        return bitmap;
}

/* 1走査線(scan_y)について、全輪郭の辺との交点をcrossingsへ集める。
   crossing_countへ書き込んだ件数を返す。 */
static size_t collect_scanline_crossings(
        const struct polygon *polygon,
        const struct contour_range *ranges,
        size_t range_count,
        float scan_y,
        struct edge_crossing *crossings){

        size_t crossing_count = 0;

        for(size_t r = 0;r < range_count;r++){
                const struct polygon_point *points =
                        &polygon->points[ranges[r].start];
                size_t count = ranges[r].count;

                for(size_t j = 0;j < count;j++){
                        const struct polygon_point *p0 = &points[j];
                        const struct polygon_point *p1 =
                                &points[(j + 1) % count];

                        if(p0->y == p1->y)continue;

                        int upward = p1->y > p0->y;
                        float lower_y = upward ? p0->y : p1->y;
                        float upper_y = upward ? p1->y : p0->y;

                        /* [lower_y, upper_y)の半開区間で判定し、頂点をまたぐ
                           走査線での二重カウントを避ける。 */
                        if(scan_y < lower_y || scan_y >= upper_y)continue;

                        float t = (scan_y - p0->y) / (p1->y - p0->y);
                        float x = p0->x + t * (p1->x - p0->x);

                        crossings[crossing_count].x = x;
                        crossings[crossing_count].direction = upward ? 1 : -1;
                        crossing_count++;
                }
        }

        return crossing_count;
}

/* ソート済みcrossingsをnonzero winding ruleで走査し、巻き数が0でない区間の
   画素をbitmap_rowへ1として書き込む。 */
static void fill_scanline_spans(
        int *bitmap_row,
        int width,
        int16_t x_min,
        const struct edge_crossing *crossings,
        size_t crossing_count){

        int winding = 0;

        for(size_t k = 0;k < crossing_count;k++){
                if(winding != 0 && k > 0){
                        float span_start_x = crossings[k - 1].x;
                        float span_end_x = crossings[k].x;

                        /* 画素中心(x_min + col + 0.5)がspan内にあるcolを塗る。 */
                        int start_col =
                                (int)ceilf(span_start_x - (float)x_min - 0.5f);
                        int end_col =
                                (int)ceilf(span_end_x - (float)x_min - 0.5f);

                        if(start_col < 0)start_col = 0;
                        if(end_col > width)end_col = width;

                        for(int col = start_col;col < end_col;col++){
                                bitmap_row[col] = 1;
                        }
                }
                winding += crossings[k].direction;
        }
}

int build_glyph_bitmap(
        const struct character_render_data *character,
        struct glyph_bitmap *out_bitmap){

        if(character == NULL || out_bitmap == NULL)return -1;

        memset(out_bitmap,0,sizeof(*out_bitmap));
        out_bitmap->unicode_codepoint = character->unicode_codepoint;

        int width = (int)character->x_max - (int)character->x_min;
        int height = (int)character->y_max - (int)character->y_min;

        /* 境界が縮退している(空白文字など)場合は、ビットマップ無しの
           成功として返す。 */
        if(width <= 0 || height <= 0)return 0;

        struct polygon polygon = {0};
        struct contour_range *ranges = NULL;
        size_t range_count = 0;
        size_t range_capacity = 0;
        int **bitmap = NULL;
        struct edge_crossing *crossings = NULL;
        int result = -1;

        for(size_t i = 0;i < character->contours.pos_data_arry_size;i++){
                size_t contour_start = polygon.count;

                if(flatten_contour(
                        &character->contours.pos_data[i],
                        &polygon) != 0)goto cleanup;

                size_t contour_point_count = polygon.count - contour_start;
                if(contour_point_count == 0)continue;

                if(range_count == range_capacity){
                        size_t new_capacity = range_capacity == 0
                                ? 8
                                : range_capacity * 2;
                        struct contour_range *new_ranges = realloc(
                                ranges,
                                sizeof(*new_ranges) * new_capacity);
                        if(new_ranges == NULL){
                                printf("glyph_bitmap contour_range allocate error\n");
                                goto cleanup;
                        }

                        ranges = new_ranges;
                        range_capacity = new_capacity;
                }

                ranges[range_count].start = contour_start;
                ranges[range_count].count = contour_point_count;
                range_count++;
        }

        if(polygon.count == 0){
                /* 輪郭を持たないグリフ。ビットマップ無しで成功とする。 */
                result = 0;
                goto cleanup;
        }

        /* 閉じたポリゴンなので、総辺数は総点数と同じ。走査線ごとに
           使い回すための最大件数として確保しておく。 */
        crossings = malloc(sizeof(*crossings) * polygon.count);
        if(crossings == NULL){
                printf("glyph_bitmap crossings allocate error\n");
                goto cleanup;
        }

        bitmap = allocate_bitmap_rows(width,height);
        if(bitmap == NULL)goto cleanup;

        for(int row = 0;row < height;row++){
                /* row=0が上端(y_max側)になるよう、走査線のy座標は
                   y_maxから下向きに数える。 */
                float scan_y = (float)character->y_max - ((float)row + 0.5f);

                size_t crossing_count = collect_scanline_crossings(
                        &polygon,
                        ranges,
                        range_count,
                        scan_y,
                        crossings);

                qsort(
                        crossings,
                        crossing_count,
                        sizeof(*crossings),
                        compare_edge_crossing);

                fill_scanline_spans(
                        bitmap[row],
                        width,
                        character->x_min,
                        crossings,
                        crossing_count);
        }

        out_bitmap->bitmap = bitmap;
        out_bitmap->width = width;
        out_bitmap->height = height;
        bitmap = NULL;
        result = 0;

cleanup:
        if(bitmap != NULL){
                for(int row = 0;row < height;row++)free(bitmap[row]);
                free(bitmap);
        }
        free(crossings);
        free(ranges);
        free(polygon.points);
        return result;
}

void free_glyph_bitmap(struct glyph_bitmap *bitmap){
        if(bitmap == NULL)return;

        if(bitmap->bitmap != NULL){
                for(int row = 0;row < bitmap->height;row++){
                        free(bitmap->bitmap[row]);
                }
                free(bitmap->bitmap);
        }

        memset(bitmap,0,sizeof(*bitmap));
}
