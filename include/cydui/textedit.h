#ifndef CYDUI_TEXTEDIT_H
#define CYDUI_TEXTEDIT_H

/*
 * Editing of a UTF-8 string in a buffer of the application, without any UI: what the text widgets do with
 * the keys they receive. The buffer is `buf` of `cap` bytes and holds a NUL terminated string; the cursor is
 * a byte offset that is always on the boundary of a character. Every function is safe for any content of
 * the buffer (no NUL, ill-formed UTF-8) and for any cursor: they never read or write outside `cap` bytes,
 * and with `cap` 0 they do nothing. Nothing allocates.
 *
 * "A character" is what cydui_utf8_decode consumes: an ill-formed sequence is one character (it is drawn as
 * the fallback glyph), so the cursor and the deletion agree with what is on the screen.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Flags of the editing functions. */
#define CYDUI_EDIT_MULTILINE 1u /**< '\n' may be inserted, and Home / End go to the start / end of the line */

/** What an editing function did (a bit mask). */
#define CYDUI_EDIT_TEXT 1u   /**< the text changed */
#define CYDUI_EDIT_CURSOR 2u /**< the cursor moved */
#define CYDUI_EDIT_FULL 4u   /**< nothing was inserted because of the size of the buffer or the limit of characters */

typedef enum {
    CYDUI_MOVE_LEFT,
    CYDUI_MOVE_RIGHT,
    CYDUI_MOVE_HOME, /**< start of the text, or of the line with CYDUI_EDIT_MULTILINE */
    CYDUI_MOVE_END   /**< end of the text, or of the line with CYDUI_EDIT_MULTILINE */
} cydui_move;

/** Bytes of the string in `buf`: up to the NUL, at most cap - 1. */
size_t cydui_textedit_len(const char *buf, size_t cap);

/** Characters of the string in `buf`. */
size_t cydui_textedit_chars(const char *buf, size_t cap);

/**
 * Makes the buffer safe to edit: a NUL is guaranteed inside it (the last byte is overwritten if there is
 * none) and the cursor is brought into the text and onto the start of a character. Call it for a buffer
 * the application filled; the other functions call it themselves.
 */
void cydui_textedit_normalize(char *buf, size_t cap, size_t *cursor);

/**
 * Inserts the code point `cp` at the cursor and puts the cursor after it. Returns CYDUI_EDIT_TEXT |
 * CYDUI_EDIT_CURSOR; or CYDUI_EDIT_FULL when it does not fit in `cap` bytes or the text already has
 * `max_chars` characters (0 = no limit); or 0 when `cp` is not accepted and nothing happens: not a Unicode
 * scalar value, a control character (the newline is accepted with CYDUI_EDIT_MULTILINE), or a
 * bidirectional formatting character (as in the drawing of text, CYDUI_FILTER_CONTROL).
 */
unsigned cydui_textedit_insert(char *buf, size_t cap, size_t *cursor, uint32_t cp, unsigned flags, size_t max_chars);

/** Deletes the character before the cursor (CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR), or returns 0 at the start. */
unsigned cydui_textedit_backspace(char *buf, size_t cap, size_t *cursor);

/** Deletes the character at the cursor (CYDUI_EDIT_TEXT), or returns 0 at the end. */
unsigned cydui_textedit_delete(char *buf, size_t cap, size_t *cursor);

/** Moves the cursor; returns CYDUI_EDIT_CURSOR when it moved and 0 when it was already there. */
unsigned cydui_textedit_move(char *buf, size_t cap, size_t *cursor, cydui_move how, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_TEXTEDIT_H */
