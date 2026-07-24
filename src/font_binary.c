#include <unistd.h>
#include "font_binary.h"

uint16_t read_be16(const uint8_t data[2]){
        return ((uint16_t)data[0] << 8) | data[1];
}

uint32_t read_be32(const uint8_t data[4]){
        return ((uint32_t)data[0] << 24) |
               ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8) |
               data[3];
}

int read_exact(int fd,uint8_t *data,size_t size){
        size_t read_size = 0;

        while(read_size < size){
                int result = read(fd,data + read_size,size - read_size);
                if(result <= 0)return -1;
                read_size += result;
        }

        return 0;
}
