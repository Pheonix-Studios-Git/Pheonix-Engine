#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <window-sys.h>
#include <window-sys/backends.h>
#include <err-codes.h>

extern const t_px_ws_backend px_ws_backend_x11;
extern const t_px_ws_backend px_ws_backend_null;

static const t_px_ws_backend* g_backend = NULL;

t_err_codes px_ws_init(void) {
    #if defined(__linux__)
        g_backend = &px_ws_backend_x11;
    #else
        g_backend = &px_ws_backend_null;
    #endif

    if (!g_backend)
        return ERR_WS_UNSUPPORTED;

    return g_backend->init();
}

void px_ws_shutdown(void) {
    if (g_backend && g_backend->shutdown)
        g_backend->shutdown();

    g_backend = NULL;
}

void px_ws_destroy(PX_Window* win) {
    if (g_backend && win)
        g_backend->destroy(win);
}

t_err_codes px_ws_create(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->create(win);
}

t_err_codes px_ws_show(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->show(win);
}

t_err_codes px_ws_hide(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->hide(win);
}

t_err_codes px_ws_poll(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->poll_events(win);
}

bool px_ws_pop_event(PX_Window* win, PX_WEvent* out) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return px_we_queue_pop(&win->queue, out);
}

t_err_codes px_ws_show_splash(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->show_splash(win);
}

t_err_codes px_ws_window_design(PX_Window* win, PX_WindowDesign* design) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win || !design)
        return ERR_INTERNAL;

    return g_backend->window_design(win, design);
}

t_err_codes px_ws_create_ctx(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->create_ctx(win);
}

t_err_codes px_ws_swap_buffers(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INTERNAL;

    return g_backend->swap_buffers(win);
}
