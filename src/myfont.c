#include "../include/myfont.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint16_t myfont_read_be16(const uint8_t data[2]){
        return ((uint16_t)data[0] << 8) | data[1];
}

static int myfont_read_exact(int fd,uint8_t *data,size_t data_size){
        size_t readed_size = 0;

        while(readed_size < data_size){
                unsigned int request_size =
                        (unsigned int)(data_size - readed_size);
                int result = read(fd,data + readed_size,request_size);
                if(result <= 0)return -1;
                readed_size += (size_t)result;
        }

        return 0;
}

void myfont_glyph_data_init(struct myfont_glyph_data *glyph_data){
        if(glyph_data == NULL)return;
        memset(glyph_data,0,sizeof(*glyph_data));
}

void myfont_glyph_data_free(struct myfont_glyph_data *glyph_data){
        if(glyph_data == NULL)return;

        free(glyph_data->end_pts_of_contours);
        free(glyph_data->instructions);
        free(glyph_data->flags);
        free(glyph_data->x_coordinates);
        free(glyph_data->y_coordinates);

        myfont_glyph_data_init(glyph_data);
}

int myfont_parse_glyph_header(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || glyph_data == NULL)return -1;
        if(glyph_data_size < 10)return -1;

        glyph_data->number_of_contours =
                (int16_t)myfont_read_be16(glyph_file_data);
        glyph_data->x_min =
                (int16_t)myfont_read_be16(glyph_file_data + 2);
        glyph_data->y_min =
                (int16_t)myfont_read_be16(glyph_file_data + 4);
        glyph_data->x_max =
                (int16_t)myfont_read_be16(glyph_file_data + 6);
        glyph_data->y_max =
                (int16_t)myfont_read_be16(glyph_file_data + 8);

        if(glyph_data->number_of_contours < 0){
                printf("composite glyph is not support\n");
                return -1;
        }

        return 0;
}

int myfont_parse_end_points(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || data_pos == NULL || glyph_data == NULL)return -1;
        if(*data_pos > glyph_data_size)return -1;

        if(glyph_data->number_of_contours == 0){
                glyph_data->point_count = 0;
                return 0;
        }

        if(glyph_data->number_of_contours < 0)return -1;

        size_t contour_count = (size_t)glyph_data->number_of_contours;
        if(contour_count > (glyph_data_size - *data_pos) / 2)return -1;

        glyph_data->end_pts_of_contours =
                malloc(sizeof(uint16_t) * contour_count);
        if(glyph_data->end_pts_of_contours == NULL){
                printf("end points allocate error\n");
                return -1;
        }

        uint16_t before_end_point = 0;

        for(size_t i = 0;i < contour_count;i++){
                uint16_t end_point =
                        myfont_read_be16(glyph_file_data + *data_pos);
                *data_pos += 2;

                if(i > 0 && before_end_point >= end_point){
                        printf("end points is not ascending order\n");
                        goto error;
                }

                glyph_data->end_pts_of_contours[i] = end_point;
                before_end_point = end_point;
        }

        glyph_data->end_pts_of_contours_count = contour_count;
        glyph_data->point_count = (size_t)before_end_point + 1;
        return 0;

error:
        free(glyph_data->end_pts_of_contours);
        glyph_data->end_pts_of_contours = NULL;
        glyph_data->end_pts_of_contours_count = 0;
        glyph_data->point_count = 0;
        return -1;
}

int myfont_parse_instructions(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || data_pos == NULL || glyph_data == NULL)return -1;
        if(*data_pos > glyph_data_size || glyph_data_size - *data_pos < 2)return -1;

        glyph_data->instruction_length =
                myfont_read_be16(glyph_file_data + *data_pos);
        *data_pos += 2;

        if((size_t)glyph_data->instruction_length > glyph_data_size - *data_pos){
                glyph_data->instruction_length = 0;
                return -1;
        }

        if(glyph_data->instruction_length == 0)return 0;

        glyph_data->instructions = malloc(glyph_data->instruction_length);
        if(glyph_data->instructions == NULL){
                glyph_data->instruction_length = 0;
                printf("instructions allocate error\n");
                return -1;
        }

        memcpy(
                glyph_data->instructions,
                glyph_file_data + *data_pos,
                glyph_data->instruction_length);
        *data_pos += glyph_data->instruction_length;

        return 0;
}

