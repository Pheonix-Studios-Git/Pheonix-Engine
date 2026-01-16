#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <window-sys.h>
#include <window-sys/backends.h>
#include <err-codes.h>
#include <event-sys.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <rendering-sys/opengl.h>

struct keysym_map {
    KeySym sym;
    PX_EKeycodes key;
};

struct window {
    Display* display;
    Window window;
    GLXContext gl_ctx;
    bool gl_ctx_valid;
    Atom wm_delete;
};

struct winarray {
    struct window* win;
    struct winarray* next;
    struct winarray* prev;
    
    int handle;
};

static const struct keysym_map g_keysym_map[] = {
    { XK_0, EKeycode_0 },
    { XK_1, EKeycode_1 },
    { XK_2, EKeycode_2 },
    { XK_3, EKeycode_3 },
    { XK_4, EKeycode_4 },
    { XK_5, EKeycode_5 },
    { XK_6, EKeycode_6 },
    { XK_7, EKeycode_7 },
    { XK_8, EKeycode_8 },
    { XK_9, EKeycode_9 },

    { XK_q, EKeycode_Q }, { XK_Q, EKeycode_Q },
    { XK_w, EKeycode_W }, { XK_W, EKeycode_W },
    { XK_e, EKeycode_E }, { XK_E, EKeycode_E },
    { XK_r, EKeycode_R }, { XK_R, EKeycode_R },
    { XK_t, EKeycode_T }, { XK_T, EKeycode_T },
    { XK_y, EKeycode_Y }, { XK_Y, EKeycode_Y },
    { XK_u, EKeycode_U }, { XK_U, EKeycode_U },
    { XK_i, EKeycode_I }, { XK_I, EKeycode_I },
    { XK_o, EKeycode_O }, { XK_O, EKeycode_O },
    { XK_p, EKeycode_P }, { XK_P, EKeycode_P },

    { XK_a, EKeycode_A }, { XK_A, EKeycode_A },
    { XK_s, EKeycode_S }, { XK_S, EKeycode_S },
    { XK_d, EKeycode_D }, { XK_D, EKeycode_D },
    { XK_f, EKeycode_F }, { XK_F, EKeycode_F },
    { XK_g, EKeycode_G }, { XK_G, EKeycode_G },
    { XK_h, EKeycode_H }, { XK_H, EKeycode_H },
    { XK_j, EKeycode_J }, { XK_J, EKeycode_J },
    { XK_k, EKeycode_K }, { XK_K, EKeycode_K },
    { XK_l, EKeycode_L }, { XK_L, EKeycode_L },

    { XK_z, EKeycode_Z }, { XK_Z, EKeycode_Z },
    { XK_x, EKeycode_X }, { XK_X, EKeycode_X },
    { XK_c, EKeycode_C }, { XK_C, EKeycode_C },
    { XK_v, EKeycode_V }, { XK_V, EKeycode_V },
    { XK_b, EKeycode_B }, { XK_B, EKeycode_B },
    { XK_n, EKeycode_N }, { XK_N, EKeycode_N },
    { XK_m, EKeycode_M }, { XK_M, EKeycode_M },

    { XK_Shift_L, EKeycode_LShift },
    { XK_Shift_R, EKeycode_RShift },
    { XK_Control_L, EKeycode_LControl },
    { XK_Control_R, EKeycode_RControl },
    { XK_Alt_L, EKeycode_LAlternate },
    { XK_Alt_R, EKeycode_RAlternate },
    { XK_Super_L, EKeycode_Super },
    { XK_Super_R, EKeycode_Super },
    { XK_Caps_Lock, EKeycode_Capslock },
    { XK_Num_Lock, EKeycode_Numlock },

    { XK_Tab, EKeycode_Tab },
    { XK_space, EKeycode_Space },
    { XK_Return, EKeycode_Enter },
    { XK_BackSpace, EKeycode_Backspace },
    { XK_Escape, EKeycode_Escape },

    { XK_Insert, EKeycode_Insert },
    { XK_Delete, EKeycode_Delete },
    { XK_Home, EKeycode_Home },
    { XK_End, EKeycode_End },
    { XK_Page_Up, EKeycode_PageUp },
    { XK_Page_Down, EKeycode_PageDown },

    { XK_Up, EKeycode_UpArrow },
    { XK_Down, EKeycode_DownArrow },
    { XK_Left, EKeycode_LeftArrow },
    { XK_Right, EKeycode_RightArrow },

    { XK_KP_0, EKeycode_Num0 },
    { XK_KP_1, EKeycode_Num1 },
    { XK_KP_2, EKeycode_Num2 },
    { XK_KP_3, EKeycode_Num3 },
    { XK_KP_4, EKeycode_Num4 },
    { XK_KP_5, EKeycode_Num5 },
    { XK_KP_6, EKeycode_Num6 },
    { XK_KP_7, EKeycode_Num7 },
    { XK_KP_8, EKeycode_Num8 },
    { XK_KP_9, EKeycode_Num9 },

    { XK_KP_Add, EKeycode_NumPlus },
    { XK_KP_Subtract, EKeycode_NumDash },
    { XK_KP_Multiply, EKeycode_NumAsterik },
    { XK_KP_Divide, EKeycode_NumSlash },
    { XK_KP_Enter, EKeycode_NumEnter },
    { XK_KP_Decimal, EKeycode_NumPoint },

    { XK_F1, EKeycode_F1 },
    { XK_F2, EKeycode_F2 },
    { XK_F3, EKeycode_F3 },
    { XK_F4, EKeycode_F4 },
    { XK_F5, EKeycode_F5 },
    { XK_F6, EKeycode_F6 },
    { XK_F7, EKeycode_F7 },
    { XK_F8, EKeycode_F8 },
    { XK_F9, EKeycode_F9 },
    { XK_F10, EKeycode_F10 },
    { XK_F11, EKeycode_F11 },
    { XK_F12, EKeycode_F12 },

    { XK_grave, EKeycode_Backtick },
    { XK_minus, EKeycode_Dash },
    { XK_equal, EKeycode_Equals },
    { XK_bracketleft, EKeycode_SqBracketOpen },
    { XK_bracketright, EKeycode_SqBracketClose },
    { XK_backslash, EKeycode_Backslash },
    { XK_semicolon, EKeycode_Semicolon },
    { XK_apostrophe, EKeycode_SingleQuotes },
    { XK_comma, EKeycode_Comma },
    { XK_period, EKeycode_Fullstop },
    { XK_slash, EKeycode_Slash },
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
            if (nxt->win->gl_ctx_valid)
                glXDestroyContext(nxt->win->display, nxt->win->gl_ctx);
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

static PX_EKeycodes x11_map_keysym(KeySym sym) {
    for (size_t i = 0; i < sizeof(g_keysym_map)/sizeof(g_keysym_map[0]); i++) {
        if (g_keysym_map[i].sym == sym)
            return g_keysym_map[i].key;
    }
    return EKeycode_Unknown;
}

static PX_EKeycodes x11_map_mousesym(unsigned int button) {
    switch (button) {
        case 1: return EKeycode_MouseLButton;
        case 2: return EKeycode_MouseMButton;
        case 3: return EKeycode_MouseRButton;
        case 4: return EKeycode_MouseScrollUp;
        case 5: return EKeycode_MouseScrollDown;
        case 6: return EKeycode_MouseScrollLeft;
        case 7: return EKeycode_MouseScrollRight;
        case 8: return EKeycode_MouseX1;
        case 9: return EKeycode_MouseX2;
        default: return EKeycode_Unknown;
    }
}

static t_err_codes x11_init(void) {
    g_display = XOpenDisplay(NULL);
    if (!g_display)
        return ERR_WS_INIT_FAILED;
    g_screen = DefaultScreen(g_display);
    XkbSetDetectableAutoRepeat(g_display, True, NULL);
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
        StructureNotifyMask |
        ExposureMask
    );

    XMapRaised(iwin->display, iwin->window); 
    XFlush(iwin->display);

    win->handle = append_window(iwin);
    return ERR_SUCCESS;
}

