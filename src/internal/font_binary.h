#ifndef FONT_BINARY_H
#define FONT_BINARY_H

#include <stddef.h>
#include <stdint.h>

uint16_t read_be16(const uint8_t data[2]);
uint32_t read_be32(const uint8_t data[4]);
int read_exact(int fd,uint8_t *data,size_t size);

#endif
