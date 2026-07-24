#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "raylib.h"

#include "font_func_flags.h"
#include "font_render.h"

static Vector2 point_pos_to_screen(
        const struct point_pos *point,
        int16_t x_min,
        int16_t y_max,
        float scale,
        float offset_x,
        float offset_y){

        Vector2 screen_point;
        screen_point.x = offset_x + (point->pos_x - x_min) * scale;
        screen_point.y = offset_y + (y_max - point->pos_y) * scale;
        return screen_point;
}

static Vector2 get_middle_point(Vector2 point_a,Vector2 point_b){
        Vector2 middle_point;
        middle_point.x = (point_a.x + point_b.x) / 2.0f;
        middle_point.y = (point_a.y + point_b.y) / 2.0f;
        return middle_point;
}

static void draw_quadratic_curve(
        Vector2 start_point,
        Vector2 control_point,
        Vector2 end_point){

        const int split_count = 24;
        Vector2 before_point = start_point;

        for(int i = 1;i <= split_count;i++){
                float t = (float)i / split_count;
                float reverse_t = 1.0f - t;
                Vector2 draw_point;

                draw_point.x =
                        reverse_t * reverse_t * start_point.x +
                        2.0f * reverse_t * t * control_point.x +
                        t * t * end_point.x;
                draw_point.y =
                        reverse_t * reverse_t * start_point.y +
                        2.0f * reverse_t * t * control_point.y +
                        t * t * end_point.y;

                DrawLineEx(before_point,draw_point,3.0f,BLACK);
                before_point = draw_point;
        }
}




static void draw_one_countour(
        const struct contour_pos_data *countour,
        int16_t x_min,
        int16_t y_max,
        float scale,
        float offset_x,
        float offset_y){

        if(countour == NULL || countour->point_pos_data == NULL ||
                countour->point_pos_data_arry_size == 0)return;

        size_t point_count = countour->point_pos_data_arry_size;
        const struct point_pos *first_point = &countour->point_pos_data[0];
        const struct point_pos *last_point =
                &countour->point_pos_data[point_count - 1];

        Vector2 start_point;
        size_t point_num;

        if(first_point->flags & ON_CURVE_POINT){
                start_point = point_pos_to_screen(
                        first_point,x_min,y_max,scale,offset_x,offset_y);
                point_num = 1;
        }else if(last_point->flags & ON_CURVE_POINT){
                start_point = point_pos_to_screen(
                        last_point,x_min,y_max,scale,offset_x,offset_y);
                point_num = 0;
        }else{
                Vector2 first_screen_point = point_pos_to_screen(
                        first_point,x_min,y_max,scale,offset_x,offset_y);
                Vector2 last_screen_point = point_pos_to_screen(
                        last_point,x_min,y_max,scale,offset_x,offset_y);
                start_point = get_middle_point(
                        first_screen_point,last_screen_point);
                point_num = 0;
        }

        Vector2 current_point = start_point;

        while(point_num < point_count){
                const struct point_pos *point =
                        &countour->point_pos_data[point_num];
                Vector2 screen_point = point_pos_to_screen(
                        point,x_min,y_max,scale,offset_x,offset_y);

                if(point->flags & ON_CURVE_POINT){
                        DrawLineEx(current_point,screen_point,3.0f,BLACK);
                        current_point = screen_point;
                        point_num++;
                        continue;
                }

                if(point_num + 1 < point_count){
                        const struct point_pos *next_point =
                                &countour->point_pos_data[point_num + 1];
                        Vector2 next_screen_point = point_pos_to_screen(
                                next_point,x_min,y_max,scale,offset_x,offset_y);

                        if(next_point->flags & ON_CURVE_POINT){
                                draw_quadratic_curve(
                                        current_point,
                                        screen_point,
                                        next_screen_point);
                                current_point = next_screen_point;
                                point_num += 2;
                        }else{
                                Vector2 middle_point = get_middle_point(
                                        screen_point,next_screen_point);
                                draw_quadratic_curve(
                                        current_point,
                                        screen_point,
                                        middle_point);
                                current_point = middle_point;
                                point_num++;
                        }
                }else{
                        draw_quadratic_curve(
                                current_point,
                                screen_point,
                                start_point);
                        current_point = start_point;
                        point_num++;
                }
        }

        DrawLineEx(current_point,start_point,3.0f,BLACK);
}

int show_glyph_with_raylib(
        const struct contour_data *contour_data,
        const struct glyf_table *glyf_table,
        uint16_t units_per_em){

        if(contour_data == NULL || contour_data->pos_data == NULL ||
                glyf_table == NULL || units_per_em == 0)return -1;

        const int screen_width = 900;
        const int screen_height = 900;
        const float font_pixel_size = 650.0f;
        float scale = font_pixel_size / units_per_em;
        float glyph_width = (glyf_table->x_max - glyf_table->x_min) * scale;
        float glyph_height = (glyf_table->y_max - glyf_table->y_min) * scale;
        float offset_x = (screen_width - glyph_width) / 2.0f;
        float offset_y = (screen_height - glyph_height) / 2.0f;

        InitWindow(screen_width,screen_height,"mytype - glyph A");
        if(!IsWindowReady()){
                printf("raylib window create error\n");
                return -1;
        }

        SetTargetFPS(60);
        bool screenshot_saved = false;
        int draw_frame_count = 0;

        while(!WindowShouldClose()){
                BeginDrawing();
                ClearBackground(RAYWHITE);

                for(size_t i = 0;i < contour_data->pos_data_arry_size;i++){
                        draw_one_countour(
                                &contour_data->pos_data[i],
                                glyf_table->x_min,
                                glyf_table->y_max,
                                scale,
                                offset_x,
                                offset_y);
                }

                DrawText("A - TrueType outline",20,20,24,DARKGRAY);
                DrawText("ESC: close",20,52,18,GRAY);
                EndDrawing();

                draw_frame_count++;
                if(!screenshot_saved && draw_frame_count >= 2){
                        TakeScreenshot("A_outline.png");
                        screenshot_saved = true;
                }
        }

        CloseWindow();
        return 0;
}
