#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <window-sys.h>
#include <rendering-sys.h>
#include <font.h>
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

t_err_codes px_ws_create(PX_Window* win, PX_GPU_Backend gpu_backend_api) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->create(win, gpu_backend_api);
}

t_err_codes px_ws_show(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->show(win);
}

t_err_codes px_ws_hide(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->hide(win);
}

t_err_codes px_ws_poll(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->poll_events(win);
}

bool px_ws_pop_event(PX_Window* win, PX_WEvent* out) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win || !out)
        return ERR_INVALID_ARGUMENTS;

    return px_we_queue_pop(&win->queue, out);
}

t_err_codes px_ws_show_splash(void) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;

    return g_backend->show_splash();
}

t_err_codes px_ws_window_design(PX_Window* win, PX_WindowDesign* design) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win || !design)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->window_design(win, design);
}

t_err_codes px_ws_create_ctx(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->create_ctx(win);
}

t_err_codes px_ws_get_ctx(PX_Window* win, PX_WContext* out) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win || !out)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->get_ctx(win, out);
}

t_err_codes px_ws_swap_buffers(PX_Window* win) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->swap_buffers(win);
}

char* px_ws_open_file_selector_dialog(void) {
    if (!g_backend)
        return NULL;
    return g_backend->open_file_selector_dialog();
}

t_err_codes px_ws_set_mouse_locked(PX_Window* win, bool locked) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->set_mouse_locked(win, locked);
}

t_err_codes px_ws_set_mouse_pos(PX_Window* win, PX_Vector2 pos) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->set_mouse_pos(win, pos);
}

t_err_codes px_ws_set_fullscreen(PX_Window* win, bool enabled) {
    if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!win)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->set_fullscreen(win, enabled);
}

// Vulkan-Specific
#include <rendering-sys/vulkan.h>

t_err_codes px_ws_vk_finish_ctx(PX_WContext* ctx, VkInstance instance, PFN_vkGetInstanceProcAddr GetInstanceProcAddr) {
	if (!g_backend)
        return ERR_WS_UNINITIALIZED;
    else if (!ctx)
        return ERR_INVALID_ARGUMENTS;

    return g_backend->vk_finish_ctx(ctx, instance, GetInstanceProcAddr);
}
