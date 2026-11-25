/*
 * The compile-time contract of the public API (docs/11-api.md).
 *
 * Generated once by tools/gen_api_surface.py and then REVIEWED AND KEPT BY HAND: a change to a line
 * here is a change to the API, and needs a CHANGELOG entry, docs/11-api.md and the version bumped in
 * the same commit. Adding API means adding lines (run the script and keep only the new ones).
 *
 *   - every function is bound to a pointer of its exact type: a changed parameter or result stops the build
 *   - every enumerator and constant macro has the value it had when the API was frozen
 *   - every tunable macro is still there (its default value may change)
 *   - every field of every stable struct is still there with its type; structs without pointers keep their size
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cydui/types.h"
#include "cydui/arena.h"
#include "cydui/cmd.h"
#include "cydui/font.h"
#include "cydui/utf8.h"
#include "cydui/textedit.h"
#include "cydui/text.h"
#include "cydui/backend.h"
#include "cydui/cydui.h"
#include "cydui/anim.h"
#include "cydui/theme.h"
#include "cydui/ui.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define CT_ASSERT(name, cond) typedef char ct_##name[(cond) ? 1 : -1]

/* ---- functions ---- */

#define SURFACE(ret, name, params) static ret(*const surface_##name) params = name;

SURFACE(cydui_rect, cydui_alloc, (cydui_ui *ui, const cydui_mods *mods, int w, int h))
SURFACE(cydui_rect, cydui_alloc_rect, (cydui_ui *ui, int w, int h))
SURFACE(void *, cydui_arena_alloc, (cydui_arena *a, size_t size, size_t align))
SURFACE(void, cydui_arena_init, (cydui_arena *a, void *mem, size_t cap))
SURFACE(void, cydui_arena_reset, (cydui_arena *a))
SURFACE(void, cydui_begin_card, (cydui_ui *ui))
SURFACE(void, cydui_begin_column, (cydui_ui *ui))
SURFACE(void, cydui_begin_frame, (cydui_ctx *ctx, cydui_color clear))
SURFACE(void, cydui_begin_grid, (cydui_ui *ui, int cols))
SURFACE(void, cydui_begin_list_item, (cydui_ui *ui, int32_t index))
SURFACE(void, cydui_begin_padding, (cydui_ui *ui, int px))
SURFACE(void, cydui_begin_row, (cydui_ui *ui))
SURFACE(size_t, cydui_bitmap_size, (cydui_bitmap_format fmt, int16_t w, int16_t h))
SURFACE(cydui_color, cydui_blend, (cydui_color dst, cydui_color src, uint8_t alpha))
SURFACE(bool, cydui_button, (cydui_ui *ui, const char *label))
SURFACE(bool, cydui_checkbox, (cydui_ui *ui, const char *label, bool *value))
SURFACE(void, cydui_clear_errors, (cydui_ctx *ctx))
SURFACE(uint32_t, cydui_cmd_count, (const cydui_ctx *ctx))
SURFACE(void, cydui_color_unpack, (cydui_color c, uint8_t *r, uint8_t *g, uint8_t *b))
SURFACE(void, cydui_draw_bitmap, (cydui_ctx *ctx, cydui_rect r, const void *data, size_t len, cydui_bitmap_format fmt, cydui_color tint, uint8_t alpha))
SURFACE(void, cydui_draw_gradient, (cydui_ctx *ctx, cydui_rect r, cydui_color c0, cydui_color c1, bool vertical, uint8_t alpha))
SURFACE(void, cydui_draw_line, (cydui_ctx *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thickness, cydui_color color, uint8_t alpha))
SURFACE(void, cydui_draw_rect, (cydui_ctx *ctx, cydui_rect r, cydui_color color, uint8_t alpha))
SURFACE(void, cydui_draw_rrect, (cydui_ctx *ctx, cydui_rect r, uint8_t radius, cydui_color color, uint8_t alpha))
SURFACE(void, cydui_draw_rrect_outline, (cydui_ctx *ctx, cydui_rect r, uint8_t radius, uint8_t thickness, cydui_color color, uint8_t alpha))
SURFACE(void, cydui_draw_text, (cydui_ctx *ctx, const cydui_font *font, int16_t x, int16_t y, int16_t wrap_w, cydui_align align, const char *s, size_t len, cydui_color color, uint8_t alpha))
SURFACE(void, cydui_draw_text_in, (cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n, cydui_color color))
SURFACE(void, cydui_end_card, (cydui_ui *ui))
SURFACE(void, cydui_end_column, (cydui_ui *ui))
SURFACE(void, cydui_end_frame, (cydui_ctx *ctx))
SURFACE(void, cydui_end_grid, (cydui_ui *ui))
SURFACE(void, cydui_end_list_item, (cydui_ui *ui))
SURFACE(void, cydui_end_padding, (cydui_ui *ui))
SURFACE(void, cydui_end_row, (cydui_ui *ui))
SURFACE(uint32_t, cydui_errors, (const cydui_ctx *ctx))
SURFACE(const cydui_glyph *, cydui_font_fallback, (const cydui_font *f))
SURFACE(const uint8_t *, cydui_font_glyph_data, (const cydui_font *f, const cydui_glyph *g))
SURFACE(const cydui_glyph *, cydui_font_lookup, (const cydui_font *f, uint32_t cp))
SURFACE(cydui_font_status, cydui_font_validate, (const cydui_font *f))
SURFACE(size_t, cydui_glyph_bytes, (const cydui_glyph *g))
SURFACE(bool, cydui_id_claim, (cydui_ui *ui, cydui_id id))
SURFACE(bool, cydui_id_eq, (cydui_id a, cydui_id b))
SURFACE(cydui_id, cydui_id_str, (const cydui_ui *ui, const char *label))
SURFACE(uint32_t, cydui_init, (cydui_ctx *ctx, const cydui_config *cfg))
SURFACE(cydui_response, cydui_interact, (cydui_ui *ui, cydui_id id, cydui_rect r, unsigned flags))
SURFACE(void, cydui_invalidate, (cydui_ctx *ctx))
SURFACE(void, cydui_label, (cydui_ui *ui, const char *text))
SURFACE(size_t, cydui_label_len, (const char *label))
SURFACE(void, cydui_label_wrapped, (cydui_ui *ui, const char *text))
SURFACE(cydui_rect, cydui_layout_avail, (const cydui_ui *ui))
SURFACE(void, cydui_list_begin, (cydui_ui *ui, const char *id, int32_t count, int16_t item_h, int16_t view_h, int32_t *first, int32_t *last))
SURFACE(void, cydui_list_end, (cydui_ui *ui))
SURFACE(void, cydui_next_disabled, (cydui_ui *ui))
SURFACE(void, cydui_next_fill, (cydui_ui *ui))
SURFACE(void, cydui_next_height, (cydui_ui *ui, int px))
SURFACE(void, cydui_next_stretch, (cydui_ui *ui))
SURFACE(void, cydui_next_width, (cydui_ui *ui, int px))
SURFACE(void, cydui_paint, (cydui_ui *ui, const cydui_paint_info *info))
SURFACE(void, cydui_paint_default, (struct cydui_ui *ui, const cydui_theme *theme, const cydui_paint_info *info))
SURFACE(void, cydui_pop_id, (cydui_ui *ui))
SURFACE(void, cydui_progress, (cydui_ui *ui, int32_t value, int32_t max))
SURFACE(void, cydui_push_id_int, (cydui_ui *ui, int32_t v))
SURFACE(void, cydui_push_id_ptr, (cydui_ui *ui, const void *p))
SURFACE(void, cydui_push_id_str, (cydui_ui *ui, const char *s))
SURFACE(void, cydui_raster_strip, (const cydui_cmd *cmds, uint32_t n, cydui_color *buf, int16_t width, int16_t y0, int16_t h, cydui_color clear))
SURFACE(bool, cydui_rect_empty, (cydui_rect r))
SURFACE(cydui_rect, cydui_rect_intersect, (cydui_rect a, cydui_rect b))
SURFACE(void, cydui_reset_clip, (cydui_ctx *ctx))
SURFACE(void, cydui_scroll_begin, (cydui_ui *ui, const char *id, int16_t view_h))
SURFACE(void, cydui_scroll_end, (cydui_ui *ui))
SURFACE(void, cydui_scroll_to, (cydui_ui *ui, const char *id, int32_t y))
SURFACE(void, cydui_set_clip, (cydui_ctx *ctx, cydui_rect r))
SURFACE(void, cydui_set_cross_align, (cydui_ui *ui, cydui_place place))
SURFACE(void, cydui_set_main_align, (cydui_ui *ui, cydui_place place))
SURFACE(void, cydui_set_spacing, (cydui_ui *ui, int px))
SURFACE(uint32_t, cydui_set_theme, (cydui_ui *ui, const cydui_theme *t))
SURFACE(bool, cydui_slider, (cydui_ui *ui, const char *label, int32_t *value, int32_t min, int32_t max))
SURFACE(int, cydui_slider_knob_x, (cydui_rect track, int32_t value, int32_t min, int32_t max))
SURFACE(int32_t, cydui_slider_value_at, (cydui_rect track, int x, int32_t min, int32_t max))
SURFACE(void, cydui_spacer, (cydui_ui *ui, int px))
SURFACE(void, cydui_spacer_fill, (cydui_ui *ui))
SURFACE(cydui_state *, cydui_state_get, (cydui_ui *ui, cydui_id id, bool create))
SURFACE(cydui_widget_state, cydui_state_of, (bool disabled, bool pressed))
SURFACE(int16_t, cydui_strip_height, (const cydui_ctx *ctx))
SURFACE(cydui_style, cydui_style_default, (const cydui_font *font))
SURFACE(bool, cydui_tabs, (cydui_ui *ui, const char *const *labels, int count, int *selected))
SURFACE(cydui_mods, cydui_take_mods, (cydui_ui *ui))
SURFACE(size_t, cydui_text_clip_len, (const char *s, size_t len, bool *truncated))
SURFACE(void, cydui_text_iter_init, (cydui_text_iter *it, const cydui_font *f, const char *s, size_t len, int16_t wrap_w))
SURFACE(bool, cydui_text_next_line, (cydui_text_iter *it, cydui_text_line *line))
SURFACE(const cydui_glyph *, cydui_text_resolve, (const cydui_font *f, uint32_t cp))
SURFACE(cydui_text_extent, cydui_text_size, (const cydui_font *f, const char *s, size_t len, int16_t wrap_w))
SURFACE(void, cydui_theme_default, (cydui_theme *t, const cydui_font *font))
SURFACE(void, cydui_theme_derive_table, (cydui_theme *t))
SURFACE(const cydui_theme *, cydui_theme_of, (const cydui_ui *ui))
SURFACE(cydui_theme_status, cydui_theme_validate, (const cydui_theme *t))
SURFACE(bool, cydui_toggle, (cydui_ui *ui, const char *label, bool *value))
SURFACE(bool, cydui_ui_begin, (cydui_ui *ui, cydui_color clear))
SURFACE(void, cydui_ui_clear_errors, (cydui_ui *ui))
SURFACE(bool, cydui_ui_end, (cydui_ui *ui))
SURFACE(uint32_t, cydui_ui_errors, (const cydui_ui *ui))
SURFACE(uint32_t, cydui_ui_frame, (const cydui_ui *ui))
SURFACE(uint32_t, cydui_ui_init, (cydui_ui *ui, const cydui_ui_config *cfg))
SURFACE(int32_t, cydui_animate, (cydui_ui *ui, cydui_id id, int32_t from, int32_t target, uint16_t duration_ms, cydui_ease ease))
SURFACE(int32_t, cydui_ease_apply, (cydui_ease e, int32_t t))
SURFACE(int32_t, cydui_lerp, (int32_t a, int32_t b, int32_t k))
SURFACE(uint32_t, cydui_now, (const cydui_ui *ui))
SURFACE(cydui_wstyle, cydui_paint_look, (const cydui_theme *theme, const cydui_paint_info *info))
SURFACE(int32_t, cydui_paint_amount, (const cydui_paint_info *info))
SURFACE(void, cydui_set_post_effect, (cydui_ctx *ctx, const cydui_post_effect *fx))
SURFACE(void, cydui_set_effects_level, (cydui_ui *ui, unsigned level))
SURFACE(unsigned, cydui_effects_level, (const cydui_ui *ui))
SURFACE(unsigned, cydui_text_input, (cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, unsigned flags))
SURFACE(int, cydui_keyboard_top, (const cydui_ui *ui))
SURFACE(void, cydui_keyboard_enable, (cydui_ui *ui, bool on))
SURFACE(bool, cydui_popup_begin, (cydui_ui *ui, const char *label, bool *open, int w, int h, unsigned flags))
SURFACE(void, cydui_popup_end, (cydui_ui *ui))
SURFACE(unsigned, cydui_text_area, (cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, int rows, unsigned flags))
SURFACE(void, cydui_focus_set, (cydui_ui *ui, const char *label))
SURFACE(void, cydui_focus_clear, (cydui_ui *ui))
SURFACE(bool, cydui_has_focus, (const cydui_ui *ui))
SURFACE(size_t, cydui_textedit_len, (const char *buf, size_t cap))
SURFACE(size_t, cydui_textedit_chars, (const char *buf, size_t cap))
SURFACE(void, cydui_textedit_normalize, (char *buf, size_t cap, size_t *cursor))
SURFACE(unsigned, cydui_textedit_insert, (char *buf, size_t cap, size_t *cursor, uint32_t cp, unsigned flags, size_t max_chars))
SURFACE(unsigned, cydui_textedit_backspace, (char *buf, size_t cap, size_t *cursor))
SURFACE(unsigned, cydui_textedit_delete, (char *buf, size_t cap, size_t *cursor))
SURFACE(unsigned, cydui_textedit_move, (char *buf, size_t cap, size_t *cursor, cydui_move how, unsigned flags))
SURFACE(size_t, cydui_utf8_decode, (const char *s, size_t len, uint32_t *cp))
SURFACE(const char *, cydui_version, (void))
SURFACE(bool, cydui_widget_begin, (cydui_ui *ui))
SURFACE(const cydui_wstyle *, cydui_wstyle_of, (const cydui_ui *ui, cydui_widget_kind kind, cydui_widget_state state, bool selected))

