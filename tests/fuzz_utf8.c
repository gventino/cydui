/*
 * libFuzzer harness for the UTF-8 decoder (CT-03). Walks arbitrary bytes and checks, at every
 * step, that the decoder makes progress, stays inside the buffer, only returns Unicode scalar
 * values, and that a well-formed character re-encodes to exactly the bytes it came from.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/utf8.h"

static size_t encode(uint32_t cp, uint8_t out[4])
{
    if (cp < 0x80) {
        out[0] = (uint8_t)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (uint8_t)(0xC0 | (cp >> 6));
        out[1] = (uint8_t)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (uint8_t)(0xE0 | (cp >> 12));
        out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (uint8_t)(0xF0 | (cp >> 18));
    out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (uint8_t)(0x80 | (cp & 0x3F));
    return 4;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* An exact-size heap copy, so that ASan sees any read past the end. */
    char *buf = (char *)malloc(size ? size : 1);
    size_t pos = 0;

    if (!buf) {
        return 0;
    }
    memcpy(buf, data, size);
    while (pos < size) {
        uint32_t cp;
        size_t used = cydui_utf8_decode(buf + pos, size - pos, &cp);

        if (used < 1 || used > size - pos) {
            abort(); /* no progress, or past the end */
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            abort(); /* not a scalar value */
        }
        if (cp != CYDUI_REPLACEMENT_CHAR || used == 3) {
            /* Well-formed (or a genuine U+FFFD, which is 3 bytes): must re-encode to itself. */
            uint8_t enc[4];
            size_t n = encode(cp, enc);
            if (cp != CYDUI_REPLACEMENT_CHAR || (used == 3 && memcmp(enc, buf + pos, 3) == 0)) {
                if (n != used || memcmp(enc, buf + pos, n) != 0) {
                    abort();
                }
            }
        }
        pos += used;
    }
    free(buf);
    return 0;
}
