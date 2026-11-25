#include "cydui/types.h"

#include "blend.h"

cydui_rect cydui_rect_intersect(cydui_rect a, cydui_rect b)
{
    int32_t x0 = a.x > b.x ? a.x : b.x;
    int32_t y0 = a.y > b.y ? a.y : b.y;
    int32_t ax1 = (int32_t)a.x + a.w, bx1 = (int32_t)b.x + b.w;
    int32_t ay1 = (int32_t)a.y + a.h, by1 = (int32_t)b.y + b.h;
    int32_t x1 = ax1 < bx1 ? ax1 : bx1;
    int32_t y1 = ay1 < by1 ? ay1 : by1;
    cydui_rect r = {0, 0, 0, 0};

    if (a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && x1 > x0 && y1 > y0) {
        r.x = (int16_t)x0;
        r.y = (int16_t)y0;
        r.w = (int16_t)(x1 - x0);
        r.h = (int16_t)(y1 - y0);
    }
    return r;
}

bool cydui_rect_empty(cydui_rect r)
{
    return r.w <= 0 || r.h <= 0;
}

cydui_color cydui_blend(cydui_color dst, cydui_color src, uint8_t alpha)
{
    return cydui__blend565(dst, src, alpha);
}

void cydui_color_unpack(cydui_color c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned r8 = (c >> 8) & 0xF8u;
    unsigned g8 = (c >> 3) & 0xFCu;
    unsigned b8 = (c << 3) & 0xF8u;
    *r = (uint8_t)(r8 | (r8 >> 5));
    *g = (uint8_t)(g8 | (g8 >> 6));
    *b = (uint8_t)(b8 | (b8 >> 5));
}
