#include "cydui/utf8.h"

size_t cydui_utf8_decode(const char *s, size_t len, uint32_t *cp)
{
    const uint8_t *p = (const uint8_t *)s;
    uint8_t b0, lo = 0x80, hi = 0xBF;
    size_t need, i;
    uint32_t v;

    if (len == 0) {
        *cp = 0;
        return 0;
    }
    b0 = p[0];
    if (b0 < 0x80) {
        *cp = b0;
        return 1;
    }

    /*
     * The second byte has a narrower range after some lead bytes; that is what rules out overlong
     * forms (E0, F0), surrogates (ED) and values above U+10FFFF (F4).
     */
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        need = 1;
        v = b0 & 0x1Fu;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        need = 2;
        v = b0 & 0x0Fu;
        if (b0 == 0xE0) lo = 0xA0;
        if (b0 == 0xED) hi = 0x9F;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        need = 3;
        v = b0 & 0x07u;
        if (b0 == 0xF0) lo = 0x90;
        if (b0 == 0xF4) hi = 0x8F;
    } else { /* 80..C1: continuation or overlong lead; F5..FF: never valid */
        *cp = CYDUI_REPLACEMENT_CHAR;
        return 1;
    }

    for (i = 1; i <= need; i++) {
        uint8_t b;
        if (i >= len) { /* cut short by the end of the buffer */
            *cp = CYDUI_REPLACEMENT_CHAR;
            return i;
        }
        b = p[i];
        if (b < lo || b > hi) { /* the bad byte starts the next sequence */
            *cp = CYDUI_REPLACEMENT_CHAR;
            return i;
        }
        lo = 0x80;
        hi = 0xBF;
        v = (v << 6) | (b & 0x3Fu);
    }
    *cp = v;
    return need + 1;
}
