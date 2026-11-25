/*
 * The keyboard on the screen (FR-34). Built at the end of the frame, after everything else, while a text field has the
 * focus: its keys are handled with cydui_interact like any widget (so the press is theirs) and what they produce goes to
 * the queue of keys that the field reads in the next frame. Rows are strings of UTF-8 characters; a few keys are special.
 */

#include <string.h>

#include "cydui/ui.h"
#include "cydui/utf8.h"
#include "ui_internal.h"

#define KEY_H 32
#define KEY_GAP 3
#define KB_PAD 3
#define KB_ROWS 4
#define KB_H (2 * KB_PAD + KB_ROWS * KEY_H + (KB_ROWS - 1) * KEY_GAP)
#define MAX_KEYS 12
#define REPEAT_DELAY_MS 500
#define REPEAT_EVERY_MS 80
#define MAX_OUT 8 /* events one frame can produce (a lot of repeats after a slow one are dropped) */

enum { P_LOWER, P_ACCENT, P_NUM, P_SYM, P_COUNT };
enum { K_CHAR, K_SHIFT, K_BACK, K_PAGE, K_SPACE, K_ENTER, K_HIDE };

typedef struct {
    char label[5]; /* what is written on the key, UTF-8 */
    uint8_t kind;
    uint8_t weight; /* relative width */
    uint8_t page;   /* K_PAGE: where it goes */
    uint32_t cp;    /* K_CHAR: the character, before Shift */
} key_def;