/* ---- enumerators and constants ---- */

CT_ASSERT(CYDUI_ALIGN_CENTER, (long long)(CYDUI_ALIGN_CENTER) == 1LL);
CT_ASSERT(CYDUI_ALIGN_LEFT, (long long)(CYDUI_ALIGN_LEFT) == 0LL);
CT_ASSERT(CYDUI_ALIGN_RIGHT, (long long)(CYDUI_ALIGN_RIGHT) == 2LL);
CT_ASSERT(CYDUI_BITMAP_A4, (long long)(CYDUI_BITMAP_A4) == 1LL);
CT_ASSERT(CYDUI_BITMAP_RGB565, (long long)(CYDUI_BITMAP_RGB565) == 0LL);
CT_ASSERT(CYDUI_CAPTURE, (long long)(CYDUI_CAPTURE) == 8LL);
CT_ASSERT(CYDUI_CMD_BITMAP, (long long)(CYDUI_CMD_BITMAP) == 3LL);
CT_ASSERT(CYDUI_CMD_GRADIENT, (long long)(CYDUI_CMD_GRADIENT) == 4LL);
CT_ASSERT(CYDUI_CMD_LINE, (long long)(CYDUI_CMD_LINE) == 2LL);
CT_ASSERT(CYDUI_CMD_RECT, (long long)(CYDUI_CMD_RECT) == 0LL);
CT_ASSERT(CYDUI_CMD_RRECT, (long long)(CYDUI_CMD_RRECT) == 1LL);
CT_ASSERT(CYDUI_CMD_TEXT, (long long)(CYDUI_CMD_TEXT) == 5LL);
CT_ASSERT(CYDUI_DISABLED, (long long)(CYDUI_DISABLED) == 4LL);
CT_ASSERT(CYDUI_ERR_BAD_ARG, (long long)(CYDUI_ERR_BAD_ARG) == 2LL);
CT_ASSERT(CYDUI_ERR_BAD_BITMAP, (long long)(CYDUI_ERR_BAD_BITMAP) == 8LL);
CT_ASSERT(CYDUI_ERR_CMD_OVERFLOW, (long long)(CYDUI_ERR_CMD_OVERFLOW) == 1LL);
CT_ASSERT(CYDUI_ERR_FRAME_BUDGET, (long long)(CYDUI_ERR_FRAME_BUDGET) == 512LL);
CT_ASSERT(CYDUI_ERR_TWEEN_LIMIT, (long long)(CYDUI_ERR_TWEEN_LIMIT) == 1024LL);
CT_ASSERT(CYDUI_ANIM_ONE, (long long)(CYDUI_ANIM_ONE) == 4096LL);
CT_ASSERT(CYDUI_FX_NONE, (long long)(CYDUI_FX_NONE) == 0LL);
CT_ASSERT(CYDUI_FX_LOW, (long long)(CYDUI_FX_LOW) == 1LL);
CT_ASSERT(CYDUI_FX_FULL, (long long)(CYDUI_FX_FULL) == 2LL);
CT_ASSERT(CYDUI_EASE_LINEAR, (long long)(CYDUI_EASE_LINEAR) == 0LL);
CT_ASSERT(CYDUI_EASE_IN_QUAD, (long long)(CYDUI_EASE_IN_QUAD) == 1LL);
CT_ASSERT(CYDUI_EASE_OUT_QUAD, (long long)(CYDUI_EASE_OUT_QUAD) == 2LL);
CT_ASSERT(CYDUI_EASE_IN_OUT_QUAD, (long long)(CYDUI_EASE_IN_OUT_QUAD) == 3LL);
CT_ASSERT(CYDUI_EASE_IN_CUBIC, (long long)(CYDUI_EASE_IN_CUBIC) == 4LL);
CT_ASSERT(CYDUI_EASE_OUT_CUBIC, (long long)(CYDUI_EASE_OUT_CUBIC) == 5LL);
CT_ASSERT(CYDUI_EASE_IN_OUT_CUBIC, (long long)(CYDUI_EASE_IN_OUT_CUBIC) == 6LL);
CT_ASSERT(CYDUI_EASE_OUT_BACK, (long long)(CYDUI_EASE_OUT_BACK) == 7LL);
CT_ASSERT(CYDUI_EASE_OUT_ELASTIC, (long long)(CYDUI_EASE_OUT_ELASTIC) == 8LL);
CT_ASSERT(CYDUI_EASE_COUNT, (long long)(CYDUI_EASE_COUNT) == 9LL);
CT_ASSERT(CYDUI_ERR_ID_COLLISION, (long long)(CYDUI_ERR_ID_COLLISION) == 32LL);
CT_ASSERT(CYDUI_ERR_LAYOUT, (long long)(CYDUI_ERR_LAYOUT) == 64LL);
CT_ASSERT(CYDUI_ERR_NO_MEMORY, (long long)(CYDUI_ERR_NO_MEMORY) == 4LL);
CT_ASSERT(CYDUI_ERR_TEXT_TRUNCATED, (long long)(CYDUI_ERR_TEXT_TRUNCATED) == 16LL);
CT_ASSERT(CYDUI_ERR_THREAD, (long long)(CYDUI_ERR_THREAD) == 256LL);
CT_ASSERT(CYDUI_ERR_TOO_MANY_WIDGETS, (long long)(CYDUI_ERR_TOO_MANY_WIDGETS) == 128LL);
CT_ASSERT(CYDUI_EVENT_DOWN, (long long)(CYDUI_EVENT_DOWN) == 0LL);
CT_ASSERT(CYDUI_EVENT_MOVE, (long long)(CYDUI_EVENT_MOVE) == 1LL);
CT_ASSERT(CYDUI_EVENT_QUIT, (long long)(CYDUI_EVENT_QUIT) == 3LL);
CT_ASSERT(CYDUI_EVENT_UP, (long long)(CYDUI_EVENT_UP) == 2LL);
CT_ASSERT(CYDUI_KEY_NONE, (long long)(CYDUI_KEY_NONE) == 0LL);
CT_ASSERT(CYDUI_KEY_BACKSPACE, (long long)(CYDUI_KEY_BACKSPACE) == 1LL);
CT_ASSERT(CYDUI_KEY_DELETE, (long long)(CYDUI_KEY_DELETE) == 2LL);
CT_ASSERT(CYDUI_KEY_LEFT, (long long)(CYDUI_KEY_LEFT) == 3LL);
CT_ASSERT(CYDUI_KEY_RIGHT, (long long)(CYDUI_KEY_RIGHT) == 4LL);
CT_ASSERT(CYDUI_KEY_UP, (long long)(CYDUI_KEY_UP) == 5LL);
CT_ASSERT(CYDUI_KEY_DOWN, (long long)(CYDUI_KEY_DOWN) == 6LL);
CT_ASSERT(CYDUI_KEY_HOME, (long long)(CYDUI_KEY_HOME) == 7LL);
CT_ASSERT(CYDUI_KEY_END, (long long)(CYDUI_KEY_END) == 8LL);
CT_ASSERT(CYDUI_KEY_ENTER, (long long)(CYDUI_KEY_ENTER) == 9LL);
CT_ASSERT(CYDUI_KEY_ESCAPE, (long long)(CYDUI_KEY_ESCAPE) == 10LL);
CT_ASSERT(CYDUI_EVENT_CHAR, (long long)(CYDUI_EVENT_CHAR) == 4LL);
CT_ASSERT(CYDUI_EVENT_KEY, (long long)(CYDUI_EVENT_KEY) == 5LL);
CT_ASSERT(CYDUI_EDIT_MULTILINE, (long long)(CYDUI_EDIT_MULTILINE) == 1LL);
CT_ASSERT(CYDUI_EDIT_TEXT, (long long)(CYDUI_EDIT_TEXT) == 1LL);
CT_ASSERT(CYDUI_EDIT_CURSOR, (long long)(CYDUI_EDIT_CURSOR) == 2LL);
CT_ASSERT(CYDUI_EDIT_FULL, (long long)(CYDUI_EDIT_FULL) == 4LL);
CT_ASSERT(CYDUI_MOVE_LEFT, (long long)(CYDUI_MOVE_LEFT) == 0LL);
CT_ASSERT(CYDUI_MOVE_RIGHT, (long long)(CYDUI_MOVE_RIGHT) == 1LL);
CT_ASSERT(CYDUI_MOVE_HOME, (long long)(CYDUI_MOVE_HOME) == 2LL);
CT_ASSERT(CYDUI_MOVE_END, (long long)(CYDUI_MOVE_END) == 3LL);
CT_ASSERT(CYDUI_FONT_ERR_BITMAP, (long long)(CYDUI_FONT_ERR_BITMAP) == 4LL);
CT_ASSERT(CYDUI_FONT_ERR_EMPTY, (long long)(CYDUI_FONT_ERR_EMPTY) == 2LL);
CT_ASSERT(CYDUI_FONT_ERR_FALLBACK, (long long)(CYDUI_FONT_ERR_FALLBACK) == 5LL);
CT_ASSERT(CYDUI_FONT_ERR_METRICS, (long long)(CYDUI_FONT_ERR_METRICS) == 6LL);
CT_ASSERT(CYDUI_FONT_ERR_NULL, (long long)(CYDUI_FONT_ERR_NULL) == 1LL);
CT_ASSERT(CYDUI_FONT_ERR_ORDER, (long long)(CYDUI_FONT_ERR_ORDER) == 3LL);
CT_ASSERT(CYDUI_FONT_OK, (long long)(CYDUI_FONT_OK) == 0LL);
CT_ASSERT(CYDUI_LAYOUT_COLUMN, (long long)(CYDUI_LAYOUT_COLUMN) == 2LL);
CT_ASSERT(CYDUI_LAYOUT_GRID, (long long)(CYDUI_LAYOUT_GRID) == 3LL);
CT_ASSERT(CYDUI_LAYOUT_PADDING, (long long)(CYDUI_LAYOUT_PADDING) == 4LL);
CT_ASSERT(CYDUI_LAYOUT_ROOT, (long long)(CYDUI_LAYOUT_ROOT) == 0LL);
CT_ASSERT(CYDUI_LAYOUT_ROW, (long long)(CYDUI_LAYOUT_ROW) == 1LL);
CT_ASSERT(CYDUI_LAYOUT_SCROLL, (long long)(CYDUI_LAYOUT_SCROLL) == 5LL);
CT_ASSERT(CYDUI_PLACE_CENTER, (long long)(CYDUI_PLACE_CENTER) == 1LL);
CT_ASSERT(CYDUI_PLACE_END, (long long)(CYDUI_PLACE_END) == 2LL);
CT_ASSERT(CYDUI_PLACE_START, (long long)(CYDUI_PLACE_START) == 0LL);
CT_ASSERT(CYDUI_SENSE_CLICK, (long long)(CYDUI_SENSE_CLICK) == 1LL);
CT_ASSERT(CYDUI_SENSE_DRAG, (long long)(CYDUI_SENSE_DRAG) == 2LL);
CT_ASSERT(CYDUI_STATE_COUNT, (long long)(CYDUI_STATE_COUNT) == 5LL);
CT_ASSERT(CYDUI_STATE_DISABLED, (long long)(CYDUI_STATE_DISABLED) == 3LL);
CT_ASSERT(CYDUI_STATE_FOCUSED, (long long)(CYDUI_STATE_FOCUSED) == 4LL);
CT_ASSERT(CYDUI_STATE_HOVER, (long long)(CYDUI_STATE_HOVER) == 1LL);
CT_ASSERT(CYDUI_STATE_NORMAL, (long long)(CYDUI_STATE_NORMAL) == 0LL);
CT_ASSERT(CYDUI_STATE_PRESSED, (long long)(CYDUI_STATE_PRESSED) == 2LL);
CT_ASSERT(CYDUI_THEME_ERR_FONT, (long long)(CYDUI_THEME_ERR_FONT) == 2LL);
CT_ASSERT(CYDUI_THEME_ERR_NULL, (long long)(CYDUI_THEME_ERR_NULL) == 1LL);
CT_ASSERT(CYDUI_THEME_OK, (long long)(CYDUI_THEME_OK) == 0LL);
CT_ASSERT(CYDUI_WIDGET_BUTTON, (long long)(CYDUI_WIDGET_BUTTON) == 0LL);
CT_ASSERT(CYDUI_WIDGET_CHECKBOX, (long long)(CYDUI_WIDGET_CHECKBOX) == 1LL);
CT_ASSERT(CYDUI_WIDGET_COUNT, (long long)(CYDUI_WIDGET_COUNT) == 11LL);
CT_ASSERT(CYDUI_WIDGET_KEY, (long long)(CYDUI_WIDGET_KEY) == 10LL);
CT_ASSERT(CYDUI_WIDGET_POPUP, (long long)(CYDUI_WIDGET_POPUP) == 9LL);
CT_ASSERT(CYDUI_POPUP_MODAL, (long long)(CYDUI_POPUP_MODAL) == 1LL);
CT_ASSERT(CYDUI_WIDGET_TEXT_INPUT, (long long)(CYDUI_WIDGET_TEXT_INPUT) == 8LL);
CT_ASSERT(CYDUI_TEXT_PASSWORD, (long long)(CYDUI_TEXT_PASSWORD) == 256LL);
CT_ASSERT(CYDUI_TEXT_CHANGED, (long long)(CYDUI_TEXT_CHANGED) == 1LL);
CT_ASSERT(CYDUI_TEXT_CONFIRMED, (long long)(CYDUI_TEXT_CONFIRMED) == 2LL);
CT_ASSERT(CYDUI_WIDGET_PANEL, (long long)(CYDUI_WIDGET_PANEL) == 6LL);
CT_ASSERT(CYDUI_WIDGET_PROGRESS, (long long)(CYDUI_WIDGET_PROGRESS) == 4LL);
CT_ASSERT(CYDUI_WIDGET_SCROLLBAR, (long long)(CYDUI_WIDGET_SCROLLBAR) == 7LL);
CT_ASSERT(CYDUI_WIDGET_SLIDER, (long long)(CYDUI_WIDGET_SLIDER) == 3LL);
CT_ASSERT(CYDUI_WIDGET_TAB, (long long)(CYDUI_WIDGET_TAB) == 5LL);
CT_ASSERT(CYDUI_WIDGET_TOGGLE, (long long)(CYDUI_WIDGET_TOGGLE) == 2LL);
CT_ASSERT(CYDUI_DEBUG_IDS, (long long)(CYDUI_DEBUG_IDS) == 1LL);
CT_ASSERT(CYDUI_DEBUG_THREAD, (long long)(CYDUI_DEBUG_THREAD) == 1LL);
CT_ASSERT(CYDUI_LIST_MAX_PX, (long long)(CYDUI_LIST_MAX_PX) == 4194304LL);
CT_ASSERT(CYDUI_NTS, (long long)(CYDUI_NTS) == -1LL);
CT_ASSERT(CYDUI_REPLACEMENT_CHAR, (long long)(CYDUI_REPLACEMENT_CHAR) == 65533LL);
CT_ASSERT(CYDUI_SLIDER_KNOB, (long long)(CYDUI_SLIDER_KNOB) == 20LL);
CT_ASSERT(CYDUI_SLIDER_TRACK, (long long)(CYDUI_SLIDER_TRACK) == 6LL);

