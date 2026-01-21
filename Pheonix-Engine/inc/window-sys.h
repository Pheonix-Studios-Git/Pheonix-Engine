#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <err-codes.h>

#define MAX_WINDOWS 10
#define PX_WE_QUEUE_SIZE 64

typedef enum {
    PX_WE_NONE = 0,
    PX_WE_CLOSE,
    PX_WE_RESIZE,
    PX_WE_KEYDOWN,
    PX_WE_KEYUP,
    PX_WE_MOUSE_DOWN,
    PX_WE_MOUSE_UP,
    PX_WE_MOUSE_MOVE
} PX_WE_Type;

typedef struct {
    PX_WE_Type type;
    int keycode; // Mapped using event-sys keys
    int x, y;
    int w, h;
} PX_WEvent;

typedef struct {
    PX_WEvent events[PX_WE_QUEUE_SIZE];
    int head;
    int tail;
} PX_WE_Queue;

typedef struct {
    unsigned int width;
    unsigned int height;
    const char* title;
    unsigned int flags;
    int handle;
    PX_WE_Queue queue;
} PX_Window;

typedef struct {
    uint32_t bg_color;
    uint32_t fg_color;
    int border_radius;
    int border_thickness;
    uint32_t border_color;
    int title_bar_height;
    uint32_t title_bar_color;
    uint32_t title_text_color;
} PX_WindowDesign;

t_err_codes px_ws_init(void);
void px_ws_shutdown(void);

t_err_codes px_ws_create(PX_Window* win);
t_err_codes px_ws_show(PX_Window* win);
t_err_codes px_ws_hide(PX_Window* win);
void px_ws_destroy(PX_Window* win);

t_err_codes px_ws_poll(PX_Window* win);
bool px_ws_pop_event(PX_Window* win, PX_WEvent* out);

t_err_codes px_ws_show_splash(void);
t_err_codes px_ws_window_design(PX_Window* win, PX_WindowDesign* design);

t_err_codes px_ws_create_ctx(PX_Window* win);
t_err_codes px_ws_swap_buffers(PX_Window* win);
