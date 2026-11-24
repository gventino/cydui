#include "testutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cydui_color tu_fb[TU_W * TU_H];
uint32_t tu_errors;

static cydui_cmd cmd_mem[1024];
static cydui_color strip_mem[TU_W * TU_H];

uint64_t tu_render(tu_scene scene, int strip_rows, cydui_color clear)
{
    cydui_mem mem;
    cydui_ctx ctx;
    cydui_config cfg;
    uint32_t err;

    memset(tu_fb, 0xAA, sizeof(tu_fb)); /* garbage: every pixel must be written */
    cydui_mem_init(&mem, tu_fb, TU_W, TU_H);

    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = &mem.backend;
    cfg.cmd_mem = cmd_mem;
    cfg.cmd_mem_size = sizeof(cmd_mem);
    cfg.strip_mem = strip_mem;
    cfg.strip_mem_size = (size_t)strip_rows * TU_W * sizeof(cydui_color);
    err = cydui_init(&ctx, &cfg);
    if (err) {
        fprintf(stderr, "tu_render: cydui_init failed (0x%x)\n", (unsigned)err);
        exit(2);
    }

    cydui_begin_frame(&ctx, clear);
    scene(&ctx);
    cydui_end_frame(&ctx);
    tu_errors = cydui_errors(&ctx);
    return cydui_mem_hash(&mem);
}