/* ---- tunable macros: the name is stable, the default may change ---- */

#ifndef CYDUI_EVENTS_PER_FRAME
#error "CYDUI_EVENTS_PER_FRAME is part of the API"
#endif
#ifndef CYDUI_EVENT_QUEUE
#error "CYDUI_EVENT_QUEUE is part of the API"
#endif
#ifndef CYDUI_KEY_QUEUE
#error "CYDUI_KEY_QUEUE is part of the API"
#endif
#ifndef CYDUI_FILTER_CONTROL
#error "CYDUI_FILTER_CONTROL is part of the API"
#endif
#ifndef CYDUI_MAX_ID_DEPTH
#error "CYDUI_MAX_ID_DEPTH is part of the API"
#endif
#ifndef CYDUI_MAX_LAYOUT_DEPTH
#error "CYDUI_MAX_LAYOUT_DEPTH is part of the API"
#endif
#ifndef CYDUI_MAX_STR
#error "CYDUI_MAX_STR is part of the API"
#endif
#ifndef CYDUI_MAX_TABS
#error "CYDUI_MAX_TABS is part of the API"
#endif
#ifndef CYDUI_MAX_WIDGETS
#error "CYDUI_MAX_WIDGETS is part of the API"
#endif
#ifndef CYDUI_FX_DEFAULT
#error "CYDUI_FX_DEFAULT is part of the API"
#endif
#ifndef CYDUI_MAX_TWEENS
#error "CYDUI_MAX_TWEENS is part of the API"
#endif
#ifndef CYDUI_ANIM_MAX_MS
#error "CYDUI_ANIM_MAX_MS is part of the API"
#endif
#ifndef CYDUI_POLL_PER_FRAME
#error "CYDUI_POLL_PER_FRAME is part of the API"
#endif
#ifndef CYDUI_SCROLL_FLING_MIN
#error "CYDUI_SCROLL_FLING_MIN is part of the API"
#endif
#ifndef CYDUI_SCROLL_FRICTION_MS
#error "CYDUI_SCROLL_FRICTION_MS is part of the API"
#endif
#ifndef CYDUI_SCROLL_MAX_V
#error "CYDUI_SCROLL_MAX_V is part of the API"
#endif
#ifndef CYDUI_SCROLL_MIN_V
#error "CYDUI_SCROLL_MIN_V is part of the API"
#endif
#ifndef CYDUI_STATE_TTL
#error "CYDUI_STATE_TTL is part of the API"
#endif
#ifndef CYDUI_VERSION_MAJOR
#error "CYDUI_VERSION_MAJOR is part of the API"
#endif
#ifndef CYDUI_VERSION_MINOR
#error "CYDUI_VERSION_MINOR is part of the API"
#endif
#ifndef CYDUI_VERSION_PATCH
#error "CYDUI_VERSION_PATCH is part of the API"
#endif

