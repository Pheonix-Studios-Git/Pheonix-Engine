#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <window-sys.h>
#include <window-sys/backends.h>
#include <err-codes.h>

#include <X11/Xlib.h>

struct window {
    Display* display;
    Window window;
    Atom wm_delete;
};

struct winarray {
    struct window* win;
    struct winarray* next;
    struct winarray* prev;
    
    int handle;
};

static struct winarray* g_windows = NULL;
static Display* g_display = NULL;
static int g_screen = 0;
static int g_handle = 0;

static int append_window(struct window* win) {
    struct winarray* node = (struct winarray*)malloc(sizeof(struct winarray));
    if (!node)
        return -1;
    
    node->win = win;
    node->next = NULL;

    if (g_windows) {
        struct winarray* nxt = g_windows;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (nxt->next)
                nxt = nxt->next;
            else 
                break;
        }

        nxt->next = node;
        node->prev = nxt;
    } else {
        g_windows = node;
        node->prev = NULL;
    }

    node->handle = g_handle;
    g_handle++;
    return node->handle;
}

static struct window* get_window(int handle) {
    if (!g_windows)
        return NULL;

    struct winarray* nxt = g_windows;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (nxt->handle == handle)
            return nxt->win;
        if (nxt->next)
            nxt = nxt->next;
        else
            break;
    }

    return NULL;
}

static void remove_window(int handle) {
    if (!g_windows)
        return;

    struct winarray* nxt = g_windows;
    bool found = false;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (nxt->handle == handle) {
            found = true;
            break;
        }

        if (nxt->next)
            nxt = nxt->next;
        else
            break;
    }

    if (found) {
        if (nxt->prev)
            nxt->prev->next = nxt->next;
        else
            g_windows = nxt->next;

        if (nxt->next)
            nxt->next->prev = nxt->prev;

        free(nxt);
    }
}

static void destroy_all_windows(void) {
    if (!g_windows)
        return;

    struct winarray* nxt = g_windows;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (nxt->win) {
            XDestroyWindow(nxt->win->display, nxt->win->window);
            free(nxt->win);
        }
        if (nxt->next)
            nxt = nxt->next;
        else
            break;
    }

    struct winarray* cur = nxt;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (nxt->prev)
            nxt = nxt->prev;
        free(cur);
        cur = nxt;
    }
}

static t_err_codes x11_init(void) {
    g_display = XOpenDisplay(NULL);
    if (!g_display)
        return ERR_WS_INIT_FAILED;
    g_screen = DefaultScreen(g_display);
    return ERR_SUCCESS;
}

static void x11_shutdown(void) {
    if (g_windows)
        destroy_all_windows();

    if (g_display) {
        XCloseDisplay(g_display);
        g_display = NULL;
    }
}

static t_err_codes x11_create(PX_Window* win) {
    if (!win)
        return ERR_INTERNAL;

    win->handle = -1;

    struct window* iwin = (struct window*)malloc(sizeof(struct window));
    if (!iwin)
        return ERR_ALLOC_FAILED;

    iwin->display = g_display;
    Window root = RootWindow(g_display, g_screen);
    iwin->window = XCreateSimpleWindow(
        g_display,
        root,
        0, 0,
        win->width, win->height,
        1,
        BlackPixel(g_display, g_screen),
        WhitePixel(g_display, g_screen)
    );

    XStoreName(g_display, iwin->window, win->title ? win->title : "Pheonix Engine - Unknown Window");
    iwin->wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, iwin->window, &iwin->wm_delete, 1);

    XSelectInput(
        g_display,
        iwin->window,
        KeyPressMask |
        KeyReleaseMask |
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        StructureNotifyMask
    );

    XMapWindow(iwin->display, iwin->window);
    XFlush(iwin->display);

    win->handle = append_window(iwin);
    return ERR_SUCCESS;
}

static void x11_destroy(PX_Window* win) {
    if (!win || win->handle < 0)
        return;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return;

    XDestroyWindow(iwin->display, iwin->window);
    free(iwin);

    remove_window(win->handle);
    
    win->handle = -1;
}

static t_err_codes x11_show(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    XMapWindow(iwin->display, iwin->window);
    XFlush(iwin->display);
    return ERR_SUCCESS;
}

static t_err_codes x11_hide(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    XUnmapWindow(iwin->display, iwin->window);
    XFlush(iwin->display);
    return ERR_SUCCESS;
}

static t_err_codes x11_poll_events(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    while (XPending(iwin->display)) {
        XEvent ev;
        XNextEvent(iwin->display, &ev);

        PX_WEvent we = {0};
        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == iwin->wm_delete)
                    we.type = PX_WE_CLOSE;
                break;

            case KeyPress:
                we.type = PX_WE_KEYDOWN;
                we.code = ev.xkey.keycode;
                break;

            case KeyRelease:
                we.type = PX_WE_KEYUP;
                we.code = ev.xkey.keycode;
                break;

            case ButtonPress:
                we.type = PX_WE_MOUSE_DOWN;
                we.code = ev.xbutton.button;
                we.x = ev.xbutton.x;
                we.y = ev.xbutton.y;
                break;

            case ButtonRelease:
                we.type = PX_WE_MOUSE_UP;
                we.code = ev.xbutton.button;
                we.x = ev.xbutton.x;
                we.y = ev.xbutton.y;
                break;

            case MotionNotify:
                we.type = PX_WE_MOUSE_MOVE;
                we.x = ev.xmotion.x;
                we.y = ev.xmotion.y;
                break;

            default:
                continue;
        }

        px_we_queue_push(&win->queue, &we);
    }

    return ERR_SUCCESS;
}

const t_px_ws_backend px_ws_backend_x11 = {
    .init = x11_init,
    .shutdown = x11_shutdown,
    .create = x11_create,
    .destroy = x11_destroy,
    .show = x11_show,
    .hide = x11_hide,
    .poll_events = x11_poll_events
};