static void x11_destroy(PX_Window* win) {
    if (!win || win->handle < 0)
        return;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return;

    if (iwin->gl_ctx_valid)
        glXDestroyContext(iwin->display, iwin->gl_ctx);
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
        KeySym sym;
        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == iwin->wm_delete)
                    we.type = PX_WE_CLOSE;
                break;

            case MapNotify:
                XSetInputFocus(iwin->display, iwin->window, RevertToParent, CurrentTime);
                break;

            case ConfigureNotify:
                we.type = PX_WE_RESIZE;
                we.w = ev.xconfigure.width;
                we.h = ev.xconfigure.height;
                break;

            case KeyPress:
                we.type = PX_WE_KEYDOWN;

                sym = XkbKeycodeToKeysym(
                    ev.xkey.display,
                    ev.xkey.keycode,
                    0,
                    0
                );

                we.keycode = x11_map_keysym(sym);
                break;

            case KeyRelease:
                we.type = PX_WE_KEYUP;

                sym = XkbKeycodeToKeysym(
                    ev.xkey.display,
                    ev.xkey.keycode,
                    0,
                    0
                );

                we.keycode = x11_map_keysym(sym);
                break;

            case ButtonPress:
                we.type = PX_WE_MOUSE_DOWN;
                we.keycode = x11_map_mousesym(ev.xbutton.button);
                we.x = ev.xbutton.x;
                we.y = ev.xbutton.y;
                break;

            case ButtonRelease:
                we.type = PX_WE_MOUSE_UP;
                we.keycode = x11_map_mousesym(ev.xbutton.button);
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

