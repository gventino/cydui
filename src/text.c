#include "cydui/text.h"

#include <string.h>

static bool is_filtered(uint32_t cp)
{
    if (cp < 0x20u || (cp >= 0x7Fu && cp <= 0x9Fu)) { /* C0 and C1 controls, including tab */
        return true;
    }
#if CYDUI_FILTER_CONTROL
    if (cp == 0x061Cu || cp == 0x200Eu || cp == 0x200Fu || (cp >= 0x202Au && cp <= 0x202Eu) ||
        (cp >= 0x2066u && cp <= 0x2069u)) { /* bidirectional formatting */
        return true;
    }
#endif
    return false;
}

const cydui_glyph *cydui_text_resolve(const cydui_font *f, uint32_t cp)
{
    if (cp == '\r') {
        return NULL;
    }
    if (is_filtered(cp)) {
        return cydui_font_fallback(f);
    }
    return cydui_font_lookup(f, cp);
}

size_t cydui_text_clip_len(const char *s, size_t len, bool *truncated)
{
    size_t n, j, expect;
    uint8_t b;

    *truncated = false;
    if (!s) {
        return 0;
    }
    if (len == CYDUI_NTS) {
        for (n = 0; n < CYDUI_MAX_STR && s[n] != '\0'; n++) {
        }
        if (n < CYDUI_MAX_STR) {
            return n;
        }
        /* Hit the limit without seeing the NUL: more text may follow. Never look further. */
        *truncated = true;
    } else if (len > CYDUI_MAX_STR) {
        n = CYDUI_MAX_STR;
        *truncated = true;
    } else {
        return len;
    }

    /* Cut on a character boundary: drop a final sequence that the cut left incomplete. */
    if (n > 0) {
        j = n - 1;
        while (j > 0 && n - 1 - j < 3 && ((uint8_t)s[j] & 0xC0u) == 0x80u) {
            j--;
        }
        b = (uint8_t)s[j];
        expect = b >= 0xF0u ? 4u : b >= 0xE0u ? 3u : b >= 0xC0u && b < 0xF0u ? 2u : 1u;
        if (n - j < expect) {
            n = j;
        }
    }
    return n;
}

void cydui_text_iter_init(cydui_text_iter *it, const cydui_font *f, const char *s, size_t len,
                          int16_t wrap_w)
{
    it->font = f;
    it->s = s;
    it->len = len;
    it->wrap = wrap_w > 0 ? wrap_w : 0;
    it->pos = 0;
}

bool cydui_text_next_line(cydui_text_iter *it, cydui_text_line *line)
{
    size_t start = it->pos, pos = it->pos;
    int width = 0;         /* including spaces */
    int ink_width = 0;     /* up to the last visible glyph */
    bool have_ink = false; /* a visible glyph is already on this line */
    bool prev_space = false;
    bool have_break = false; /* the last space run after some ink, as a place to wrap */
    size_t break_pos = 0;
    int break_width = 0;
    unsigned glyphs = 0;

    if (pos >= it->len) {
        return false;
    }
    while (pos < it->len) {
        uint32_t cp;
        size_t used = cydui_utf8_decode(it->s + pos, it->len - pos, &cp);
        const cydui_glyph *g;
        int adv;
        bool is_space;

        if (cp == '\n') {
            line->start = start;
            line->end = pos;
            line->width = ink_width;
            it->pos = pos + used;
            return true;
        }
        g = cydui_text_resolve(it->font, cp);
        adv = g ? g->advance : 0;
        is_space = (cp == ' ');

        if (is_space && !prev_space && have_ink) {
            have_break = true;
            break_pos = pos;
            break_width = ink_width;
        }
        if (it->wrap && glyphs > 0 && width + adv > it->wrap) {
            if (have_break) { /* wrap at the last space run; skip the spaces for the next line */
                line->start = start;
                line->end = break_pos;
                line->width = break_width;
                pos = break_pos;
                while (pos < it->len && it->s[pos] == ' ') {
                    pos++;
                }
                it->pos = pos;
                return true;
            }
            if (!is_space) { /* a word wider than the line: break inside it */
                line->start = start;
                line->end = pos;
                line->width = ink_width;
                it->pos = pos;
                return true;
            }
        }
        width += adv;
        glyphs++;
        if (!is_space && g) {
            ink_width = width;
            have_ink = true;
        }
        prev_space = is_space;
        pos += used;
    }
    line->start = start;
    line->end = pos;
    line->width = ink_width;
    it->pos = pos;
    return true;
}

static int16_t clamp16(long v)
{
    return (int16_t)(v > 32767 ? 32767 : v);
}