int myfont_parse_flags(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || data_pos == NULL || glyph_data == NULL)return -1;
        if(glyph_data->point_count == 0)return 0;

        glyph_data->flags = malloc(glyph_data->point_count);
        if(glyph_data->flags == NULL){
                printf("flags allocate error\n");
                return -1;
        }

        size_t point_num = 0;

        while(point_num < glyph_data->point_count){
                if(*data_pos >= glyph_data_size)goto error;

                uint8_t flag = glyph_file_data[*data_pos];
                *data_pos += 1;
                glyph_data->flags[point_num] = flag;
                point_num++;

                if((flag & MYFONT_REPEAT_FLAG) != 0){
                        if(*data_pos >= glyph_data_size)goto error;

                        uint8_t repeat_count = glyph_file_data[*data_pos];
                        *data_pos += 1;

                        if((size_t)repeat_count > glyph_data->point_count - point_num){
                                goto error;
                        }

                        for(uint8_t i = 0;i < repeat_count;i++){
                                glyph_data->flags[point_num] = flag;
                                point_num++;
                        }
                }
        }

        glyph_data->flags_count = glyph_data->point_count;
        return 0;

error:
        free(glyph_data->flags);
        glyph_data->flags = NULL;
        glyph_data->flags_count = 0;
        printf("flags parse error\n");
        return -1;
}

int myfont_parse_x_coordinates(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || data_pos == NULL || glyph_data == NULL)return -1;
        if(glyph_data->flags_count != glyph_data->point_count)return -1;
        if(glyph_data->point_count == 0)return 0;

        glyph_data->x_coordinates =
                malloc(sizeof(int16_t) * glyph_data->point_count);
        if(glyph_data->x_coordinates == NULL){
                printf("x coordinates allocate error\n");
                return -1;
        }

        int32_t current_x = 0;

        for(size_t i = 0;i < glyph_data->point_count;i++){
                uint8_t flag = glyph_data->flags[i];
                int32_t move_x = 0;

                if((flag & MYFONT_X_SHORT_VECTOR) != 0){
                        if(*data_pos >= glyph_data_size)goto error;

                        move_x = glyph_file_data[*data_pos];
                        *data_pos += 1;

                        if((flag & MYFONT_X_IS_SAME_OR_POSITIVE) == 0){
                                move_x = -move_x;
                        }
                }else if((flag & MYFONT_X_IS_SAME_OR_POSITIVE) == 0){
                        if(*data_pos > glyph_data_size || glyph_data_size - *data_pos < 2){
                                goto error;
                        }

                        move_x = (int16_t)myfont_read_be16(glyph_file_data + *data_pos);
                        *data_pos += 2;
                }

                current_x += move_x;
                if(current_x < INT16_MIN || current_x > INT16_MAX)goto error;
                glyph_data->x_coordinates[i] = (int16_t)current_x;
        }

        return 0;

error:
        free(glyph_data->x_coordinates);
        glyph_data->x_coordinates = NULL;
        printf("x coordinates parse error\n");
        return -1;
}