static t_err_codes x11_engine_splash(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;
    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    XMapWindow(iwin->display, iwin->window);
    XEvent ev;
    do {
        XNextEvent(iwin->display, &ev);
    } while (ev.type != Expose);

    GC gc = XCreateGC(iwin->display, iwin->window, 0, NULL);
    XSetForeground(iwin->display, gc, BlackPixel(iwin->display, g_screen));
    XDrawString(iwin->display, iwin->window, gc, 50, 50, "Pheonix Engine", 13);

    XFlush(iwin->display);
    XFreeGC(iwin->display, gc);

    return ERR_SUCCESS;
}

static t_err_codes x11_window_design(PX_Window* win, PX_WindowDesign* design) {
    if (!win || win->handle < 0 || !design)
        return ERR_INTERNAL;
    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    Colormap cmap = DefaultColormap(iwin->display, g_screen);
    XColor xcolor;
    xcolor.pixel = 0;
    xcolor.red = ((design->bg_color >> 16) & 0xFF) * 257;
    xcolor.green = ((design->bg_color >> 8) & 0xFF) * 257;
    xcolor.blue = (design->bg_color & 0xFF) * 257;
    XAllocColor(iwin->display, cmap, &xcolor);
    XSetWindowBackground(iwin->display, iwin->window, xcolor.pixel);
    XClearWindow(iwin->display, iwin->window);

    return ERR_SUCCESS;
}

static t_err_codes x11_create_ctx(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;
    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    XVisualInfo* visual = glXChooseVisual(iwin->display, 0, (int[]){
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    });

    GLXContext gl_ctx = glXCreateContext(iwin->display, visual, 0, True);
    iwin->gl_ctx_valid = true;
    iwin->gl_ctx = gl_ctx;
    glXMakeCurrent(iwin->display, iwin->window, gl_ctx);

    XFree(visual);

    return ERR_SUCCESS;
}

static t_err_codes x11_swap_buffers(PX_Window* win) {
    if (!win || win->handle < 0)
        return ERR_INTERNAL;
    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->gl_ctx_valid) return ERR_WS_NO_WINDOW_FOUND;

    glXSwapBuffers(iwin->display, iwin->window);

    return ERR_SUCCESS;
}

const t_px_ws_backend px_ws_backend_x11 = {
    .init = x11_init,
    .shutdown = x11_shutdown,
    .create = x11_create,
    .destroy = x11_destroy,
    .show = x11_show,
    .hide = x11_hide,
    .poll_events = x11_poll_events,
    .show_splash = x11_engine_splash,
    .window_design = x11_window_design,
    .create_ctx = x11_create_ctx,
    .swap_buffers = x11_swap_buffers
};

