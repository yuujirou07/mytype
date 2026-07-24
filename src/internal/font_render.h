#ifndef FONT_RENDER_H
#define FONT_RENDER_H

#include <stdint.h>

#include "font_glyph.h"

int glyph_window_open(void);
int glyph_window_should_close(void);
uint32_t glyph_window_next_codepoint(void);
void glyph_window_draw(
        const struct character_render_data *character,
        const char *input_status);
void glyph_window_close(void);

#endif