int myfont_parse_y_coordinates(
        const uint8_t *glyph_file_data,
        size_t glyph_data_size,
        size_t *data_pos,
        struct myfont_glyph_data *glyph_data){

        if(glyph_file_data == NULL || data_pos == NULL || glyph_data == NULL)return -1;
        if(glyph_data->flags_count != glyph_data->point_count)return -1;
        if(glyph_data->point_count == 0)return 0;

        glyph_data->y_coordinates =
                malloc(sizeof(int16_t) * glyph_data->point_count);
        if(glyph_data->y_coordinates == NULL){
                printf("y coordinates allocate error\n");
                return -1;
        }

        int32_t current_y = 0;

        for(size_t i = 0;i < glyph_data->point_count;i++){
                uint8_t flag = glyph_data->flags[i];
                int32_t move_y = 0;

                if((flag & MYFONT_Y_SHORT_VECTOR) != 0){
                        if(*data_pos >= glyph_data_size)goto error;

                        move_y = glyph_file_data[*data_pos];
                        *data_pos += 1;

                        if((flag & MYFONT_Y_IS_SAME_OR_POSITIVE) == 0){
                                move_y = -move_y;
                        }
                }else if((flag & MYFONT_Y_IS_SAME_OR_POSITIVE) == 0){
                        if(*data_pos > glyph_data_size || glyph_data_size - *data_pos < 2){
                                goto error;
                        }

                        move_y = (int16_t)myfont_read_be16(glyph_file_data + *data_pos);
                        *data_pos += 2;
                }

                current_y += move_y;
                if(current_y < INT16_MIN || current_y > INT16_MAX)goto error;
                glyph_data->y_coordinates[i] = (int16_t)current_y;
        }

        return 0;

error:
        free(glyph_data->y_coordinates);
        glyph_data->y_coordinates = NULL;
        printf("y coordinates parse error\n");
        return -1;
}

int myfont_parse_simple_glyph(
        int ttf_fd,
        uint32_t glyph_file_pos,
        uint32_t glyph_data_length,
        struct myfont_glyph_data *glyph_data){

        if(glyph_data == NULL || glyph_data_length < 10)return -1;

        if(lseek(ttf_fd,(off_t)glyph_file_pos,SEEK_SET) < 0){
                printf("glyph lseek error\n");
                return -1;
        }

        uint8_t *glyph_file_data = malloc(glyph_data_length);
        if(glyph_file_data == NULL){
                printf("glyph file data allocate error\n");
                return -1;
        }

        if(myfont_read_exact(ttf_fd,glyph_file_data,glyph_data_length) != 0){
                printf("glyph file data read error\n");
                goto error;
        }

        if(myfont_parse_glyph_header(
                glyph_file_data,glyph_data_length,glyph_data) != 0)goto error;

        size_t data_pos = 10;

        if(myfont_parse_end_points(
                glyph_file_data,glyph_data_length,&data_pos,glyph_data) != 0)goto error;

        if(glyph_data->number_of_contours == 0 && data_pos == glyph_data_length){
                free(glyph_file_data);
                return 0;
        }

        if(myfont_parse_instructions(
                glyph_file_data,glyph_data_length,&data_pos,glyph_data) != 0)goto error;

        if(myfont_parse_flags(
                glyph_file_data,glyph_data_length,&data_pos,glyph_data) != 0)goto error;

        if(myfont_parse_x_coordinates(
                glyph_file_data,glyph_data_length,&data_pos,glyph_data) != 0)goto error;

        if(myfont_parse_y_coordinates(
                glyph_file_data,glyph_data_length,&data_pos,glyph_data) != 0)goto error;

        free(glyph_file_data);
        return 0;

error:
        free(glyph_file_data);
        myfont_glyph_data_free(glyph_data);
        return -1;
}

int myfont_get_contour_range(
        const struct myfont_glyph_data *glyph_data,
        size_t contour_num,
        size_t *start_point,
        size_t *end_point){

        if(glyph_data == NULL || start_point == NULL || end_point == NULL)return -1;
        if(glyph_data->end_pts_of_contours == NULL)return -1;
        if(contour_num >= glyph_data->end_pts_of_contours_count)return -1;

        if(contour_num == 0){
                *start_point = 0;
        }else{
                *start_point =
                        (size_t)glyph_data->end_pts_of_contours[contour_num - 1] + 1;
        }

        *end_point = glyph_data->end_pts_of_contours[contour_num];
        return 0;
}
