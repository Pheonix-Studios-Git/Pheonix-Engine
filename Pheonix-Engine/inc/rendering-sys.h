#pragma once

#include <err-codes.h>

typedef struct {
    unsigned char r, g, b, a;
} PX_Color4;

typedef struct {
    unsigned char r, g, b;
} PX_Color3;

typedef struct {
    int x, y;
} PX_Vector2;

typedef struct {
    int x, y, z;
} PX_Vector3;

typedef struct {
    int w, h;
} PX_Scale2;

typedef struct {
    int w, h, l;
} PX_Scale3;

typedef struct {
    PX_Vector3 position;
    PX_Scale3 scale;
} PX_Transform;

t_err_codes px_rs_init_ui(PX_Scale2 screen_scale);
void px_rs_shutdown_ui(void);
void px_rs_ui_frame_update(void);
void px_rs_ui_resize(PX_Scale2 screen_scale);
t_err_codes px_rs_draw_panel(PX_Scale2 scale, PX_Vector2 pos, PX_Color4 color);
