/*
 * libFuzzer harness for the text editing (CT-12, AB-01). Runs arbitrary operations on a buffer of an arbitrary size
 * (an exact-size heap block, so that ASan sees any access outside it) and checks after every one that the string is
 * closed by a NUL inside the buffer, that the cursor is inside it on the start of a character, and that the limits
 * were respected by what was inserted.
 *
 * Input: cap (1 byte, 0..48), max_chars (1 byte), flags (1 byte), then operations of a byte and its operands.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/textedit.h"
#include "cydui/utf8.h"

static void check(const char *buf, size_t cap, size_t cursor)
{
    size_t len, i = 0, step;
    uint32_t cp;

    if (cap == 0) {
        return;
    }
    len = cydui_textedit_len(buf, cap);
    if (len >= cap || buf[len] != '\0' || cursor > len) {
        abort();
    }
    while (i < cursor) { /* the cursor is on a boundary of the decoder */
        step = cydui_utf8_decode(buf + i, len - i, &cp);
        i += step;
    }
    if (i != cursor) {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t cap, max_chars, cursor = 0, pos = 3;
    unsigned flags;
    char *buf;

    if (size < 3) {
        return 0;
    }
    cap = data[0] % 49u;
    max_chars = data[1] % 40u;
    flags = data[2] & 1u;
    buf = (char *)malloc(cap ? cap : 1);
    if (!buf) {
        return 0;
    }
    memset(buf, 'k', cap); /* not terminated: the first thing is to close it, as an application would */
    cydui_textedit_normalize(buf, cap, &cursor);
    while (pos < size) {
        uint8_t op = data[pos++] % 9u;
        uint32_t cp = 0;
        size_t i, before;

        switch (op) {
        case 0:
        case 1:
        case 2: /* insert: a code point of 1, 2 or 4 bytes of the input */
            for (i = 0; i < (op == 2 ? 4u : op + 1u) && pos < size; i++) {
                cp = (cp << 8) | data[pos++];
            }
            before = cap ? cydui_textedit_chars(buf, cap) : 0;
            if (cap && (cydui_textedit_insert(buf, cap, &cursor, cp, flags, max_chars) & CYDUI_EDIT_TEXT) &&
                max_chars != 0 && cydui_textedit_chars(buf, cap) > (before < max_chars ? max_chars : before)) {
                abort(); /* a character was inserted over the limit */
            }
            break;
        case 3:
            cydui_textedit_backspace(buf, cap, &cursor);
            break;
        case 4:
            cydui_textedit_delete(buf, cap, &cursor);
            break;
        case 5:
            if (pos < size) {
                cydui_textedit_move(buf, cap, &cursor, (cydui_move)(data[pos++] % 4u), flags);
            }
            break;
        case 6: /* any cursor at all */
            if (pos + 1 < size) {
                cursor = (size_t)data[pos] << 8 | data[pos + 1];
                pos += 2;
            }
            cydui_textedit_normalize(buf, cap, &cursor);
            break;
        default: /* the application fills the buffer with arbitrary bytes */
            for (i = 0; i < cap && pos < size; i++) {
                buf[i] = (char)data[pos++];
            }
            cydui_textedit_normalize(buf, cap, &cursor);
            break;
        }
        check(buf, cap, cursor);
    }
    free(buf);
    return 0;
}
