#ifndef CYDUI_BLEND_H
#define CYDUI_BLEND_H

/* The one definition of the RGB565 blend: cydui_blend() calls it and the rasterizer inlines it. */

#include "cydui/types.h"

/* RGB565 fields spread so each has headroom for the multiply: 0x07E0F81F. */
#define CYDUI_SPREAD_MASK 0x07E0F81Fu

static inline cydui_color cydui__blend565(cydui_color dst, cydui_color src, uint8_t alpha)
{
    uint32_t a = ((uint32_t)alpha + 4u) >> 3; /* 0..32 */
    uint32_t s = ((uint32_t)src | ((uint32_t)src << 16)) & CYDUI_SPREAD_MASK;
    uint32_t d = ((uint32_t)dst | ((uint32_t)dst << 16)) & CYDUI_SPREAD_MASK;
    uint32_t r = ((((s - d) * a) >> 5) + d) & CYDUI_SPREAD_MASK;
    return (cydui_color)(r | (r >> 16));
}

#endif /* CYDUI_BLEND_H */
