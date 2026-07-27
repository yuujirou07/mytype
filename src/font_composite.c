#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "font_binary.h"
#include "font_composite.h"
#include "font_glyph.h"
#include "font_loca.h"
#include "font_table.h"

/* numberOfContoursとxMin..yMaxを合わせた、グリフ先頭の固定長。 */
#define GLYPH_HEADER_SIZE 10

/* 2x2行列と平行移動。点は (x,y) * [[a,b],[c,d]] + (dx,dy) へ移る。 */
struct component_transform{
        float a;
        float b;
        float c;
        float d;
        float dx;
        float dy;
};

/* 輪郭を追記で積み上げる。point_pos_dataの所有権はbuilderへ移る。 */
struct contour_builder{
        struct contour_pos_data *pos_data;
        size_t count;
        size_t capacity;
        size_t point_total;
};

struct glyph_header{
        int16_t number_of_contours;
        int16_t x_min;
        int16_t y_min;
        int16_t x_max;
        int16_t y_max;
};

static const struct component_transform identity_transform = {
        1.0f,0.0f,0.0f,1.0f,0.0f,0.0f};

/* 単純/複合を問わずglyph_idを1つ展開してbuilderへ積む。複合グリフの
   コンポーネントから子グリフを読む際に再帰呼び出しされるための前方宣言。 */
static int append_glyph(
        struct contour_builder *builder,
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t glyph_id,
        const struct component_transform *transform,
        int depth);


/* 32bit値をint16_tの範囲に丸め込む(飽和)。 */
static int16_t clamp_to_int16(int32_t value){
        if(value > INT16_MAX)return INT16_MAX;
        if(value < INT16_MIN)return INT16_MIN;
        return (int16_t)value;
}

/* floatを四捨五入してint16_tへ変換する(飽和付き)。座標変換後の丸めに使う。 */
static int16_t round_to_int16(float value){
        float rounded = value >= 0.0f ? value + 0.5f : value - 0.5f;

        if(rounded > (float)INT16_MAX)return INT16_MAX;
        if(rounded < (float)INT16_MIN)return INT16_MIN;
        return (int16_t)rounded;
}

/* F2Dot14は符号付き16bitの固定小数点。1.0が0x4000にあたる。 */
static float read_f2dot14(const uint8_t data[2]){
        return (float)(int16_t)read_be16(data) / 16384.0f;
}

/* 1点(source_x, source_y)にtransformの2x2行列と平行移動を適用し、
   丸めてdest_x/dest_yへ書き出す。 */
static void apply_transform(
        const struct component_transform *transform,
        int16_t source_x,
        int16_t source_y,
        int16_t *dest_x,
        int16_t *dest_y){

        float x = (float)source_x;
        float y = (float)source_y;

        *dest_x = round_to_int16(
                x * transform->a + y * transform->c + transform->dx);
        *dest_y = round_to_int16(
                x * transform->b + y * transform->d + transform->dy);
}

/* childを先に適用し、その結果へparentを適用する合成。 */
static struct component_transform compose_transform(
        const struct component_transform *child,
        const struct component_transform *parent){

        struct component_transform composed;

        composed.a = child->a * parent->a + child->b * parent->c;
        composed.b = child->a * parent->b + child->b * parent->d;
        composed.c = child->c * parent->a + child->d * parent->c;
        composed.d = child->c * parent->b + child->d * parent->d;
        composed.dx = child->dx * parent->a + child->dy * parent->c + parent->dx;
        composed.dy = child->dx * parent->b + child->dy * parent->d + parent->dy;
        return composed;
}


/* contour_builderをゼロ初期化する。 */
static void contour_builder_init(struct contour_builder *builder){
        memset(builder,0,sizeof(*builder));
}

/* builderが保持する輪郭と点配列を全て解放し、ゼロ初期化に戻す。 */
static void contour_builder_free(struct contour_builder *builder){
        if(builder == NULL)return;

        for(size_t i = 0;i < builder->count;i++){
                free(builder->pos_data[i].point_pos_data);
        }
        free(builder->pos_data);
        memset(builder,0,sizeof(*builder));
}