/* ---- function-like macros ---- */

#ifndef CYDUI_CARD
#error "CYDUI_CARD is part of the API"
#endif
#ifndef CYDUI_COLUMN
#error "CYDUI_COLUMN is part of the API"
#endif
#ifndef CYDUI_CONFIG
#error "CYDUI_CONFIG is part of the API"
#endif
#ifndef CYDUI_GRID
#error "CYDUI_GRID is part of the API"
#endif
#ifndef CYDUI_PADDING
#error "CYDUI_PADDING is part of the API"
#endif
#ifndef CYDUI_RGB
#error "CYDUI_RGB is part of the API"
#endif
#ifndef CYDUI_ROW
#error "CYDUI_ROW is part of the API"
#endif
#ifndef CYDUI_SCROLL
#error "CYDUI_SCROLL is part of the API"
#endif
#ifndef CYDUI_STORAGE
#error "CYDUI_STORAGE is part of the API"
#endif
#ifndef CYDUI_TEXT_REACH
#error "CYDUI_TEXT_REACH is part of the API"
#endif

/* ---- structs: fields and types ---- */

static void structs_have_their_fields(void)
{
    {
        cydui_backend v;
        (void)v;
        { void * *p = &v.user; (void)p; }
        { int16_t *p = &v.width; (void)p; }
        { int16_t *p = &v.height; (void)p; }
        (void)sizeof(v.flush_strip);
        (void)sizeof(v.poll_event);
        (void)sizeof(v.now_ms);
        (void)sizeof(v.frame_end);
    }
    {
        cydui_cmd v;
        (void)v;
        { uint8_t *p = &v.type; (void)p; }
        { uint8_t *p = &v.alpha; (void)p; }
        { cydui_rect *p = &v.rect; (void)p; }
        { cydui_rect *p = &v.clip; (void)p; }
        (void)sizeof(v.u);
    }
    {
        cydui_config v;
        (void)v;
        { const cydui_backend * *p = &v.backend; (void)p; }
        { void * *p = &v.cmd_mem; (void)p; }
        { size_t *p = &v.cmd_mem_size; (void)p; }
        { void * *p = &v.strip_mem; (void)p; }
        { size_t *p = &v.strip_mem_size; (void)p; }
        { void * *p = &v.text_mem; (void)p; }
        { size_t *p = &v.text_mem_size; (void)p; }
        { uint8_t *p = &v.strip_buffers; (void)p; }
        { void * *p = &v.dirty_mem; (void)p; }
        { size_t *p = &v.dirty_mem_size; (void)p; }
    }
    {
        cydui_event v;
        (void)v;
        { cydui_event_type *p = &v.type; (void)p; }
        { int16_t *p = &v.x; (void)p; }
        { int16_t *p = &v.y; (void)p; }
        { uint32_t *p = &v.key; (void)p; }
    }
    {
        cydui_font v;
        (void)v;
        { const cydui_glyph * *p = &v.glyphs; (void)p; }
        { uint16_t *p = &v.glyph_count; (void)p; }
        { const uint8_t * *p = &v.bitmap; (void)p; }
        { uint32_t *p = &v.bitmap_len; (void)p; }
        { uint8_t *p = &v.line_height; (void)p; }
        { uint8_t *p = &v.ascent; (void)p; }
        { uint16_t *p = &v.fallback; (void)p; }
    }
    {
        cydui_glyph v;
        (void)v;
        { uint32_t *p = &v.cp; (void)p; }
        { uint32_t *p = &v.offset; (void)p; }
        { uint8_t *p = &v.w; (void)p; }
        { uint8_t *p = &v.h; (void)p; }
        { int8_t *p = &v.left; (void)p; }
        { int8_t *p = &v.top; (void)p; }
        { uint8_t *p = &v.advance; (void)p; }
    }
    {
        cydui_id v;
        (void)v;
        { uint32_t *p = &v.h; (void)p; }
        { uint16_t *p = &v.c; (void)p; }
    }
    {
        cydui_mods v;
        (void)v;
        { int16_t *p = &v.w; (void)p; }
        { int16_t *p = &v.h; (void)p; }
        { bool *p = &v.fill; (void)p; }
        { bool *p = &v.stretch; (void)p; }
        { bool *p = &v.disabled; (void)p; }
    }
    {
        cydui_paint_info v;
        (void)v;
        { cydui_widget_kind *p = &v.kind; (void)p; }
        { cydui_widget_state *p = &v.state; (void)p; }
        { bool *p = &v.selected; (void)p; }
        { cydui_rect *p = &v.rect; (void)p; }
        { const char * *p = &v.text; (void)p; }
        { size_t *p = &v.text_len; (void)p; }
        { cydui_text_extent *p = &v.text_extent; (void)p; }
        { int32_t *p = &v.value; (void)p; }
        { int32_t *p = &v.min; (void)p; }
        { int32_t *p = &v.max; (void)p; }
        { cydui_rect *p = &v.aux; (void)p; }
        { uint16_t *p = &v.mix; (void)p; }
        { cydui_widget_state *p = &v.from_state; (void)p; }
        { bool *p = &v.from_selected; (void)p; }
    }
    {
        cydui_rect v;
        (void)v;
        { int16_t *p = &v.x; (void)p; }
        { int16_t *p = &v.y; (void)p; }
        { int16_t *p = &v.w; (void)p; }
        { int16_t *p = &v.h; (void)p; }
    }
    {
        cydui_response v;
        (void)v;
        { bool *p = &v.pressed; (void)p; }
        { bool *p = &v.clicked; (void)p; }
        { bool *p = &v.long_pressed; (void)p; }
        { bool *p = &v.dragging; (void)p; }
        { bool *p = &v.released; (void)p; }
        { int16_t *p = &v.dx; (void)p; }
        { int16_t *p = &v.dy; (void)p; }
        { int16_t *p = &v.x; (void)p; }
        { int16_t *p = &v.y; (void)p; }
    }
    {
        cydui_state v;
        (void)v;
        { cydui_id *p = &v.id; (void)p; }
        { uint32_t *p = &v.last_frame; (void)p; }
        { int32_t *p = &v.a; (void)p; }
        { int32_t *p = &v.b; (void)p; }
        { int32_t *p = &v.c; (void)p; }
        { int32_t *p = &v.d; (void)p; }
        { uint16_t *p = &v.used; (void)p; }
    }
    {
        cydui_style v;
        (void)v;
        { cydui_color *p = &v.bg; (void)p; }
        { cydui_color *p = &v.panel; (void)p; }
        { cydui_color *p = &v.text; (void)p; }
        { cydui_color *p = &v.text_muted; (void)p; }
        { cydui_color *p = &v.accent; (void)p; }
        { cydui_color *p = &v.accent_text; (void)p; }
        { cydui_color *p = &v.border; (void)p; }
        { const cydui_font * *p = &v.font; (void)p; }
        { uint8_t *p = &v.radius; (void)p; }
        { uint8_t *p = &v.pad_x; (void)p; }
        { uint8_t *p = &v.pad_y; (void)p; }
        { uint8_t *p = &v.spacing; (void)p; }
        { uint8_t *p = &v.min_target; (void)p; }
        { uint16_t *p = &v.anim_ms; (void)p; }
    }
    {
        cydui_text_extent v;
        (void)v;
        { int16_t *p = &v.w; (void)p; }
        { int16_t *p = &v.h; (void)p; }
        { uint16_t *p = &v.lines; (void)p; }
    }
    {
        cydui_text_iter v;
        (void)v;
        { const cydui_font * *p = &v.font; (void)p; }
        { const char * *p = &v.s; (void)p; }
        { size_t *p = &v.len; (void)p; }
        { int *p = &v.wrap; (void)p; }
        { size_t *p = &v.pos; (void)p; }
    }
    {
        cydui_text_line v;
        (void)v;
        { size_t *p = &v.start; (void)p; }
        { size_t *p = &v.end; (void)p; }
        { int *p = &v.width; (void)p; }
    }
    {
        cydui_theme v;
        (void)v;
        { const char * *p = &v.name; (void)p; }
        { cydui_style *p = &v.style; (void)p; }
        (void)sizeof(v.table);
        (void)sizeof(v.paint);
        { const cydui_post_effect * *p = &v.post; (void)p; }
    }
    {
        cydui_post_effect v;
        (void)v;
        { uint32_t (**p)(void *, int16_t, int16_t) = &v.key; (void)p; }
        { void (**p)(void *, cydui_color *, int16_t, int16_t, int16_t) = &v.apply; (void)p; }
        { void * *p = &v.user; (void)p; }
    }
    {
        cydui_ui_config v;
        (void)v;
        { cydui_config *p = &v.core; (void)p; }
        { void * *p = &v.state_mem; (void)p; }
        { size_t *p = &v.state_mem_size; (void)p; }
        { void * *p = &v.idset_mem; (void)p; }
        { size_t *p = &v.idset_mem_size; (void)p; }
        { const cydui_theme * *p = &v.theme; (void)p; }
        { int16_t *p = &v.tap_slop; (void)p; }
        { uint16_t *p = &v.long_press_ms; (void)p; }
        (void)sizeof(v.thread_id);
        { uint16_t *p = &v.frame_budget_ms; (void)p; }
    }
    {
        cydui_wstyle v;
        (void)v;
        { cydui_color *p = &v.fill; (void)p; }
        { cydui_color *p = &v.border; (void)p; }
        { cydui_color *p = &v.text; (void)p; }
        { cydui_color *p = &v.accent; (void)p; }
        { uint8_t *p = &v.border_w; (void)p; }
        { uint8_t *p = &v.radius; (void)p; }
        { uint8_t *p = &v.shadow; (void)p; }
    }
}

