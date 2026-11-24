#ifndef CYDUI_UTF8_H
#define CYDUI_UTF8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Code point written for any ill-formed byte sequence. */
#define CYDUI_REPLACEMENT_CHAR 0xFFFDu

/**
 * Decodes one code point from the first `len` bytes of `s`.
 *
 * Strict: overlong forms, surrogates (U+D800..U+DFFF), values above U+10FFFF, stray continuation
 * bytes and bytes that can never appear in UTF-8 (C0, C1, F5..FF) are ill-formed. An ill-formed
 * sequence yields CYDUI_REPLACEMENT_CHAR and consumes its "maximal subpart": the longest prefix
 * that could still have been valid, so one bad sequence gives one replacement character, as
 * recommended by the Unicode standard. A sequence cut short by the end of the buffer is handled
 * the same way.
 *
 * Never reads past s[len - 1]. Returns the number of bytes consumed: at least 1, so callers always
 * make progress, and at most `len`. With len == 0 it returns 0 and sets *cp to 0.
 */
size_t cydui_utf8_decode(const char *s, size_t len, uint32_t *cp);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_UTF8_H */