/* 確保済みの点配列をそのまま受け取り、所有権をbuilderへ移す。 */
static int contour_builder_push(
        struct contour_builder *builder,
        struct point_pos *point_pos_data,
        size_t point_count){

        if(builder->count == builder->capacity){
                size_t new_capacity = builder->capacity == 0
                        ? 8
                        : builder->capacity * 2;

                if(new_capacity > COMPOSITE_MAX_TOTAL_CONTOURS){
                        new_capacity = COMPOSITE_MAX_TOTAL_CONTOURS;
                }
                if(new_capacity <= builder->capacity){
                        printf("composite contour count is too large\n");
                        return -1;
                }

                struct contour_pos_data *new_pos_data = realloc(
                        builder->pos_data,
                        sizeof(*new_pos_data) * new_capacity);
                if(new_pos_data == NULL){
                        printf("contour_builder pos_data allocate error\n");
                        return -1;
                }

                builder->pos_data = new_pos_data;
                builder->capacity = new_capacity;
        }

        builder->pos_data[builder->count].point_pos_data = point_pos_data;
        builder->pos_data[builder->count].point_pos_data_arry_size = point_count;
        builder->count++;
        builder->point_total += point_count;
        return 0;
}

/* sourceの輪郭を平行移動しながらdestinationへ移す。成否によらずsourceは空になる。 */
static int contour_builder_merge(
        struct contour_builder *destination,
        struct contour_builder *source,
        int16_t offset_x,
        int16_t offset_y){

        for(size_t i = 0;i < source->count;i++){
                struct contour_pos_data *contour = &source->pos_data[i];

                if(contour->point_pos_data_arry_size == 0)continue;

                for(size_t j = 0;j < contour->point_pos_data_arry_size;j++){
                        struct point_pos *point = &contour->point_pos_data[j];

                        point->pos_x = clamp_to_int16(
                                (int32_t)point->pos_x + offset_x);
                        point->pos_y = clamp_to_int16(
                                (int32_t)point->pos_y + offset_y);
                }

                if(contour_builder_push(
                        destination,
                        contour->point_pos_data,
                        contour->point_pos_data_arry_size) != 0){
                        return -1;
                }

                /* 所有権を移したので、二重解放を避ける。 */
                contour->point_pos_data = NULL;
                contour->point_pos_data_arry_size = 0;
        }

        return 0;
}

/* 輪郭をまたいだ通し番号で点を探す。点番号による位置合わせで使う。 */
static const struct point_pos *find_point_by_index(
        const struct contour_builder *builder,
        size_t point_index){

        size_t seen_point_count = 0;

        for(size_t i = 0;i < builder->count;i++){
                size_t contour_size = builder->pos_data[i].point_pos_data_arry_size;

                if(point_index < seen_point_count + contour_size){
                        return &builder->pos_data[i]
                                .point_pos_data[point_index - seen_point_count];
                }
                seen_point_count += contour_size;
        }

        return NULL;
}


/* parse_glyph_data_tableが確保したglyf_table内の配列を全て解放する。
   font_glyph.c内のfree処理と同じ内容を、この呼び出し側からも使えるように
   複製したもの。 */
static void free_parsed_glyf_table(struct glyf_table *glyf_table){
        if(glyf_table == NULL)return;

        free(glyf_table->end_pts_of_contours);
        free(glyf_table->instructions);
        free(glyf_table->flags);
        free(glyf_table->points);
        memset(glyf_table,0,sizeof(*glyf_table));
}

/* glyph_idのloca位置を求め、glyph_count・glyf_recordの長さと突き合わせて
   妥当性を確認する。範囲外・逆転・オーバーフローのいずれかがあれば-1。 */
static int get_glyph_data_range(
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t glyph_id,
        uint32_t *glyph_start,
        uint32_t *glyph_end){

        if(glyph_id >= glyph_count)return -1;
        if(get_glyph_loca_pos(loca,glyph_id,glyph_start,glyph_end) != 0)return -1;
        if(*glyph_start > *glyph_end)return -1;
        if(*glyph_end > glyf_record->length)return -1;
        if(*glyph_start > UINT32_MAX - glyf_record->offcet)return -1;
        return 0;
}

