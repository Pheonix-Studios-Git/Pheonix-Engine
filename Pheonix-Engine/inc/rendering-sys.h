#pragma once

#include <stdbool.h>

#include <err-codes.h>
#include <font.h>

#define PX_RS_MAX_DROPDOWN_ITEMS 16
#define PX_RS_MAX_DROPDOWN_OPTIONS 16

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
    float x, y, z, w;
} PX_Orientation3;

typedef struct {
    PX_Vector3 pos;
    PX_Scale3 scale;
    PX_Orientation3 rot;
} PX_Transform3;

typedef struct {
    PX_Vector2 pos;
    PX_Scale2 scale;
} PX_Transform2;

typedef struct {
    char* label;
    int width;
    int height;
} PX_DropdownOption;

typedef struct {
    char* label;
    int width;
    int height;
    int spacing;

    PX_DropdownOption options[PX_RS_MAX_DROPDOWN_OPTIONS];
    int option_count;

    bool is_open;
    
    PX_Transform2 panel_tran;
    PX_Vector2 stext_pos;
    
    float font_size;

    PX_Color4 text_color;
    PX_Color4 panel_color;
    PX_Color4 hover_color;

    float panel_noise;
    float panel_cradius;

    int hover_index;
} PX_DropdownItem;

typedef struct {
    PX_Vector2 pos;
    PX_Vector2 stext_pos;
    int spacing;
    int width, height;
    
    PX_Color4 color;
    PX_Color4 hover_color;
    PX_Color4 text_color;
    
    PX_Font* font;
    float font_size;
    
    float noise;
    float cradius;

    PX_DropdownItem items[PX_RS_MAX_DROPDOWN_ITEMS];
    int item_count;

    int hover_index;
} PX_Dropdown;

t_err_codes px_rs_init_ui(PX_Scale2 screen_scale);
void px_rs_shutdown_ui(void);
void px_rs_frame_start(void);
void px_rs_frame_end(void);
void px_rs_ui_frame_update(void);
void px_rs_ui_resize(PX_Scale2 screen_scale);
t_err_codes px_rs_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius);
int px_rs_text_width(PX_Font* font, const char* text, float pixel_height);
t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font);
t_err_codes px_rs_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color);
t_err_codes px_rs_draw_dropdown(PX_Dropdown* dd);
