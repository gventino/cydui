#include "cydui_mem.h"

#include <stdio.h>
#include <string.h>

static void mem_flush(void *user, const cydui_rect *area, const cydui_color *pixels)
{
    cydui_mem *m = (cydui_mem *)user;
    int y;
    for (y = 0; y < area->h; y++) {
        memcpy(m->fb + (size_t)(area->y + y) * (size_t)m->backend.width + (size_t)area->x,
               pixels + (size_t)y * (size_t)area->w, (size_t)area->w * sizeof(cydui_color));
    }
    m->flushes++;
}

static bool mem_poll(void *user, cydui_event *ev)
{
    cydui_mem *m = (cydui_mem *)user;
    if (m->ev_len == 0) {
        return false;
    }
    *ev = m->events[m->ev_head];
    m->ev_head = (uint16_t)((m->ev_head + 1u) % CYDUI_MEM_EVENTS);
    m->ev_len--;
    return true;
}

bool cydui_mem_push_event(cydui_mem *m, cydui_event_type type, int16_t x, int16_t y)
{
    cydui_event *slot;
    if (m->ev_len >= CYDUI_MEM_EVENTS) {
        return false;
    }
    slot = &m->events[(m->ev_head + m->ev_len) % CYDUI_MEM_EVENTS];
    slot->type = type;
    slot->x = x;
    slot->y = y;
    m->ev_len++;
    return true;
}

static uint32_t mem_now(void *user)
{
    return ((cydui_mem *)user)->now;
}

void cydui_mem_init(cydui_mem *m, cydui_color *fb, int16_t width, int16_t height)
{
    memset(m, 0, sizeof(*m));
    m->fb = fb;
    m->backend.user = m;
    m->backend.width = width;
    m->backend.height = height;
    m->backend.flush_strip = mem_flush;
    m->backend.poll_event = mem_poll;
    m->backend.now_ms = mem_now;
}

uint64_t cydui_mem_hash(const cydui_mem *m)
{
    uint64_t h = 1469598103934665603ull;
    size_t i, n = (size_t)m->backend.width * (size_t)m->backend.height;
    for (i = 0; i < n; i++) {
        h = (h ^ (uint64_t)(m->fb[i] & 0xFFu)) * 1099511628211ull;
        h = (h ^ (uint64_t)(m->fb[i] >> 8)) * 1099511628211ull;
    }
    return h;
}

/* ---- minimal PNG writer: 8-bit RGB, zlib "stored" blocks (no compression) ---- */

typedef struct {
    FILE *f;
    uint32_t crc;
    uint32_t adler_a, adler_b;
} png_t;

static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n)
{
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

static void put_be32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)(v >> 24);
    o[1] = (uint8_t)(v >> 16);
    o[2] = (uint8_t)(v >> 8);
    o[3] = (uint8_t)v;
}

/* Writes bytes into the current chunk, updating its CRC. */
static void chunk_bytes(png_t *p, const uint8_t *d, size_t n)
{
    fwrite(d, 1, n, p->f);
    p->crc = crc_update(p->crc, d, n);
}

static void chunk_begin(png_t *p, uint32_t len, const char *type)
{
    uint8_t b[4];
    put_be32(b, len);
    fwrite(b, 1, 4, p->f);
    p->crc = 0xFFFFFFFFu;
    chunk_bytes(p, (const uint8_t *)type, 4);
}

static void chunk_end(png_t *p)
{
    uint8_t b[4];
    put_be32(b, p->crc ^ 0xFFFFFFFFu);
    fwrite(b, 1, 4, p->f);
}

static void adler_update(png_t *p, const uint8_t *d, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p->adler_a = (p->adler_a + d[i]) % 65521u;
        p->adler_b = (p->adler_b + p->adler_a) % 65521u;
    }
}

/* Emits `n` raw bytes of zlib payload, splitting into stored blocks as needed. */
static void zlib_emit(png_t *p, const uint8_t *d, size_t n, size_t *left_in_block,
                      size_t *remaining_total)
{
    while (n > 0) {
        size_t take;
        if (*left_in_block == 0) {
            size_t blk = *remaining_total > 65535u ? 65535u : *remaining_total;
            uint8_t hdr[5];
            hdr[0] = (*remaining_total <= 65535u) ? 1u : 0u;
            hdr[1] = (uint8_t)(blk & 0xFFu);
            hdr[2] = (uint8_t)(blk >> 8);
            hdr[3] = (uint8_t)(~blk & 0xFFu);
            hdr[4] = (uint8_t)((~blk >> 8) & 0xFFu);
            chunk_bytes(p, hdr, 5);
            *left_in_block = blk;
        }
        take = n < *left_in_block ? n : *left_in_block;
        chunk_bytes(p, d, take);
        adler_update(p, d, take);
        d += take;
        n -= take;
        *left_in_block -= take;
        *remaining_total -= take;
    }
}

int cydui_mem_write_png(const cydui_mem *m, const char *path)
{
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    size_t w = (size_t)m->backend.width, h = (size_t)m->backend.height;
    size_t raw_len = h * (1u + 3u * w);
    size_t blocks = (raw_len + 65534u) / 65535u;
    size_t left_in_block = 0, remaining = raw_len;
    png_t p;
    uint8_t ihdr[13], row[1 + 3 * 1024];
    uint8_t zhdr[2] = {0x78, 0x01}, ad[4];
    size_t x, y;

    if (w > 1024u) {
        return -1;
    }
    p.f = fopen(path, "wb");
    if (!p.f) {
        return -1;
    }
    p.adler_a = 1;
    p.adler_b = 0;
    fwrite(sig, 1, 8, p.f);

    put_be32(ihdr, (uint32_t)w);
    put_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* RGB */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk_begin(&p, 13, "IHDR");
    chunk_bytes(&p, ihdr, 13);
    chunk_end(&p);

    chunk_begin(&p, (uint32_t)(2u + raw_len + 5u * blocks + 4u), "IDAT");
    chunk_bytes(&p, zhdr, 2);
    for (y = 0; y < h; y++) {
        row[0] = 0; /* filter: none */
        for (x = 0; x < w; x++) {
            uint16_t c = m->fb[y * w + x];
            unsigned r8 = (c >> 8) & 0xF8u, g8 = (c >> 3) & 0xFCu, b8 = (c << 3) & 0xF8u;
            row[1 + 3 * x] = (uint8_t)(r8 | (r8 >> 5));
            row[2 + 3 * x] = (uint8_t)(g8 | (g8 >> 6));
            row[3 + 3 * x] = (uint8_t)(b8 | (b8 >> 5));
        }
        zlib_emit(&p, row, 1u + 3u * w, &left_in_block, &remaining);
    }
    put_be32(ad, (p.adler_b << 16) | p.adler_a);
    chunk_bytes(&p, ad, 4);
    chunk_end(&p);

    chunk_begin(&p, 0, "IEND");
    chunk_end(&p);
    return fclose(p.f) == 0 ? 0 : -1;
}