/* glyph_file_positionからGLYPH_HEADER_SIZE分だけ読み、number_of_contoursと
   境界値をheaderへ格納する。number_of_contours<0なら複合グリフの印。 */
static int read_glyph_header(
        int ttf_fd,
        uint32_t glyph_file_position,
        struct glyph_header *header){

        if(lseek(ttf_fd,glyph_file_position,SEEK_SET) < 0){
                printf("glyph header lseek error\n");
                return -1;
        }

        uint8_t header_data[GLYPH_HEADER_SIZE];
        if(read_exact(ttf_fd,header_data,sizeof(header_data)) != 0){
                printf("glyph header read error\n");
                return -1;
        }

        header->number_of_contours = (int16_t)read_be16(header_data);
        header->x_min = (int16_t)read_be16(header_data + 2);
        header->y_min = (int16_t)read_be16(header_data + 4);
        header->x_max = (int16_t)read_be16(header_data + 6);
        header->y_max = (int16_t)read_be16(header_data + 8);
        return 0;
}

/* コンポーネント定義を、グリフのデータ範囲内に収まる範囲で順に読む。
   子グリフを再帰で読むとファイル位置が動くため、毎回lseekし直す。 */
static int read_component_bytes(
        int ttf_fd,
        uint32_t *read_position,
        uint32_t end_position,
        uint8_t *data,
        uint32_t size){

        if(size > end_position - *read_position){
                printf("composite component data is out of range\n");
                return -1;
        }
        if(lseek(ttf_fd,*read_position,SEEK_SET) < 0){
                printf("composite component lseek error\n");
                return -1;
        }
        if(read_exact(ttf_fd,data,size) != 0){
                printf("composite component read error\n");
                return -1;
        }

        *read_position += size;
        return 0;
}


/* 単純グリフを既存の解析経路で読み、変換をかけてbuilderへ積む。 */
static int append_simple_glyph(
        struct contour_builder *builder,
        int ttf_fd,
        uint32_t glyph_file_position,
        const struct component_transform *transform){

        struct glyf_table parsed_glyph = {0};
        struct contour_data contours = {0};
        int result = -1;

        if(parse_glyph_data_table(
                ttf_fd,
                glyph_file_position,
                &parsed_glyph) != 0){
                goto cleanup;
        }
        if(get_countour_data(&parsed_glyph,&contours) != 0){
                goto cleanup;
        }

        for(size_t i = 0;i < contours.pos_data_arry_size;i++){
                struct contour_pos_data *contour = &contours.pos_data[i];

                if(contour->point_pos_data_arry_size == 0)continue;

                for(size_t j = 0;j < contour->point_pos_data_arry_size;j++){
                        struct point_pos *point = &contour->point_pos_data[j];

                        apply_transform(
                                transform,
                                point->pos_x,
                                point->pos_y,
                                &point->pos_x,
                                &point->pos_y);
                }

                if(contour_builder_push(
                        builder,
                        contour->point_pos_data,
                        contour->point_pos_data_arry_size) != 0){
                        goto cleanup;
                }

                /* 所有権を移したので、二重解放を避ける。 */
                contour->point_pos_data = NULL;
                contour->point_pos_data_arry_size = 0;
        }

        result = 0;

cleanup:
        free_countour_data(&contours);
        free_parsed_glyf_table(&parsed_glyph);
        return result;
}

/* ARGS_ARE_XY_VALUESがない場合の、点番号による位置合わせ。
   コンポーネントを一度別のbuilderへ組み立ててから、ずれの分だけ移動する。 */
