/*
 * Easing curves and tweens (FR-53, FR-55, CT-08). Integer arithmetic only. A tween lives in the
 * state table, so it needs no memory of its own and is recycled with the rest of the state when
 * nobody asks for it any more.
 */

#include "cydui/anim.h"
#include "cydui/ui.h"
#include "ui_internal.h"

#define ONE CYDUI_ANIM_ONE
#define HALF (ONE / 2)

/* sin over a quarter of a turn in 64 steps, scaled by ONE: the spring of OUT_ELASTIC */
static const int16_t sin_quarter[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189,
    1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
    2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290,
    3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889,
    3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096,
};

/* cos of a phase in 1/256 of a turn, scaled by ONE */
static int32_t cos256(uint32_t phase)
{
    uint32_t p = (phase + 64u) & 255u; /* cos x = sin (x + a quarter) */
    uint32_t q = p & 63u;

    switch (p >> 6) {
    case 0:
        return sin_quarter[q];
    case 1:
        return sin_quarter[64u - q];
    case 2:
        return -(int32_t)sin_quarter[q];
    default:
        return -(int32_t)sin_quarter[64u - q];
    }
}

int32_t cydui_ease_apply(cydui_ease e, int32_t t)
{
    int64_t x = t < 0 ? 0 : (t > ONE ? ONE : t), u = ONE - x, s;

    switch (e) {
    case CYDUI_EASE_IN_QUAD:
        return (int32_t)(x * x / ONE);
    case CYDUI_EASE_OUT_QUAD:
        return (int32_t)(ONE - u * u / ONE);
    case CYDUI_EASE_IN_OUT_QUAD:
        return x < HALF ? (int32_t)(2 * x * x / ONE) : (int32_t)(ONE - 2 * u * u / ONE);
    case CYDUI_EASE_IN_CUBIC:
        return (int32_t)(x * x * x / ((int64_t)ONE * ONE));
    case CYDUI_EASE_OUT_CUBIC:
        return (int32_t)(ONE - u * u * u / ((int64_t)ONE * ONE));
    case CYDUI_EASE_IN_OUT_CUBIC:
        return x < HALF ? (int32_t)(4 * x * x * x / ((int64_t)ONE * ONE))
                        : (int32_t)(ONE - 4 * u * u * u / ((int64_t)ONE * ONE));
    case CYDUI_EASE_OUT_BACK: { /* 1 + c3 s^3 + c1 s^2 with s = t - 1, c1 = 1.70158, c3 = c1 + 1 */
        const int64_t c1 = 6970, c3 = c1 + ONE;
        s = x - ONE;
        return (int32_t)(ONE + c3 * s * s * s / ((int64_t)ONE * ONE * ONE) + c1 * s * s / ((int64_t)ONE * ONE));
    }
    case CYDUI_EASE_OUT_ELASTIC: /* 1 - (1 - t)^4 cos (2.5 turns * t): starts at 0, swings, ends at 1 */
        return (int32_t)(ONE - u * u * u * u / ((int64_t)ONE * ONE * ONE) *
                                   cos256((uint32_t)(x * 640 / ONE)) / ONE);
    case CYDUI_EASE_LINEAR:
    case CYDUI_EASE_COUNT:
    default:
        return (int32_t)x;
    }
}

int32_t cydui_lerp(int32_t a, int32_t b, int32_t k)
{
    int64_t v = (int64_t)a + ((int64_t)b - (int64_t)a) * k / ONE;
    return v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : (int32_t)v);
}

/* ---- tweens ---- */

/* The state of a tween: a from, b to, c start (the clock), d = flag | curve << 16 | duration. */
#define TW_LIVE 0x80000000u

static uint32_t tw_pack(cydui_ease e, uint16_t dur)
{
    return TW_LIVE | ((uint32_t)e << 16) | dur;
}

/* Where a tween is at `now`; `running` tells whether it still has to move. */
static int32_t tw_value(const cydui_state *s, uint32_t now, bool *running)
{
    uint32_t d = (uint32_t)s->d;
    int32_t dur = (int32_t)(d & 0xFFFFu), el = (int32_t)(now - (uint32_t)s->c); /* wraps right; a clock that went back reads 0 */

    if (el < 0) {
        el = 0;
    }
    *running = false;
    if (s->a == s->b || el >= dur) {
        return s->b;
    }
    *running = true;
    return cydui_lerp(s->a, s->b, cydui_ease_apply((cydui_ease)((d >> 16) & 0x7Fu), (int32_t)((int64_t)el * ONE / dur)));
}

int32_t cydui_animate(cydui_ui *ui, cydui_id id, int32_t from, int32_t target, uint16_t duration_ms,
                      cydui_ease ease)
{
    cydui_state *s;
    bool running;
    int32_t v;

    cydui__check_thread(ui);
    if (duration_ms > CYDUI_ANIM_MAX_MS) {
        duration_ms = CYDUI_ANIM_MAX_MS;
    }
    if (!ui->in.has_clock || duration_ms == 0 || (unsigned)ease >= CYDUI_EASE_COUNT) {
        return target; /* nothing to move with: it is there at once */
    }
    s = cydui_state_get(ui, cydui__id_mix(id, "\x01tween", 6), true);
    if (!s) {
        return target; /* the state table is full */
    }
    if (!((uint32_t)s->d & TW_LIVE)) { /* seen for the first time: it starts at `from` */
        s->a = from;
        s->b = target;
        s->c = (int32_t)ui->in.now;
        s->d = (int32_t)tw_pack(ease, duration_ms);
    } else if (s->b != target) { /* a new target: go there from wherever it is now */
        v = tw_value(s, ui->in.now, &running);
        s->a = v;
        s->b = target;
        s->c = (int32_t)ui->in.now;
        s->d = (int32_t)tw_pack(ease, duration_ms);
    }
    v = tw_value(s, ui->in.now, &running);
    if (running) {
        if (ui->tweens_now >= CYDUI_MAX_TWEENS) { /* too many at once: this one does not move */
            ui->ctx.errors |= CYDUI_ERR_TWEEN_LIMIT;
            s->a = target;
            return target;
        }
        ui->tweens_now++;
    }
    return v;
}

void cydui__animate_reset(cydui_ui *ui, cydui_id id)
{
    cydui_state *s = cydui_state_get(ui, cydui__id_mix(id, "\x01tween", 6), false);

    if (s) {
        s->d = 0;
    }
}

uint32_t cydui_now(const cydui_ui *ui)
{
    return ui->in.now;
}