cydui_text_extent cydui_text_size(const cydui_font *f, const char *s, size_t len, int16_t wrap_w)
{
    cydui_text_extent e = {0, 0, 0};
    cydui_text_iter it;
    cydui_text_line ln;
    bool truncated;
    int widest = 0;

    if (!cydui_font_fallback(f)) {
        return e;
    }
    len = cydui_text_clip_len(s, len, &truncated);
    cydui_text_iter_init(&it, f, s, len, wrap_w);
    while (cydui_text_next_line(&it, &ln)) {
        if (ln.width > widest) {
            widest = ln.width;
        }
        e.lines++;
    }
    e.w = clamp16(widest);
    e.h = clamp16((long)e.lines * f->line_height);
    return e;
}

/* ---- fitting ---- */

static bool is_continuation(const char *s, size_t i)
{
    return ((uint8_t)s[i] & 0xC0u) == 0x80u;
}

/* The largest boundary (the start of a character, or the end) at or before `i`. */
static size_t boundary_down(const char *s, size_t n, size_t i)
{
    while (i > 0 && i < n && is_continuation(s, i)) {
        i--;
    }
    return i;
}

/* The smallest boundary after `i` (i < n). */
static size_t boundary_after(const char *s, size_t n, size_t i)
{
    i++;
    while (i < n && is_continuation(s, i)) {
        i++;
    }
    return i;
}

static bool fits(const cydui_font *f, const char *s, size_t k, int16_t max_w)
{
    return cydui_text_size(f, s, k, 0).w <= max_w;
}

size_t cydui_text_fit(const cydui_font *f, const char *s, size_t len, int16_t max_w)
{
    bool truncated;
    size_t n, lo = 0, hi;

    if (!s || max_w < 0 || !cydui_font_fallback(f)) {
        return 0;
    }
    n = cydui_text_clip_len(s, len, &truncated);
    if (fits(f, s, n, max_w)) {
        return n;
    }
    hi = n - 1; /* the whole does not fit: the answer is in [0, n - 1] */
    while (lo < hi) { /* lo always fits; the answer is in [lo, hi] */
        size_t mid = lo + (hi - lo + 1) / 2, b = boundary_down(s, n, mid);

        if (b <= lo) { /* mid is inside the character that starts at lo: only the next boundary is left to try */
            size_t nb = boundary_after(s, n, lo);
            if (nb <= hi && fits(f, s, nb, max_w)) {
                lo = nb;
                continue;
            }
            break;
        }
        if (fits(f, s, b, max_w)) {
            lo = b;
        } else {
            hi = b - 1;
        }
    }
    return lo;
}

size_t cydui_text_ellipsize(const cydui_font *f, const char *s, size_t len, int16_t max_w, char *out, size_t cap, bool *cut)
{
    static const char DOTS[] = "\xE2\x80\xA6";
    const char *ell = DOTS;
    size_t ell_len = 3, n, take, limit;
    int ell_w;
    bool truncated;
    const cydui_glyph *g;

    if (cut) {
        *cut = false;
    }
    if (!out || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!s || !cydui_font_fallback(f)) {
        return 0;
    }
    n = cydui_text_clip_len(s, len, &truncated);
    if (max_w >= 0 && fits(f, s, n, max_w) && n < cap) {
        memcpy(out, s, n);
        out[n] = '\0';
        return n;
    }
    if (cut) {
        *cut = true;
    }
    g = cydui_font_lookup(f, 0x2026u);
    if (!g || g->cp != 0x2026u) {
        ell = "...";
        ell_len = 3;
    }
    if (cap <= ell_len) { /* not even the ellipsis fits the buffer: what fits of it, whole characters only */
        size_t m = cap - 1;
        m = boundary_down(ell, ell_len, m);
        memcpy(out, ell, m);
        out[m] = '\0';
        return m;
    }
    ell_w = cydui_text_size(f, ell, ell_len, 0).w;
    limit = cap - 1 - ell_len;
    /* the room left for the text; when the ellipsis alone does not fit, it is shown alone, the least there is */
    take = max_w >= ell_w ? cydui_text_fit(f, s, n, (int16_t)(max_w - ell_w)) : 0;
    if (take > limit) {
        take = boundary_down(s, n, limit);
    }
    /* the whole as drawn is not the sum of its parts: back off a character at a time until it fits */
    for (;;) {
        memcpy(out, s, take);
        memcpy(out + take, ell, ell_len);
        if (take == 0 || cydui_text_size(f, out, take + ell_len, 0).w <= max_w) {
            break;
        }
        take = boundary_down(s, n, take - 1);
    }
    out[take + ell_len] = '\0';
    return take + ell_len;
}