static int append_point_matched_component(
        struct contour_builder *builder,
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t component_glyph_id,
        const struct component_transform *transform,
        size_t parent_point_index,
        size_t component_point_index,
        int depth){

        struct contour_builder component_builder;
        int result = -1;

        contour_builder_init(&component_builder);

        if(append_glyph(
                &component_builder,
                ttf_fd,
                loca,
                glyf_record,
                glyph_count,
                component_glyph_id,
                transform,
                depth + 1) != 0){
                goto cleanup;
        }

        const struct point_pos *parent_point =
                find_point_by_index(builder,parent_point_index);
        const struct point_pos *component_point =
                find_point_by_index(&component_builder,component_point_index);

        if(parent_point == NULL || component_point == NULL){
                printf("composite point matching index error\n");
                goto cleanup;
        }

        int16_t offset_x = clamp_to_int16(
                (int32_t)parent_point->pos_x - component_point->pos_x);
        int16_t offset_y = clamp_to_int16(
                (int32_t)parent_point->pos_y - component_point->pos_y);

        /* 移動量を確定させてから統合する。builderの再確保でparent_pointが
           無効になるため、この順序を崩さない。 */
        if(contour_builder_merge(
                builder,
                &component_builder,
                offset_x,
                offset_y) != 0){
                goto cleanup;
        }

        result = 0;

cleanup:
        contour_builder_free(&component_builder);
        return result;
}

/* 複合グリフのコンポーネント列を読み、それぞれをbuilderへ積む。 */
static int append_composite_glyph(
        struct contour_builder *builder,
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint32_t component_position,
        uint32_t component_end_position,
        const struct component_transform *parent_transform,
        int depth){

        uint32_t read_position = component_position;
        int component_count = 0;
        uint16_t flags = 0;

        do{
                if(component_count >= COMPOSITE_MAX_COMPONENTS){
                        printf("composite component count is too large\n");
                        return -1;
                }
                component_count++;

                uint8_t component_data[4];
                if(read_component_bytes(
                        ttf_fd,
                        &read_position,
                        component_end_position,
                        component_data,
                        sizeof(component_data)) != 0){
                        return -1;
                }

                flags = read_be16(component_data);
                uint16_t component_glyph_id = read_be16(component_data + 2);

                int32_t argument1 = 0;
                int32_t argument2 = 0;

                if(flags & ARG_1_AND_2_ARE_WORDS){
                        uint8_t argument_data[4];

                        if(read_component_bytes(
                                ttf_fd,
                                &read_position,
                                component_end_position,
                                argument_data,
                                sizeof(argument_data)) != 0){
                                return -1;
                        }

                        if(flags & ARGS_ARE_XY_VALUES){
                                argument1 = (int16_t)read_be16(argument_data);
                                argument2 = (int16_t)read_be16(argument_data + 2);
                        }else{
                                argument1 = read_be16(argument_data);
                                argument2 = read_be16(argument_data + 2);
                        }
                }else{
                        uint8_t argument_data[2];

                        if(read_component_bytes(
                                ttf_fd,
                                &read_position,
                                component_end_position,
                                argument_data,
                                sizeof(argument_data)) != 0){
                                return -1;
                        }

                        if(flags & ARGS_ARE_XY_VALUES){
                                argument1 = (int8_t)argument_data[0];
                                argument2 = (int8_t)argument_data[1];
                        }else{
                                argument1 = argument_data[0];
                                argument2 = argument_data[1];
                        }
                }

                struct component_transform component = identity_transform;

                if(flags & WE_HAVE_A_SCALE){
                        uint8_t scale_data[2];

                        if(read_component_bytes(
                                ttf_fd,
                                &read_position,
                                component_end_position,
                                scale_data,
                                sizeof(scale_data)) != 0){
                                return -1;
                        }

                        float scale = read_f2dot14(scale_data);
                        component.a = scale;
                        component.d = scale;
                }else if(flags & WE_HAVE_AN_X_AND_Y_SCALE){
                        uint8_t scale_data[4];

                        if(read_component_bytes(
                                ttf_fd,
                                &read_position,
                                component_end_position,
                                scale_data,
                                sizeof(scale_data)) != 0){
                                return -1;
                        }

                        component.a = read_f2dot14(scale_data);
                        component.d = read_f2dot14(scale_data + 2);
                }else if(flags & WE_HAVE_A_TWO_BY_TWO){
                        uint8_t scale_data[8];

                        if(read_component_bytes(
                                ttf_fd,
                                &read_position,
                                component_end_position,
                                scale_data,
                                sizeof(scale_data)) != 0){
                                return -1;
                        }

                        component.a = read_f2dot14(scale_data);
                        component.b = read_f2dot14(scale_data + 2);
                        component.c = read_f2dot14(scale_data + 4);
                        component.d = read_f2dot14(scale_data + 6);
                }

                if(flags & ARGS_ARE_XY_VALUES){
                        float offset_x = (float)argument1;
                        float offset_y = (float)argument2;

                        /* 既定では移動量を変換しない。SCALED_COMPONENT_OFFSETが
                           立っている場合だけ、行列を移動量にも適用する。 */
                        if((flags & SCALED_COMPONENT_OFFSET) &&
                                !(flags & UNSCALED_COMPONENT_OFFSET)){
                                float scaled_x = offset_x * component.a +
                                        offset_y * component.c;
                                float scaled_y = offset_x * component.b +
                                        offset_y * component.d;

                                offset_x = scaled_x;
                                offset_y = scaled_y;
                        }

                        component.dx = offset_x;
                        component.dy = offset_y;

                        struct component_transform child_transform =
                                compose_transform(&component,parent_transform);

                        if(append_glyph(
                                builder,
                                ttf_fd,
                                loca,
                                glyf_record,
                                glyph_count,
                                component_glyph_id,
                                &child_transform,
                                depth + 1) != 0){
                                return -1;
                        }
                }else{
                        struct component_transform child_transform =
                                compose_transform(&component,parent_transform);

                        if(append_point_matched_component(
                                builder,
                                ttf_fd,
                                loca,
                                glyf_record,
                                glyph_count,
                                component_glyph_id,
                                &child_transform,
                                (size_t)argument1,
                                (size_t)argument2,
                                depth) != 0){
                                return -1;
                        }
                }
        }while(flags & MORE_COMPONENTS);

        return 0;
}

