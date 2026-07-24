#ifndef FONT_RENDER_H
#define FONT_RENDER_H

#include <stdint.h>

#include "font_glyph.h"

int show_glyph_with_raylib(
        const struct contour_data *contour_data,
        const struct glyf_table *glyf_table,
        uint16_t units_per_em);

#endif
