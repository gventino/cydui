#include "uirig.h"

#include <string.h>

#include "cydui_sans_12.h"

uint32_t rig_init_with(uirig *r, size_t state_entries, uint16_t long_press_ms)
{
    cydui_ui_config cfg;

    memset(r, 0, sizeof(*r));
    cydui_mem_init(&r->mem, r->fb, RIG_W, RIG_H);
    cydui_theme_default(&r->theme, &cydui_sans_12);

    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &r->mem.backend;
    cfg.core.cmd_mem = r->cmds;
    cfg.core.cmd_mem_size = sizeof(r->cmds);
    cfg.core.strip_mem = r->strip;
    cfg.core.strip_mem_size = sizeof(r->strip);
    cfg.core.text_mem = r->text;
    cfg.core.text_mem_size = sizeof(r->text);
    cfg.state_mem = r->state;
    cfg.state_mem_size = state_entries * sizeof(cydui_state);
    cfg.idset_mem = r->idset;
    cfg.idset_mem_size = sizeof(r->idset);
    cfg.theme = &r->theme;
    cfg.long_press_ms = long_press_ms;
    return cydui_ui_init(&r->ui, &cfg);
}

uint32_t rig_init(uirig *r)
{
    return rig_init_with(r, sizeof(r->state) / sizeof(r->state[0]), 0);
}

void rig_down(uirig *r, int x, int y)
{
    cydui_mem_push_event(&r->mem, CYDUI_EVENT_DOWN, (int16_t)x, (int16_t)y);
}

void rig_move(uirig *r, int x, int y)
{
    cydui_mem_push_event(&r->mem, CYDUI_EVENT_MOVE, (int16_t)x, (int16_t)y);
}

void rig_up(uirig *r, int x, int y)
{
    cydui_mem_push_event(&r->mem, CYDUI_EVENT_UP, (int16_t)x, (int16_t)y);
}

void rig_advance(uirig *r, uint32_t ms)
{
    r->mem.now += ms;
}

bool rig_empty_frame(uirig *r)
{
    cydui_ui_begin(&r->ui, 0);
    return cydui_ui_end(&r->ui);
}

void rig_char(uirig *r, uint32_t cp)
{
    cydui_mem_push_key(&r->mem, CYDUI_EVENT_CHAR, cp);
}

void rig_key(uirig *r, cydui_key key)
{
    cydui_mem_push_key(&r->mem, CYDUI_EVENT_KEY, (uint32_t)key);
}