/* 単純グリフならそのまま、複合グリフならコンポーネントを展開して積む。 */
static int append_glyph(
        struct contour_builder *builder,
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t glyph_id,
        const struct component_transform *transform,
        int depth){

        if(depth > COMPOSITE_MAX_DEPTH){
                printf("composite glyph nest is too deep\n");
                return -1;
        }
        if(builder->point_total > COMPOSITE_MAX_TOTAL_POINTS){
                printf("composite glyph point count is too large\n");
                return -1;
        }

        uint32_t glyph_start = 0;
        uint32_t glyph_end = 0;

        if(get_glyph_data_range(
                loca,
                glyf_record,
                glyph_count,
                glyph_id,
                &glyph_start,
                &glyph_end) != 0){
                printf("component glyph range error\n");
                return -1;
        }

        /* 空白のように中身を持たないグリフは、輪郭を足さずに成功とする。 */
        if(glyph_start == glyph_end)return 0;
        if(glyph_end - glyph_start < GLYPH_HEADER_SIZE){
                printf("component glyph data is too short\n");
                return -1;
        }

        uint32_t glyph_file_position = glyf_record->offcet + glyph_start;
        struct glyph_header header;

        if(read_glyph_header(ttf_fd,glyph_file_position,&header) != 0)return -1;

        if(header.number_of_contours >= 0){
                return append_simple_glyph(
                        builder,
                        ttf_fd,
                        glyph_file_position,
                        transform);
        }

        return append_composite_glyph(
                builder,
                ttf_fd,
                loca,
                glyf_record,
                glyph_count,
                glyph_file_position + GLYPH_HEADER_SIZE,
                glyf_record->offcet + glyph_end,
                transform,
                depth);
}

/* ヘッダの境界が使えるならそれを、壊れているなら実際の点から計算する。 */
static void set_composite_bounds(
        struct character_render_data *character,
        const struct glyph_header *header){

        if(header->x_min < header->x_max && header->y_min < header->y_max){
                character->x_min = header->x_min;
                character->y_min = header->y_min;
                character->x_max = header->x_max;
                character->y_max = header->y_max;
                return;
        }

        int16_t x_min = 0;
        int16_t y_min = 0;
        int16_t x_max = 0;
        int16_t y_max = 0;
        int found_point = 0;

        for(size_t i = 0;i < character->contours.pos_data_arry_size;i++){
                const struct contour_pos_data *contour =
                        &character->contours.pos_data[i];

                for(size_t j = 0;j < contour->point_pos_data_arry_size;j++){
                        int16_t x = contour->point_pos_data[j].pos_x;
                        int16_t y = contour->point_pos_data[j].pos_y;

                        if(found_point == 0){
                                x_min = x;
                                x_max = x;
                                y_min = y;
                                y_max = y;
                                found_point = 1;
                                continue;
                        }

                        if(x < x_min)x_min = x;
                        if(x > x_max)x_max = x;
                        if(y < y_min)y_min = y;
                        if(y > y_max)y_max = y;
                }
        }

        character->x_min = x_min;
        character->y_min = y_min;
        character->x_max = x_max;
        character->y_max = y_max;
}


