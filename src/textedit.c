#include "cydui/textedit.h"

#include <string.h>

#include "cydui/text.h"
#include "cydui/utf8.h"

size_t cydui_textedit_len(const char *buf, size_t cap)
{
    size_t n = 0;

    while (n + 1 < cap && buf[n] != '\0') {
        n++;
    }
    return n;
}

size_t cydui_textedit_chars(const char *buf, size_t cap)
{
    size_t len = cydui_textedit_len(buf, cap), i = 0, n = 0;
    uint32_t cp;

    while (i < len) {
        i += cydui_utf8_decode(buf + i, len - i, &cp);
        n++;
    }
    return n;
}

/* The start of the character that holds byte `at` (or `len` for `at` >= len), found from the start of the text
 * so that it agrees with the decoder even for ill-formed bytes. */
static size_t boundary_at_or_before(const char *buf, size_t len, size_t at)
{
    size_t i = 0, step;
    uint32_t cp;

    if (at >= len) {
        return len;
    }
    for (;;) {
        step = cydui_utf8_decode(buf + i, len - i, &cp);
        if (i + step > at) {
            return i;
        }
        i += step;
    }
}

static size_t step_right(const char *buf, size_t len, size_t at)
{
    uint32_t cp;

    return at >= len ? len : at + cydui_utf8_decode(buf + at, len - at, &cp);
}

static size_t step_left(const char *buf, size_t len, size_t at)
{
    return at == 0 ? 0 : boundary_at_or_before(buf, len, at - 1);
}

void cydui_textedit_normalize(char *buf, size_t cap, size_t *cursor)
{
    size_t len, i;

    if (cap == 0) {
        *cursor = 0;
        return;
    }
    for (i = 0; i < cap && buf[i] != '\0'; i++) {
    }
    if (i == cap) {
        buf[cap - 1] = '\0';
    }
    len = cydui_textedit_len(buf, cap);
    *cursor = boundary_at_or_before(buf, len, *cursor);
}

static size_t encode(uint32_t cp, char out[4])
{
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

static int accepted(uint32_t cp, unsigned flags)
{
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return 0;
    }
    if (cp == '\n') {
        return (flags & CYDUI_EDIT_MULTILINE) != 0;
    }
    if (cp < 0x20u || (cp >= 0x7Fu && cp <= 0x9Fu)) {
        return 0;
    }
#if CYDUI_FILTER_CONTROL
    if (cp == 0x061Cu || cp == 0x200Eu || cp == 0x200Fu || (cp >= 0x202Au && cp <= 0x202Eu) ||
        (cp >= 0x2066u && cp <= 0x2069u)) {
        return 0;
    }
#endif
    return 1;
}

unsigned cydui_textedit_insert(char *buf, size_t cap, size_t *cursor, uint32_t cp, unsigned flags, size_t max_chars)
{
    char enc[4];
    size_t len, n;

    cydui_textedit_normalize(buf, cap, cursor);
    if (cap == 0 || !accepted(cp, flags)) {
        return 0;
    }
    len = cydui_textedit_len(buf, cap);
    n = encode(cp, enc);
    if (len + n > cap - 1u || (max_chars != 0 && cydui_textedit_chars(buf, cap) >= max_chars)) {
        return CYDUI_EDIT_FULL;
    }
    memmove(buf + *cursor + n, buf + *cursor, len - *cursor + 1u); /* with the NUL */
    memcpy(buf + *cursor, enc, n);
    *cursor += n;
    return CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR;
}

unsigned cydui_textedit_backspace(char *buf, size_t cap, size_t *cursor)
{
    size_t len, from;

    cydui_textedit_normalize(buf, cap, cursor);
    if (cap == 0 || *cursor == 0) {
        return 0;
    }
    len = cydui_textedit_len(buf, cap);
    from = step_left(buf, len, *cursor);
    memmove(buf + from, buf + *cursor, len - *cursor + 1u);
    *cursor = boundary_at_or_before(buf, len - (*cursor - from), from); /* what was around may have merged into one character */
    return CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR;
}

unsigned cydui_textedit_delete(char *buf, size_t cap, size_t *cursor)
{
    size_t len, to;

    cydui_textedit_normalize(buf, cap, cursor);
    if (cap == 0) {
        return 0;
    }
    len = cydui_textedit_len(buf, cap);
    if (*cursor >= len) {
        return 0;
    }
    to = step_right(buf, len, *cursor);
    memmove(buf + *cursor, buf + to, len - to + 1u);
    *cursor = boundary_at_or_before(buf, len - (to - *cursor), *cursor); /* the same: the neighbours may have merged */
    return CYDUI_EDIT_TEXT;
}

unsigned cydui_textedit_move(char *buf, size_t cap, size_t *cursor, cydui_move how, unsigned flags)
{
    size_t len, to = 0;

    cydui_textedit_normalize(buf, cap, cursor);
    if (cap == 0) {
        return 0;
    }
    len = cydui_textedit_len(buf, cap);
    switch (how) {
    case CYDUI_MOVE_LEFT:
        to = step_left(buf, len, *cursor);
        break;
    case CYDUI_MOVE_RIGHT:
        to = step_right(buf, len, *cursor);
        break;
    case CYDUI_MOVE_HOME:
        to = *cursor;
        if (flags & CYDUI_EDIT_MULTILINE) {
            while (to > 0 && buf[to - 1] != '\n') {
                to--;
            }
        } else {
            to = 0;
        }
        break;
    case CYDUI_MOVE_END:
        to = *cursor;
        if (flags & CYDUI_EDIT_MULTILINE) {
            while (to < len && buf[to] != '\n') {
                to++;
            }
        } else {
            to = len;
        }
        break;
    default:
        return 0;
    }
    if (to == *cursor) {
        return 0;
    }
    *cursor = to;
    return CYDUI_EDIT_CURSOR;
}