static const char *const rows[P_COUNT][3] = {
    {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
    {"\xC3\xA1\xC3\xA0\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA9\xC3\xA8\xC3\xAA\xC3\xAB\xC3\xAD", /* á à â ã ä é è ê ë í */
     "\xC3\xAC\xC3\xAE\xC3\xAF\xC3\xB3\xC3\xB2\xC3\xB4\xC3\xB5\xC3\xB6\xC3\xBA\xC3\xB9",  /* ì î ï ó ò ô õ ö ú ù */
     "\xC3\xBB\xC3\xBC\xC3\xA7\xC3\xB1\xC3\xBD"},                                         /* û ü ç ñ ý */
    {"1234567890", "-/:;()$&@\"", ".,?!'"},
    {"[]{}#%^*+=", "_\\|~<>\xC2\xA3\xC2\xA5\xC2\xA7", ".,?!'"}, /* £ ¥ § */
};

static key_def mk(const char *label, uint8_t kind, uint8_t weight, uint8_t page, uint32_t cp)
{
    key_def k;
    size_t n = strlen(label);

    memset(&k, 0, sizeof(k));
    memcpy(k.label, label, n < sizeof(k.label) - 1 ? n : sizeof(k.label) - 1); /* the last byte stays the NUL */
    k.kind = kind;
    k.weight = weight;
    k.page = page;
    k.cp = cp;
    return k;
}

/* Shift on a letter: ASCII and the accented letters of Latin-1 (except the division sign) go up by 32. */
static uint32_t shifted(uint32_t cp)
{
    if ((cp >= 'a' && cp <= 'z') || (cp >= 0xE0u && cp <= 0xFEu && cp != 0xF7u)) {
        return cp - 32u;
    }
    return cp;
}

static size_t encode(uint32_t cp, char out[5])
{
    size_t n;

    if (cp < 0x80u) {
        out[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        n = 2;
    } else {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        n = 3;
    }
    out[n] = '\0';
    return n;
}

static int add_chars(key_def *out, int n, const char *s, bool shift)
{
    size_t len = strlen(s), i = 0;

    while (i < len && n < MAX_KEYS) {
        uint32_t cp;
        char lab[5];

        i += cydui_utf8_decode(s + i, len - i, &cp);
        if (shift) {
            cp = shifted(cp);
        }
        (void)encode(cp, lab);
        out[n++] = mk(lab, K_CHAR, 2, 0, cp);
    }
    return n;
}

static int build_row(const cydui_ui *ui, int row, key_def *out)
{
    unsigned page = ui->kb_page < P_COUNT ? ui->kb_page : 0u;
    bool letters = page == P_LOWER || page == P_ACCENT;
    int n = 0;

    if (row < 2) {
        return add_chars(out, 0, rows[page][row], letters && ui->kb_shift);
    }
    if (row == 2) {
        if (letters) {
            out[n++] = mk("\xE2\x86\x91", K_SHIFT, 3, 0, 0); /* ↑ */
        } else {
            out[n++] = mk(page == P_NUM ? "#+=" : "123", K_PAGE, 3, page == P_NUM ? P_SYM : P_NUM, 0);
        }
        n = add_chars(out, n, rows[page][2], letters && ui->kb_shift);
        out[n++] = mk("\xE2\x86\x90", K_BACK, 3, 0, 0); /* ← */
        return n;
    }
    out[n++] = page == P_NUM || page == P_SYM ? mk("abc", K_PAGE, 3, P_LOWER, 0) : mk("123", K_PAGE, 3, P_NUM, 0);
    out[n++] = page == P_ACCENT ? mk("abc", K_PAGE, 3, P_LOWER, 0) : mk("\xC3\xA1\xC3\xA7", K_PAGE, 3, P_ACCENT, 0); /* áç */
    out[n++] = mk("", K_SPACE, 10, 0, ' ');
    out[n++] = mk("Ok", K_ENTER, 4, 0, 0);
    out[n++] = mk("\xE2\x86\x93", K_HIDE, 3, 0, 0); /* ↓ */
    return n;
}

static cydui_rect kb_rect(const cydui_backend *be)
{
    return (cydui_rect){0, (int16_t)(be->height - KB_H), be->width, KB_H};
}

int cydui_keyboard_top(const cydui_ui *ui)
{
    const cydui_backend *be = ui->ctx.backend;

    return ui->kb_prev ? be->height - KB_H : be->height;
}

void cydui_keyboard_enable(cydui_ui *ui, bool on)
{
    cydui__check_thread(ui);
    ui->kb_off = !on;
}

int cydui__keyboard_top(const cydui_ui *ui)
{
    return ui->ctx.backend->height - KB_H;
}

bool cydui__keyboard_hit(const cydui_ui *ui, int x, int y)
{
    cydui_rect r = kb_rect(ui->ctx.backend);

    return ui->kb_prev && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static bool is_active_toggle(const cydui_ui *ui, const key_def *k)
{
    return (k->kind == K_SHIFT && ui->kb_shift) || (k->kind == K_PAGE && k->page == ui->kb_page);
}

/* Handles and draws the keyboard; the events its keys produced go to `out` (up to MAX_OUT). Returns how many. */
unsigned cydui__keyboard_run(cydui_ui *ui, cydui_event *out)
{
    const cydui_backend *be = ui->ctx.backend;
    const cydui_style *st = &ui->theme->style;
    const cydui_input *in = &ui->in;
    const cydui_rect kb = kb_rect(be);
    const cydui_wstyle *bg = cydui_wstyle_of(ui, CYDUI_WIDGET_POPUP, CYDUI_STATE_NORMAL, false);
    bool saved_popup = ui->in_popup;
    unsigned n_out = 0;
    int row;

    ui->in_kb = ui->in_popup = true; /* its keys react, also under a popup */
    cydui_reset_clip(&ui->ctx);
    cydui_draw_rect(&ui->ctx, kb, bg->fill, 255);
    cydui_draw_rect(&ui->ctx, (cydui_rect){kb.x, kb.y, kb.w, 1}, bg->border, 255);
    cydui_push_id_str(ui, "##keyboard");
    for (row = 0; row < KB_ROWS; row++) {
        key_def keys[MAX_KEYS];
        int n = build_row(ui, row, keys), i, total = 0, gaps, avail, cum = 0;
        int y = kb.y + KB_PAD + row * (KEY_H + KEY_GAP);

        for (i = 0; i < n; i++) {
            total += keys[i].weight;
        }
        gaps = (n - 1) * KEY_GAP;
        avail = be->width - 2 * KB_PAD - gaps;
        for (i = 0; i < n; i++) {
            const key_def *k = &keys[i];
            int x0 = KB_PAD + cum * avail / total + i * KEY_GAP, x1;
            cydui_rect r;
            cydui_id id;
            cydui_response resp;
            cydui_paint_info pi;
            bool on;

            cum += k->weight;
            x1 = KB_PAD + cum * avail / total + i * KEY_GAP;
            r = (cydui_rect){(int16_t)x0, (int16_t)y, (int16_t)(x1 - x0), KEY_H};
            cydui_push_id_int(ui, (long)(row * 16 + i));
            id = cydui_id_str(ui, "k");
            cydui_pop_id(ui);
            resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK);

            if (k->kind == K_BACK) { /* Backspace works on the press and repeats while it is held */
                if (resp.pressed && in->frame_down && n_out < MAX_OUT) {
                    out[n_out++] = (cydui_event){CYDUI_EVENT_KEY, 0, 0, CYDUI_KEY_BACKSPACE};
                    ui->kb_rep = 0;
                } else if (resp.pressed && in->down && in->has_clock && (uint32_t)(in->now - in->press_ms) >= REPEAT_DELAY_MS) {
                    uint32_t want = 1u + (uint32_t)(in->now - in->press_ms - REPEAT_DELAY_MS) / REPEAT_EVERY_MS;

                    while (ui->kb_rep < want && n_out < MAX_OUT) {
                        out[n_out++] = (cydui_event){CYDUI_EVENT_KEY, 0, 0, CYDUI_KEY_BACKSPACE};
                        ui->kb_rep++;
                    }
                    ui->kb_rep = want;
                }
            } else if (resp.clicked) {
                switch (k->kind) {
                case K_CHAR:
                case K_SPACE:
                    if (n_out < MAX_OUT) {
                        uint32_t cp = k->kind == K_CHAR && ui->kb_shift ? shifted(k->cp) : k->cp;
                        out[n_out++] = (cydui_event){CYDUI_EVENT_CHAR, 0, 0, cp};
                    }
                    if (k->kind == K_CHAR) {
                        ui->kb_shift = false; /* one capital */
                    }
                    break;
                case K_SHIFT:
                    ui->kb_shift = !ui->kb_shift;
                    break;
                case K_PAGE:
                    ui->kb_page = k->page;
                    ui->kb_shift = false;
                    break;
                case K_ENTER:
                    if (n_out < MAX_OUT) {
                        out[n_out++] = (cydui_event){CYDUI_EVENT_KEY, 0, 0, CYDUI_KEY_ENTER};
                    }
                    break;
                case K_HIDE:
                    cydui_focus_clear(ui);
                    break;
                default:
                    break;
                }
            }

            on = is_active_toggle(ui, k);
            memset(&pi, 0, sizeof(pi));
            pi.kind = k->kind == K_ENTER ? CYDUI_WIDGET_BUTTON : CYDUI_WIDGET_KEY;
            pi.state = resp.pressed ? CYDUI_STATE_PRESSED : CYDUI_STATE_NORMAL;
            pi.selected = on;
            pi.rect = r;
            pi.text = k->label;
            pi.text_len = strlen(k->label);
            pi.text_extent = cydui_text_size(st->font, k->label, pi.text_len, 0);
            cydui_paint(ui, &pi);
        }
    }
    cydui_pop_id(ui);
    ui->in_kb = false;
    ui->in_popup = saved_popup;
    return n_out;
}