/* myfont.cから呼ばれる公開入口。glyph_idが単純グリフなら既存の解析経路、
   複合グリフならコンポーネントを展開して、どちらの場合もcharacterの
   contoursとx_min..y_maxを書き込む。character->contoursは未使用である
   ことを前提とする(呼び出し側で二重初期化しないこと)。 */
int load_glyph_outline(
        int ttf_fd,
        const struct loca_table_data_wrapper *loca,
        const struct table_record *glyf_record,
        uint16_t glyph_count,
        uint16_t glyph_id,
        struct character_render_data *character){

        if(loca == NULL || glyf_record == NULL || character == NULL){
                return GLYPH_OUTLINE_ERROR_INVALID;
        }
        if(character->contours.pos_data != NULL){
                return GLYPH_OUTLINE_ERROR_INVALID;
        }

        uint32_t glyph_start = 0;
        uint32_t glyph_end = 0;

        if(get_glyph_data_range(
                loca,
                glyf_record,
                glyph_count,
                glyph_id,
                &glyph_start,
                &glyph_end) != 0){
                return GLYPH_OUTLINE_ERROR_INVALID;
        }
        if(glyph_end - glyph_start < GLYPH_HEADER_SIZE){
                return GLYPH_OUTLINE_ERROR_INVALID;
        }

        uint32_t glyph_file_position = glyf_record->offcet + glyph_start;
        struct glyph_header header;

        if(read_glyph_header(ttf_fd,glyph_file_position,&header) != 0){
                return GLYPH_OUTLINE_ERROR_INVALID;
        }

        /* 単純グリフは従来どおりの経路をそのまま使う。 */
        if(header.number_of_contours >= 0){
                struct glyf_table parsed_glyph = {0};

                if(parse_glyph_data_table(
                        ttf_fd,
                        glyph_file_position,
                        &parsed_glyph) != 0){
                        free_parsed_glyf_table(&parsed_glyph);
                        return GLYPH_OUTLINE_ERROR_UNSUPPORTED;
                }

                character->x_min = parsed_glyph.x_min;
                character->y_min = parsed_glyph.y_min;
                character->x_max = parsed_glyph.x_max;
                character->y_max = parsed_glyph.y_max;

                if(get_countour_data(&parsed_glyph,&character->contours) != 0){
                        free_parsed_glyf_table(&parsed_glyph);
                        return GLYPH_OUTLINE_ERROR_INVALID;
                }

                free_parsed_glyf_table(&parsed_glyph);
                return GLYPH_OUTLINE_OK;
        }

        struct contour_builder builder;
        contour_builder_init(&builder);

        if(append_composite_glyph(
                &builder,
                ttf_fd,
                loca,
                glyf_record,
                glyph_count,
                glyph_file_position + GLYPH_HEADER_SIZE,
                glyf_record->offcet + glyph_end,
                &identity_transform,
                0) != 0){
                contour_builder_free(&builder);
                return GLYPH_OUTLINE_ERROR_INVALID;
        }

        /* pos_data_arry_sizeはuint16_tなので、その範囲に収まることを確かめる。 */
        if(builder.count > UINT16_MAX){
                printf("composite contour count does not fit\n");
                contour_builder_free(&builder);
                return GLYPH_OUTLINE_ERROR_INVALID;
        }

        character->contours.pos_data = builder.pos_data;
        character->contours.pos_data_arry_size = (uint16_t)builder.count;
        builder.pos_data = NULL;
        builder.count = 0;
        contour_builder_free(&builder);

        set_composite_bounds(character,&header);
        return GLYPH_OUTLINE_OK;
}
