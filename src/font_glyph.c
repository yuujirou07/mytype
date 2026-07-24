#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "font_binary.h"
#include "font_func_flags.h"
#include "font_glyph.h"

int parse_glyph_data_table(long ttf_fd,uint32_t glyf_file_table_offset,struct glyf_table *glyf_table){
        if(glyf_table == NULL){
                printf("glyf table is  NULL\n");
                return -1;
        }

        if(lseek(ttf_fd,glyf_file_table_offset,SEEK_SET) < 0){
                printf("glyf table lseek error\n");
                return -1;
        }

        int8_t ui8_data_storage[10];
        if(read_exact(ttf_fd,(uint8_t *)ui8_data_storage,sizeof(ui8_data_storage)) != 0){
                printf("glyf table parse error\n");
                return -1;
        }

        int16_t i16_data = (int16_t)read_be16((uint8_t *)ui8_data_storage);
        glyf_table->number_of_contours = i16_data;


        if(glyf_table->number_of_contours < 0 ){
                printf("this font shape is not support\n");
                return -1;
        }

        i16_data = (int16_t)read_be16((uint8_t *)ui8_data_storage + 2);
        glyf_table->x_min = i16_data;

        i16_data = (int16_t)read_be16((uint8_t *)ui8_data_storage + 4);
        glyf_table->y_min = i16_data;

        i16_data = (int16_t)read_be16((uint8_t *)ui8_data_storage + 6);
        glyf_table->x_max = i16_data;

        i16_data = (int16_t)read_be16((uint8_t *)ui8_data_storage + 8);
        glyf_table->y_max = i16_data;


        uint8_t ui8_2byte_data_storage[2];
        if(glyf_table->number_of_contours > 0){
                uint16_t end_pos_countor = glyf_table->number_of_contours;
                glyf_table->end_pts_of_contours = malloc(sizeof(uint16_t) * end_pos_countor);
                if(glyf_table->end_pts_of_contours == NULL){
                        printf("glyf_table->end_pts_of_contours allocate error\n");
                        goto error;
                }

                uint16_t check_point_num_data = 0;
                for(int i = 0;i < end_pos_countor;i++){

                        if(read_exact(ttf_fd,ui8_2byte_data_storage,sizeof(ui8_2byte_data_storage)) != 0){
                                printf("glyf_table->end_pts_of_contours read error\n");
                                goto error;
                        }

                        uint16_t ui16_data = read_be16(ui8_2byte_data_storage);

                        if(i == 0){
                                check_point_num_data = ui16_data;
                        }
                        else if(check_point_num_data >= ui16_data){
                                printf("this point arry is not ascending order\n");
                                goto error;
                        }
                        else{
                                check_point_num_data = ui16_data;
                        }

                        glyf_table->end_pts_of_contours[i] = ui16_data;

                }
                glyf_table->point_count =  glyf_table->end_pts_of_contours[end_pos_countor-1] + 1;
                glyf_table->end_pts_of_contours_count = end_pos_countor;
        }



        if(read_exact(ttf_fd,ui8_2byte_data_storage,sizeof(ui8_2byte_data_storage)) != 0){
                printf("glyf_table->instruction_length data can not read\n");
                goto error;
        }

        uint16_t instruction_length = read_be16(ui8_2byte_data_storage);
        glyf_table->instruction_length = instruction_length;
        if(instruction_length > 0){
                glyf_table->instructions = malloc(instruction_length);
                if(glyf_table->instructions == NULL){
                        printf("glyf_table->instructions allocate error\n");
                        goto error;
                }

                for(int i=0;i < instruction_length;i++){
                        uint8_t ui8_data_storage;
                        if(read_exact(ttf_fd,&ui8_data_storage,sizeof(ui8_data_storage)) != 0){
                                printf("instruction_length can not read\n");
                                goto error;
                        }

                        glyf_table->instructions[i] = ui8_data_storage;
                }
        }


        if(glyf_table->point_count == 0)return 0;

        glyf_table->flags = malloc(sizeof(uint8_t) * glyf_table->point_count);
        if(glyf_table->flags == NULL){
                printf("glyf_table->flags can not allocate\n");
                goto error;
        }

        size_t point_flags_read_count = 0;
        while(point_flags_read_count < glyf_table->point_count){
                uint8_t ui8_data;
                if(read_exact(ttf_fd,&ui8_data,sizeof(ui8_data)) != 0){
                        printf("point flags can not read\n");
                        goto error;
                }

                if(ui8_data & REPEAT_FLAG){
                        uint8_t nex_arry_mem_data;
                        if(read_exact(ttf_fd,&nex_arry_mem_data,sizeof(nex_arry_mem_data)) != 0){
                                printf("point flags read error\n");
                                goto error;
                        }
                        glyf_table->flags[point_flags_read_count] = ui8_data;
                        point_flags_read_count++;

                        if((size_t)nex_arry_mem_data >
                                glyf_table->point_count - point_flags_read_count){
                                printf("point flags repeat count error\n");
                                goto error;
                        }

                        for(int i = 0;i < nex_arry_mem_data;i++){
                                glyf_table->flags[point_flags_read_count] = ui8_data;
                                point_flags_read_count++;
                        }

                }else{
                        glyf_table->flags[point_flags_read_count] = ui8_data;
                        point_flags_read_count++;
                }
        }

        glyf_table->flags_count = glyf_table->point_count;
        glyf_table->points = calloc(
                glyf_table->point_count,
                sizeof(struct point_data));

        if(glyf_table->points == NULL){
                printf("glyf_table->points allocate error\n");
                goto error;
        }

        uint8_t *flags = glyf_table->flags;
        int32_t current_x = 0;

        for(size_t i = 0;i < glyf_table->point_count;i++){
                uint8_t flag = flags[i];
                int32_t move_x = 0;

                glyf_table->points[i].x_pos_data.flags = flag;
                glyf_table->points[i].y_pos_data.flags = flag;

                if(flag & X_SHORT_VECTOR){
                        uint8_t x_data_storage;
                        if(read_exact(
                                ttf_fd,
                                &x_data_storage,
                                sizeof(x_data_storage)) != 0){
                                printf("x short vector read error\n");
                                goto error;
                        }

                        move_x = x_data_storage;
                        if((flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) == 0){
                                move_x = -move_x;
                        }
                }else if((flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) == 0){
                        uint8_t x_data_storage[2];
                        if(read_exact(
                                ttf_fd,
                                x_data_storage,
                                sizeof(x_data_storage)) != 0){
                                printf("x coordinate read error\n");
                                goto error;
                        }

                        move_x = (int16_t)read_be16(x_data_storage);
                }

                current_x += move_x;
                if(current_x < INT16_MIN || current_x > INT16_MAX){
                        printf("x coordinate range error\n");
                        goto error;
                }

                glyf_table->points[i].x_pos_data.x_cordinates =
                        (int16_t)current_x;
        }

        int32_t current_y = 0;

        for(size_t i = 0;i < glyf_table->point_count;i++){
                uint8_t flag = flags[i];
                int32_t move_y = 0;

                if(flag & Y_SHORT_VECTOR){
                        uint8_t y_data_storage;
                        if(read_exact(
                                ttf_fd,
                                &y_data_storage,
                                sizeof(y_data_storage)) != 0){
                                printf("y short vector read error\n");
                                goto error;
                        }

                        move_y = y_data_storage;
                        if((flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) == 0){
                                move_y = -move_y;
                        }
                }else if((flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) == 0){
                        uint8_t y_data_storage[2];
                        if(read_exact(
                                ttf_fd,
                                y_data_storage,
                                sizeof(y_data_storage)) != 0){
                                printf("y coordinate read error\n");
                                goto error;
                        }

                        move_y = (int16_t)read_be16(y_data_storage);
                }

                current_y += move_y;
                if(current_y < INT16_MIN || current_y > INT16_MAX){
                        printf("y coordinate range error\n");
                        goto error;
                }

                glyf_table->points[i].y_pos_data.y_cordinates =
                        (int16_t)current_y;
        }



        return 0;

error:
        free(glyf_table->end_pts_of_contours);
        glyf_table->end_pts_of_contours = NULL;
        glyf_table->end_pts_of_contours_count = 0;
        free(glyf_table->instructions);
        glyf_table->instructions = NULL;
        free(glyf_table->flags);
        glyf_table->flags = NULL;
        glyf_table->flags_count = 0;
        free(glyf_table->points);
        glyf_table->points = NULL;
        glyf_table->point_count = 0;
        glyf_table->instruction_length = 0;
        return -1;
}
int get_countour_data(struct glyf_table *glif_table,struct contour_data *contour_data){
        if(contour_data == NULL || glif_table == NULL
                || contour_data->pos_data != NULL)return -1;

        if(glif_table->number_of_contours < 0)return -1;
        if(glif_table->number_of_contours == 0){
                contour_data->pos_data_arry_size = 0;
                return 0;
        }

        size_t contour_count = (size_t)glif_table->number_of_contours;

        if(glif_table->end_pts_of_contours == NULL ||
                glif_table->points == NULL ||
                glif_table->end_pts_of_contours_count != contour_count){
                return -1;
        }

        contour_data->pos_data = calloc(
                contour_count,
                sizeof(struct contour_pos_data));
        if(contour_data->pos_data == NULL){
                printf("contour_data->pos_data allocate error\n");
                return -1;
        }

        size_t start_point_num = 0;
        size_t allocated_contour_count = 0;

        for(size_t i = 0;i < contour_count;i++){
                size_t end_point_num = glif_table->end_pts_of_contours[i];

                if(end_point_num < start_point_num ||
                        end_point_num >= glif_table->point_count){
                        printf("contour point range error\n");
                        goto error;
                }

                size_t contour_point_count =
                        end_point_num - start_point_num + 1;

                contour_data->pos_data[i].point_pos_data = malloc(
                        sizeof(struct point_pos) * contour_point_count);
                if(contour_data->pos_data[i].point_pos_data == NULL){
                        printf("point_pos_data allocate error\n");
                        goto error;
                }
                contour_data->pos_data[i].point_pos_data_arry_size =
                        contour_point_count;
                allocated_contour_count++;

                for(size_t j = 0;j < contour_point_count;j++){
                        size_t point_num = start_point_num + j;

                        contour_data->pos_data[i].point_pos_data[j].pos_x =
                                glif_table->points[point_num].x_pos_data.x_cordinates;
                        contour_data->pos_data[i].point_pos_data[j].pos_y =
                                glif_table->points[point_num].y_pos_data.y_cordinates;
                        contour_data->pos_data[i].point_pos_data[j].flags =
                                glif_table->points[point_num].x_pos_data.flags;
                }

                start_point_num = end_point_num + 1;
        }

        contour_data->pos_data_arry_size = (uint16_t)contour_count;
        return 0;

error:
        for(size_t i = 0;i < allocated_contour_count;i++){
                free(contour_data->pos_data[i].point_pos_data);
        }
        free(contour_data->pos_data);
        contour_data->pos_data = NULL;
        contour_data->pos_data_arry_size = 0;
        return -1;
}

void free_countour_data(struct contour_data *contour_data){
        if(contour_data == NULL)return;

        for(size_t i = 0;i < contour_data->pos_data_arry_size;i++){
                free(contour_data->pos_data[i].point_pos_data);
        }

        free(contour_data->pos_data);
        contour_data->pos_data = NULL;
        contour_data->pos_data_arry_size = 0;
}