CT_ASSERT(sizeof_cydui_event, sizeof(cydui_event) == 12);
CT_ASSERT(sizeof_cydui_glyph, sizeof(cydui_glyph) == 16);
CT_ASSERT(sizeof_cydui_id, sizeof(cydui_id) == 8);
CT_ASSERT(sizeof_cydui_mods, sizeof(cydui_mods) == 8);
CT_ASSERT(sizeof_cydui_rect, sizeof(cydui_rect) == 8);
CT_ASSERT(sizeof_cydui_response, sizeof(cydui_response) == 14);
CT_ASSERT(sizeof_cydui_state, sizeof(cydui_state) == 32);
CT_ASSERT(sizeof_cydui_text_extent, sizeof(cydui_text_extent) == 6);
CT_ASSERT(sizeof_cydui_wstyle, sizeof(cydui_wstyle) == 12);

/* ---- run ---- */

static void test_every_function_is_there(void)
{
    TEST_ASSERT_NOT_NULL(surface_cydui_alloc);
    TEST_ASSERT_NOT_NULL(surface_cydui_alloc_rect);
    TEST_ASSERT_NOT_NULL(surface_cydui_arena_alloc);
    TEST_ASSERT_NOT_NULL(surface_cydui_arena_init);
    TEST_ASSERT_NOT_NULL(surface_cydui_arena_reset);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_card);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_column);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_frame);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_grid);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_list_item);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_padding);
    TEST_ASSERT_NOT_NULL(surface_cydui_begin_row);
    TEST_ASSERT_NOT_NULL(surface_cydui_bitmap_size);
    TEST_ASSERT_NOT_NULL(surface_cydui_blend);
    TEST_ASSERT_NOT_NULL(surface_cydui_button);
    TEST_ASSERT_NOT_NULL(surface_cydui_checkbox);
    TEST_ASSERT_NOT_NULL(surface_cydui_clear_errors);
    TEST_ASSERT_NOT_NULL(surface_cydui_cmd_count);
    TEST_ASSERT_NOT_NULL(surface_cydui_color_unpack);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_bitmap);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_gradient);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_line);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_rect);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_rrect);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_rrect_outline);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_text);
    TEST_ASSERT_NOT_NULL(surface_cydui_draw_text_in);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_card);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_column);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_frame);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_grid);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_list_item);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_padding);
    TEST_ASSERT_NOT_NULL(surface_cydui_end_row);
    TEST_ASSERT_NOT_NULL(surface_cydui_errors);
    TEST_ASSERT_NOT_NULL(surface_cydui_font_fallback);
    TEST_ASSERT_NOT_NULL(surface_cydui_font_glyph_data);
    TEST_ASSERT_NOT_NULL(surface_cydui_font_lookup);
    TEST_ASSERT_NOT_NULL(surface_cydui_font_validate);
    TEST_ASSERT_NOT_NULL(surface_cydui_glyph_bytes);
    TEST_ASSERT_NOT_NULL(surface_cydui_id_claim);
    TEST_ASSERT_NOT_NULL(surface_cydui_id_eq);
    TEST_ASSERT_NOT_NULL(surface_cydui_id_str);
    TEST_ASSERT_NOT_NULL(surface_cydui_init);
    TEST_ASSERT_NOT_NULL(surface_cydui_interact);
    TEST_ASSERT_NOT_NULL(surface_cydui_invalidate);
    TEST_ASSERT_NOT_NULL(surface_cydui_label);
    TEST_ASSERT_NOT_NULL(surface_cydui_label_len);
    TEST_ASSERT_NOT_NULL(surface_cydui_label_wrapped);
    TEST_ASSERT_NOT_NULL(surface_cydui_layout_avail);
    TEST_ASSERT_NOT_NULL(surface_cydui_list_begin);
    TEST_ASSERT_NOT_NULL(surface_cydui_list_end);
    TEST_ASSERT_NOT_NULL(surface_cydui_next_disabled);
    TEST_ASSERT_NOT_NULL(surface_cydui_next_fill);
    TEST_ASSERT_NOT_NULL(surface_cydui_next_height);
    TEST_ASSERT_NOT_NULL(surface_cydui_next_stretch);
    TEST_ASSERT_NOT_NULL(surface_cydui_next_width);
    TEST_ASSERT_NOT_NULL(surface_cydui_paint);
    TEST_ASSERT_NOT_NULL(surface_cydui_paint_default);
    TEST_ASSERT_NOT_NULL(surface_cydui_pop_id);
    TEST_ASSERT_NOT_NULL(surface_cydui_progress);
    TEST_ASSERT_NOT_NULL(surface_cydui_push_id_int);
    TEST_ASSERT_NOT_NULL(surface_cydui_push_id_ptr);
    TEST_ASSERT_NOT_NULL(surface_cydui_push_id_str);
    TEST_ASSERT_NOT_NULL(surface_cydui_raster_strip);
    TEST_ASSERT_NOT_NULL(surface_cydui_rect_empty);
    TEST_ASSERT_NOT_NULL(surface_cydui_rect_intersect);
    TEST_ASSERT_NOT_NULL(surface_cydui_reset_clip);
    TEST_ASSERT_NOT_NULL(surface_cydui_scroll_begin);
    TEST_ASSERT_NOT_NULL(surface_cydui_scroll_end);
    TEST_ASSERT_NOT_NULL(surface_cydui_scroll_to);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_clip);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_cross_align);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_main_align);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_spacing);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_theme);
    TEST_ASSERT_NOT_NULL(surface_cydui_slider);
    TEST_ASSERT_NOT_NULL(surface_cydui_slider_knob_x);
    TEST_ASSERT_NOT_NULL(surface_cydui_slider_value_at);
    TEST_ASSERT_NOT_NULL(surface_cydui_spacer);
    TEST_ASSERT_NOT_NULL(surface_cydui_spacer_fill);
    TEST_ASSERT_NOT_NULL(surface_cydui_state_get);
    TEST_ASSERT_NOT_NULL(surface_cydui_state_of);
    TEST_ASSERT_NOT_NULL(surface_cydui_strip_height);
    TEST_ASSERT_NOT_NULL(surface_cydui_style_default);
    TEST_ASSERT_NOT_NULL(surface_cydui_tabs);
    TEST_ASSERT_NOT_NULL(surface_cydui_take_mods);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_clip_len);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_iter_init);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_next_line);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_resolve);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_size);
    TEST_ASSERT_NOT_NULL(surface_cydui_theme_default);
    TEST_ASSERT_NOT_NULL(surface_cydui_theme_derive_table);
    TEST_ASSERT_NOT_NULL(surface_cydui_theme_of);
    TEST_ASSERT_NOT_NULL(surface_cydui_theme_validate);
    TEST_ASSERT_NOT_NULL(surface_cydui_toggle);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_begin);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_clear_errors);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_end);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_errors);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_frame);
    TEST_ASSERT_NOT_NULL(surface_cydui_ui_init);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_post_effect);
    TEST_ASSERT_NOT_NULL(surface_cydui_set_effects_level);
    TEST_ASSERT_NOT_NULL(surface_cydui_effects_level);
    TEST_ASSERT_NOT_NULL(surface_cydui_animate);
    TEST_ASSERT_NOT_NULL(surface_cydui_ease_apply);
    TEST_ASSERT_NOT_NULL(surface_cydui_lerp);
    TEST_ASSERT_NOT_NULL(surface_cydui_now);
    TEST_ASSERT_NOT_NULL(surface_cydui_paint_look);
    TEST_ASSERT_NOT_NULL(surface_cydui_paint_amount);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_input);
    TEST_ASSERT_NOT_NULL(surface_cydui_keyboard_top);
    TEST_ASSERT_NOT_NULL(surface_cydui_keyboard_enable);
    TEST_ASSERT_NOT_NULL(surface_cydui_popup_begin);
    TEST_ASSERT_NOT_NULL(surface_cydui_popup_end);
    TEST_ASSERT_NOT_NULL(surface_cydui_text_area);
    TEST_ASSERT_NOT_NULL(surface_cydui_focus_set);
    TEST_ASSERT_NOT_NULL(surface_cydui_focus_clear);
    TEST_ASSERT_NOT_NULL(surface_cydui_has_focus);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_len);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_chars);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_normalize);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_insert);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_backspace);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_delete);
    TEST_ASSERT_NOT_NULL(surface_cydui_textedit_move);
    TEST_ASSERT_NOT_NULL(surface_cydui_utf8_decode);
    TEST_ASSERT_NOT_NULL(surface_cydui_version);
    TEST_ASSERT_NOT_NULL(surface_cydui_widget_begin);
    TEST_ASSERT_NOT_NULL(surface_cydui_wstyle_of);
}

static void test_the_version_agrees_with_itself(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d.%d.%d", CYDUI_VERSION_MAJOR, CYDUI_VERSION_MINOR, CYDUI_VERSION_PATCH);
    TEST_ASSERT_EQUAL_STRING(CYDUI_VERSION_STRING, buf);
    TEST_ASSERT_EQUAL_STRING(CYDUI_VERSION_STRING, cydui_version());
}

static void test_the_structs_are_still_what_they_were(void)
{
    structs_have_their_fields();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_function_is_there);
    RUN_TEST(test_the_version_agrees_with_itself);
    RUN_TEST(test_the_structs_are_still_what_they_were);
    return UNITY_END();
}
