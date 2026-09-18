#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>

#include <window-sys.h>
#include <err-codes.h>
#include <event-sys.h>
#include <core/image.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrender.h>

#define HAVE_X11
#include <external/tinyfiledialogs.h>

#include <rendering-sys/opengl.h>
#define VK_USE_PLATFORM_XLIB_KHR
#include <rendering-sys/vulkan.h>

#include <window-sys/backends.h>
#include <GL/glx.h>

struct keysym_map {
    KeySym sym;
    PX_EKeycodes key;
};

struct window {
    Display* display;
    Window window;
	Colormap colormap;

	GLXFBConfig glx_fb_config;
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

struct x11_lib {
    void* x11;
    void* xrender;
    void* gl;

    // XLib
    Display* (*XOpenDisplay)(const char*);
    int (*XCloseDisplay)(Display*);
    int (*XPending)(Display*);
    int (*XNextEvent)(Display*, XEvent*);
    int (*XSelectInput)(Display*, Window, long);
    int (*XStoreName)(Display*, Window, const char*);
    Atom (*XInternAtom)(Display*, const char*, Bool);
    Status (*XSetWMProtocols)(Display*, Window, Atom*, int);
    int (*XMapWindow)(Display*, Window);
    int (*XMapRaised)(Display*, Window);
    int (*XUnmapWindow)(Display*, Window);
    int (*XFlush)(Display*);
    int (*XSync)(Display*, Bool);
    int (*XDestroyWindow)(Display*, Window);
    Window (*XCreateSimpleWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
    Window (*XCreateWindow)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual*, unsigned long, XSetWindowAttributes*);
    Colormap (*XCreateColormap)(Display*, Window, Visual*, int);
    int (*XFreeColormap)(Display*, Colormap);
    int (*XFree)(void*);
    int (*XFreeCursor)(Display*, Cursor);
    int (*XMoveWindow)(Display*, Window, int, int);
    int (*XSetWindowBackground)(Display*, Window, unsigned long);
    int (*XClearWindow)(Display*, Window);
    Status (*XAllocColor)(Display*, Colormap, XColor*);
    int (*XSetInputFocus)(Display*, Window, int, Time);
    int (*XWarpPointer)(Display*, Window, Window, int, int, unsigned int, unsigned int, int, int);
    int (*XGrabPointer)(Display*, Window, Bool, unsigned int, int, int, Window, Cursor, Time);
    int (*XUngrabPointer)(Display*, Time);
    Pixmap (*XCreateBitmapFromData)(Display*, Drawable, const char*, unsigned int, unsigned int);
    Cursor (*XCreatePixmapCursor)(Display*, Pixmap, Pixmap, XColor*, XColor*, unsigned int, unsigned int);
    int (*XFreePixmap)(Display*, Pixmap);
    int (*XSendEvent)(Display*, Window, Bool, long, XEvent*);
    Status (*XGetWindowAttributes)(Display*, Window, XWindowAttributes*);
    Status (*XMatchVisualInfo)(Display*, int, int, int, XVisualInfo*);
    int (*XChangeProperty)(Display*, Window, Atom, Atom, int, int, const unsigned char*, int);
    Pixmap (*XCreatePixmap)(Display*, Drawable, unsigned int, unsigned int, unsigned int);
    GC (*XCreateGC)(Display*, Drawable, unsigned long, XGCValues*);
    int (*XSetForeground)(Display*, GC, unsigned long);
    int (*XFillRectangle)(Display*, Drawable, GC, int, int, unsigned int, unsigned int);
    XImage* (*XCreateImage)(Display*, Visual*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int);
    int (*XPutImage)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned int, unsigned int);
    int (*XFreeGC)(Display*, GC);
    int (*XDestroyImage)(XImage*);

    // XKB
    Bool (*XkbSetDetectableAutoRepeat)(Display*, Bool, Bool*);
    KeySym (*XkbKeycodeToKeysym)(Display*, KeyCode, int, int);

    // GLX
    const char* (*glXQueryExtensionsString)(Display*, int);
    GLXFBConfig* (*glXChooseFBConfig)(Display*, int, const int*, int*);
    XVisualInfo* (*glXGetVisualFromFBConfig)(Display*, GLXFBConfig);
    __GLXextFuncPtr (*glXGetProcAddressARB)(const GLubyte*);
    Bool (*glXMakeCurrent)(Display*, GLXDrawable, GLXContext);
    void (*glXDestroyContext)(Display*, GLXContext);
    GLXDrawable (*glXGetCurrentDrawable)(void);
    void (*glXSwapBuffers)(Display*, GLXDrawable);

    // GLX Extensions
    GLXContext (*glXCreateContextAttribsARB)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    void (*glXSwapIntervalEXT)(Display*, GLXDrawable, int);

    // XRender
    XRenderPictFormat* (*XRenderFindVisualFormat)(Display*, _Xconst Visual*);
    Picture (*XRenderCreatePicture)(Display*, Drawable, XRenderPictFormat*, unsigned long, const XRenderPictureAttributes*);
    XRenderPictFormat* (*XRenderFindStandardFormat)(Display*, int);
    void (*XRenderComposite)(Display*, int, Picture, Picture, Picture, int, int, int, int, int, int, unsigned int, unsigned int);
    void (*XRenderFreePicture)(Display*, Picture);
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

static struct x11_lib g_xlib = {0};
static struct winarray* g_windows = NULL;
static Display* g_display = NULL;
static int g_screen = 0;
static int g_handle = 0;

static const char* g_vk_instance_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME
};

static int glx_is_ext_supported(Display *dpy, int screen, const char *extName) {
	if (!g_xlib.gl || !g_xlib.glXQueryExtensionsString) return 0;

    const char *exts = g_xlib.glXQueryExtensionsString(dpy, screen);
    if (exts) {
        return (strstr(exts, extName) != NULL);
    }
    return 0;
}

static int append_window(struct window* win) {
    struct winarray* node = (struct winarray*)malloc(sizeof(struct winarray));
    if (!node)
        return -1;
    
    node->win = win;
    node->next = NULL;

    if (g_windows) {
        struct winarray* nxt = g_windows;
        for (int i = 0; i < PX_WS_MAX_WINDOWS; i++) {
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
    for (int i = 0; i < PX_WS_MAX_WINDOWS; i++) {
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
    for (int i = 0; i < PX_WS_MAX_WINDOWS; i++) {
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
    struct winarray* node = g_windows;

    while (node) {
        struct winarray* next = node->next;

        if (node->win) {
            if (node->win->gl_ctx_valid) {
                g_xlib.glXMakeCurrent(node->win->display, None, NULL);
                g_xlib.glXDestroyContext(node->win->display, node->win->gl_ctx);
            }
			g_xlib.XDestroyWindow(node->win->display, node->win->window);
			if (node->win->colormap) g_xlib.XFreeColormap(node->win->display, node->win->colormap);

            free(node->win);
        }

        free(node);
        node = next;
    }

    g_windows = NULL;
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

static GLXFBConfig x11_choose_fb_config(Display* display) {
	if (!g_xlib.gl || !g_xlib.glXChooseFBConfig) return 0;

    int fb_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,

        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,

        GLX_DEPTH_SIZE, 24,

        GLX_DOUBLEBUFFER, True,

        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 4,

        None
    };

    int count = 0;
    GLXFBConfig* configs = g_xlib.glXChooseFBConfig(display, g_screen, fb_attribs, &count);

    if (!configs || count == 0) {
        if (configs) g_xlib.XFree(configs);
        return 0;
    }

    GLXFBConfig result = configs[0];
    g_xlib.XFree(configs);

    return result;
}

static t_err_codes x11_init(void) {
	struct x11_lib xlib = {0};
	xlib.x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
	xlib.gl = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
	xlib.xrender = dlopen("libXrender.so.1", RTLD_LAZY | RTLD_LOCAL);

	#define LOAD_SYM(lib, fn, name) { \
		*(void**)(&(fn)) = dlsym((lib), (name)); \
		if (!(fn)) { \
			fprintf(stderr, "[X11] Failed to find symbol '%s'\n", (name)); \
			goto failure; \
		} \
	}

	#define GLX_LOAD_SYM(fn, name) { \
		*(void**)(&(fn)) = xlib.glXGetProcAddressARB((const GLubyte*)(name)); \
		if (!(fn)) { \
			fprintf(stderr, "[X11] Failed to find GLX Symbol '%s'\n", (name)); \
			goto failure; \
		} \
	}

	#define GLX_LOAD_SYM_NERROR(fn, name) { \
		*(void**)(&(fn)) = xlib.glXGetProcAddressARB((const GLubyte*)(name)); \
	}

	// Xlib Symbols
	LOAD_SYM(xlib.x11, xlib.XOpenDisplay, "XOpenDisplay");
	LOAD_SYM(xlib.x11, xlib.XCloseDisplay, "XCloseDisplay");
	LOAD_SYM(xlib.x11, xlib.XPending, "XPending");
	LOAD_SYM(xlib.x11, xlib.XNextEvent, "XNextEvent");
	LOAD_SYM(xlib.x11, xlib.XSelectInput, "XSelectInput");
	LOAD_SYM(xlib.x11, xlib.XStoreName, "XStoreName");
	LOAD_SYM(xlib.x11, xlib.XInternAtom, "XInternAtom");
	LOAD_SYM(xlib.x11, xlib.XSetWMProtocols, "XSetWMProtocols");
	LOAD_SYM(xlib.x11, xlib.XMapWindow, "XMapWindow");
	LOAD_SYM(xlib.x11, xlib.XMapRaised, "XMapRaised");
	LOAD_SYM(xlib.x11, xlib.XUnmapWindow, "XUnmapWindow");
	LOAD_SYM(xlib.x11, xlib.XFlush, "XFlush");
	LOAD_SYM(xlib.x11, xlib.XSync, "XSync");
	LOAD_SYM(xlib.x11, xlib.XDestroyWindow, "XDestroyWindow");
	LOAD_SYM(xlib.x11, xlib.XCreateSimpleWindow, "XCreateSimpleWindow");
	LOAD_SYM(xlib.x11, xlib.XCreateWindow, "XCreateWindow");
	LOAD_SYM(xlib.x11, xlib.XCreateColormap, "XCreateColormap");
	LOAD_SYM(xlib.x11, xlib.XFreeColormap, "XFreeColormap");
	LOAD_SYM(xlib.x11, xlib.XFree, "XFree");
	LOAD_SYM(xlib.x11, xlib.XFreeCursor, "XFreeCursor");
	LOAD_SYM(xlib.x11, xlib.XMoveWindow, "XMoveWindow");
	LOAD_SYM(xlib.x11, xlib.XSetWindowBackground, "XSetWindowBackground");
	LOAD_SYM(xlib.x11, xlib.XClearWindow, "XClearWindow");
	LOAD_SYM(xlib.x11, xlib.XAllocColor, "XAllocColor");
	LOAD_SYM(xlib.x11, xlib.XSetInputFocus, "XSetInputFocus");
	LOAD_SYM(xlib.x11, xlib.XWarpPointer, "XWarpPointer");
	LOAD_SYM(xlib.x11, xlib.XGrabPointer, "XGrabPointer");
	LOAD_SYM(xlib.x11, xlib.XUngrabPointer, "XUngrabPointer");
	LOAD_SYM(xlib.x11, xlib.XCreateBitmapFromData, "XCreateBitmapFromData");
	LOAD_SYM(xlib.x11, xlib.XCreatePixmapCursor, "XCreatePixmapCursor");
	LOAD_SYM(xlib.x11, xlib.XFreePixmap, "XFreePixmap");
	LOAD_SYM(xlib.x11, xlib.XSendEvent, "XSendEvent");
	LOAD_SYM(xlib.x11, xlib.XGetWindowAttributes, "XGetWindowAttributes");
	LOAD_SYM(xlib.x11, xlib.XMatchVisualInfo, "XMatchVisualInfo");
	LOAD_SYM(xlib.x11, xlib.XChangeProperty, "XChangeProperty");
	LOAD_SYM(xlib.x11, xlib.XCreatePixmap, "XCreatePixmap");
	LOAD_SYM(xlib.x11, xlib.XCreateGC, "XCreateGC");
	LOAD_SYM(xlib.x11, xlib.XSetForeground, "XSetForeground");
	LOAD_SYM(xlib.x11, xlib.XFillRectangle, "XFillRectangle");
	LOAD_SYM(xlib.x11, xlib.XCreateImage, "XCreateImage");
	LOAD_SYM(xlib.x11, xlib.XPutImage, "XPutImage");
	LOAD_SYM(xlib.x11, xlib.XFreeGC, "XFreeGC");
	LOAD_SYM(xlib.x11, xlib.XDestroyImage, "XDestroyImage");

	// XKB Symbols
	LOAD_SYM(xlib.x11, xlib.XkbSetDetectableAutoRepeat, "XkbSetDetectableAutoRepeat");
	LOAD_SYM(xlib.x11, xlib.XkbKeycodeToKeysym, "XkbKeycodeToKeysym");

	// GLX Symbols
	LOAD_SYM(xlib.gl, xlib.glXQueryExtensionsString, "glXQueryExtensionsString");
	LOAD_SYM(xlib.gl, xlib.glXChooseFBConfig, "glXChooseFBConfig");
	LOAD_SYM(xlib.gl, xlib.glXGetVisualFromFBConfig, "glXGetVisualFromFBConfig");
	LOAD_SYM(xlib.gl, xlib.glXGetProcAddressARB, "glXGetProcAddressARB");
	LOAD_SYM(xlib.gl, xlib.glXMakeCurrent, "glXMakeCurrent");
	LOAD_SYM(xlib.gl, xlib.glXDestroyContext, "glXDestroyContext");
	LOAD_SYM(xlib.gl, xlib.glXGetCurrentDrawable, "glXGetCurrentDrawable");
	LOAD_SYM(xlib.gl, xlib.glXSwapBuffers, "glXSwapBuffers");

	GLX_LOAD_SYM(xlib.glXCreateContextAttribsARB, "glXCreateContextAttribsARB");
	GLX_LOAD_SYM_NERROR(xlib.glXSwapIntervalEXT, "glXSwapIntervalEXT");

	// XRender
	LOAD_SYM(xlib.xrender, xlib.XRenderFindVisualFormat, "XRenderFindVisualFormat");
	LOAD_SYM(xlib.xrender, xlib.XRenderCreatePicture, "XRenderCreatePicture");
	LOAD_SYM(xlib.xrender, xlib.XRenderFindStandardFormat, "XRenderFindStandardFormat");
	LOAD_SYM(xlib.xrender, xlib.XRenderComposite, "XRenderComposite");
	LOAD_SYM(xlib.xrender, xlib.XRenderFreePicture, "XRenderFreePicture");

	#undef LOAD_SYM
	#undef GLX_LOAD_SYM
	#undef GLX_LOAD_SYM_NERROR

	g_xlib = xlib;

    g_display = g_xlib.XOpenDisplay(NULL);
    if (!g_display) goto failure;

    g_screen = DefaultScreen(g_display);
    g_xlib.XkbSetDetectableAutoRepeat(g_display, True, NULL);
    return ERR_SUCCESS;

	failure: {
		if (g_display && xlib.XCloseDisplay && xlib.x11) {
			xlib.XCloseDisplay(g_display);
			g_display = NULL;
		}

		if (xlib.x11) dlclose(xlib.x11);
		if (xlib.gl) dlclose(xlib.gl);
		if (xlib.xrender) dlclose(xlib.xrender);

		memset(&g_xlib, 0, sizeof(struct x11_lib));
		return ERR_WS_INIT_FAILED;
	}
}

static void x11_shutdown(void) {
    if (g_windows) destroy_all_windows();

    if (g_display) {
        g_xlib.XCloseDisplay(g_display);
        g_display = NULL;
    }

	if (g_xlib.x11) dlclose(g_xlib.x11);
	if (g_xlib.gl) dlclose(g_xlib.gl);
	if (g_xlib.xrender) dlclose(g_xlib.xrender);
	memset(&g_xlib, 0, sizeof(struct x11_lib));
}

static t_err_codes x11_create(PX_Window* win, PX_GPU_Backend gpu_backend_api) {
	if (!g_display || !g_xlib.x11) return ERR_WS_UNINITIALIZED;

	if (!win) return ERR_INVALID_ARGUMENTS;
    win->handle = -1;
	win->gpu_backend_api = gpu_backend_api;

    struct window* iwin = (struct window*)calloc(1, sizeof(struct window));
    if (!iwin) return ERR_ALLOC_FAILED;

    iwin->display = g_display;
    Window root = RootWindow(g_display, g_screen);

	long event_mask = (
		KeyPressMask |
        KeyReleaseMask |
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        StructureNotifyMask |
        ExposureMask
	);

	switch (gpu_backend_api) {
		case PX_RS_GPU_BACKEND_OPENGL: {
			if (!g_xlib.gl || !g_xlib.glXGetVisualFromFBConfig) {
				free(iwin);
				return ERR_WS_UNINITIALIZED;
			}

			GLXFBConfig config = x11_choose_fb_config(g_display);
			if (!config) {
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}
			iwin->glx_fb_config = config;

			XVisualInfo* visual = g_xlib.glXGetVisualFromFBConfig(g_display, config);
			if (!visual) {
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}

			Colormap colormap = g_xlib.XCreateColormap(g_display, root, visual->visual, AllocNone);
			if (!colormap) {
				g_xlib.XFree(visual);
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}
			iwin->colormap = colormap;

			XSetWindowAttributes attrs = {
				.colormap = colormap,
				.border_pixel = BlackPixel(g_display, g_screen),
				.background_pixel = WhitePixel(g_display, g_screen),
				.event_mask = event_mask
			};

			iwin->window = g_xlib.XCreateWindow(
				g_display,
				root,
				0, 0,
				win->width,
				win->height,
				0,
				visual->depth,
				InputOutput,
				visual->visual,
				CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
				&attrs
			);

			if (!iwin->window) {
				g_xlib.XFreeColormap(g_display, iwin->colormap);
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}

			g_xlib.XFree(visual);
			break;
		}

		case PX_RS_GPU_BACKEND_VULKAN: {
			Visual* visual = DefaultVisual(g_display, g_screen);
			if (!visual) {
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}

			Colormap colormap = g_xlib.XCreateColormap(g_display, root, visual, AllocNone);
			if (!colormap) {
    			free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}
			iwin->colormap = colormap;

			XSetWindowAttributes attrs = {
				.colormap = colormap,
				.border_pixel = BlackPixel(g_display, g_screen),
				.background_pixel = WhitePixel(g_display, g_screen),
				.event_mask = event_mask
			};

			iwin->window = g_xlib.XCreateWindow(
				g_display,
				root,
				0, 0,
				win->width,
				win->height,
				0,
				DefaultDepth(g_display, g_screen),
				InputOutput,
				visual,
				CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
				&attrs
			);

			if (!iwin->window) {
				g_xlib.XFreeColormap(g_display, iwin->colormap);
				free(iwin);
				return ERR_WS_WINDOW_CREATION_FAILED;
			}
			break;
		}
		
		default: {
			free(iwin);
			return ERR_WS_INVALID_GPU_BACKEND;
		}
	}

    g_xlib.XStoreName(g_display, iwin->window, win->title ? win->title : "Pheonix Engine - Unknown Window");
    iwin->wm_delete = g_xlib.XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    g_xlib.XSetWMProtocols(g_display, iwin->window, &iwin->wm_delete, 1);

    g_xlib.XSelectInput(g_display, iwin->window, event_mask);

    g_xlib.XMapRaised(iwin->display, iwin->window); 
    g_xlib.XFlush(iwin->display);

	int handle = append_window(iwin);
	if (handle < 0) {
		g_xlib.XDestroyWindow(iwin->display, iwin->window);
		if (iwin->colormap) g_xlib.XFreeColormap(iwin->display, iwin->colormap);
		free(iwin);
		return ERR_ALLOC_FAILED;
	}
	win->handle = handle;

    return ERR_SUCCESS;
}

static void x11_destroy(PX_Window* win) {
    if (!win) return;
	if (win->handle < 0) return;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return;

	if (win->ctx_handle) {
		PX_WContext* ctx = (PX_WContext*)(uintptr_t)win->ctx_handle;
		free(ctx);
		win->ctx_handle = 0;
	}

	if (g_xlib.gl && g_xlib.glXDestroyContext) {
		g_xlib.glXMakeCurrent(iwin->display, None, NULL);
		if (iwin->gl_ctx_valid) g_xlib.glXDestroyContext(iwin->display, iwin->gl_ctx);
	}
	g_xlib.XDestroyWindow(iwin->display, iwin->window);\
	if (iwin->colormap) g_xlib.XFreeColormap(iwin->display, iwin->colormap);

    free(iwin);
    remove_window(win->handle);
    
    win->handle = -1;
}

static t_err_codes x11_show(PX_Window* win) {
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    g_xlib.XMapWindow(iwin->display, iwin->window);
    g_xlib.XFlush(iwin->display);
    return ERR_SUCCESS;
}

static t_err_codes x11_hide(PX_Window* win) {
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    g_xlib.XUnmapWindow(iwin->display, iwin->window);
    g_xlib.XFlush(iwin->display);
    return ERR_SUCCESS;
}

static t_err_codes x11_poll_events(PX_Window* win) {
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    while (g_xlib.XPending(iwin->display)) {
        XEvent ev;
        g_xlib.XNextEvent(iwin->display, &ev);

        PX_WEvent we = {0};
        KeySym sym;
        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == iwin->wm_delete)
                    we.type = PX_WE_CLOSE;
                break;

            case MapNotify:
                g_xlib.XSetInputFocus(iwin->display, iwin->window, RevertToParent, CurrentTime);
                break;

            case ConfigureNotify:
                we.type = PX_WE_RESIZE;
                we.w = ev.xconfigure.width;
                we.h = ev.xconfigure.height;
                break;

            case KeyPress:
                we.type = PX_WE_KEYDOWN;

                sym = g_xlib.XkbKeycodeToKeysym(
                    ev.xkey.display,
                    ev.xkey.keycode,
                    0,
                    0
                );

                we.keycode = x11_map_keysym(sym);
                break;

            case KeyRelease:
                we.type = PX_WE_KEYUP;

                sym = g_xlib.XkbKeycodeToKeysym(
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

static t_err_codes x11_engine_splash(void) {
    return ERR_SUCCESS; // TODO: Fix Splash Screen
    int logo_w, logo_h;
    int logo_bit_depth;
    int logo_color_type;

    unsigned char* logo_data;
    t_err_codes err = image_get_png("assets/icons/logo.png", &logo_w, &logo_h, &logo_bit_depth, &logo_color_type, &logo_data);
    if (err != ERR_SUCCESS)
        return err;

    for (int y = 0; y < logo_h; y++) {
        uint8_t* p = logo_data + y * logo_w * 4;
        for (int x = 0; x < logo_w; x++) {
            uint8_t a = p[3];
            p[0] = (p[0] * a) / 255;
            p[1] = (p[1] * a) / 255;
            p[2] = (p[2] * a) / 255;
            p += 4;
        }
    }

    for (int y = 0; y < logo_h; y++) {
        uint8_t* p = logo_data + y * logo_w * 4;
        for (int x = 0; x < logo_w; x++) {
            uint8_t r = p[0];
            uint8_t g = p[1];
            uint8_t b = p[2];
            uint8_t a = p[3];

            r = (r * a) / 255;
            g = (g * a) / 255;
            b = (b * a) / 255;

            p[0] = b;
            p[1] = g;
            p[2] = r;
            p[3] = a;

            p += 4;
        }
    }

    Window root = RootWindow(g_display, g_screen);

    XVisualInfo vinfo;
    if (!g_xlib.XMatchVisualInfo(g_display, g_screen, 32, TrueColor, &vinfo)) {
        free(logo_data);
        return ERR_WS_INIT_FAILED;
    }

    XSetWindowAttributes attrs = {0};
    attrs.colormap = g_xlib.XCreateColormap(
        g_display,
        root,
        vinfo.visual,
        AllocNone
    );
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;

    Window win = g_xlib.XCreateWindow(
        g_display, root,
        0, 0,
        600, 300,
        0, vinfo.depth,
        InputOutput, vinfo.visual,
        CWColormap | CWBorderPixel | CWBackPixel,
        &attrs
    );

    Atom wm_window_type = g_xlib.XInternAtom(g_display, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_window_type_splash = g_xlib.XInternAtom(g_display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    g_xlib.XChangeProperty(g_display, win, wm_window_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_window_type_splash, 1);
    Atom wm_state = g_xlib.XInternAtom(g_display, "_NET_WM_STATE", False);
    Atom wm_state_above = g_xlib.XInternAtom(g_display, "_NET_WM_STATE_ABOVE", False);
    Atom wm_state_skip_taskbar = g_xlib.XInternAtom(g_display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom wm_state_skip_pager = g_xlib.XInternAtom(g_display, "_NET_WM_STATE_SKIP_PAGER", False);

    Atom states[] = {
        wm_state_above,
        wm_state_skip_taskbar,
        wm_state_skip_pager
    };

    g_xlib.XChangeProperty(g_display, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 3);

    g_xlib.XSelectInput(g_display, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    g_xlib.XMapRaised(g_display, win);

    for (;;) {
        XEvent e;
        g_xlib.XNextEvent(g_display, &e);
        if (e.type == MapNotify && e.xmap.window == win) break;
    }
 
    int screen_w = DisplayWidth(g_display, g_screen);
    int screen_h = DisplayHeight(g_display, g_screen);

    int win_w = logo_w;
    int win_h = logo_h;

    g_xlib.XMoveWindow(g_display, win, (screen_w - win_w) / 2, (screen_h - win_h) / 2);

    Picture win_pic, img_pic;

    XRenderPictFormat* win_fmt = g_xlib.XRenderFindVisualFormat(g_display, vinfo.visual);
    win_pic = g_xlib.XRenderCreatePicture(g_display, win, win_fmt, 0, NULL);

    Pixmap bg_pix = g_xlib.XCreatePixmap(g_display, win, 600, 300, vinfo.depth);
    GC bg_gc = g_xlib.XCreateGC(g_display, bg_pix, 0, NULL);
    g_xlib.XSetForeground(g_display, bg_gc, BlackPixel(g_display, g_screen));
    g_xlib.XFillRectangle(g_display, bg_pix, bg_gc, 0, 0, 600, 300);

    Picture bg_pic = g_xlib.XRenderCreatePicture(g_display, bg_pix, g_xlib.XRenderFindVisualFormat(g_display, vinfo.visual), 0, NULL);
    g_xlib.XRenderComposite(g_display, PictOpSrc, bg_pic, None, win_pic, 0, 0, 0, 0, 0, 0, 600, 300);

    XRenderPictFormat* pix_fmt = g_xlib.XRenderFindStandardFormat(g_display, PictStandardARGB32);
    Pixmap pix = g_xlib.XCreatePixmap(g_display, win, logo_w, logo_h, 32);

    GC gc = g_xlib.XCreateGC(g_display, pix, 0, NULL);
    XImage* ximage = g_xlib.XCreateImage(
        g_display, vinfo.visual,
        32, ZPixmap, 0,
        (char*)logo_data, logo_w, logo_h, 32, logo_w * 4
    );
    g_xlib.XPutImage(g_display, pix, gc, ximage, 0, 0, 0, 0, logo_w, logo_h);
    
    img_pic = g_xlib.XRenderCreatePicture(g_display, pix, pix_fmt, 0, NULL);
 
    int logo_scaled_w = logo_w / 2;
    int logo_scaled_h = logo_h / 2;
    int dest_x = screen_w - logo_scaled_w - 50; // right-center offset
    int dest_y = (screen_h - logo_scaled_h) / 2;

    g_xlib.XRenderComposite(
        g_display, PictOpOver,
        img_pic, None,
        win_pic,
        0, 0,
        0, 0,
        dest_x, dest_y,
        logo_scaled_w, logo_scaled_h
    );

    g_xlib.XFlush(g_display);

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {
        while (g_xlib.XPending(g_display)) {
            XEvent e;
            g_xlib.XNextEvent(g_display, &e);
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed_ms >= 3000)
            break;

        nanosleep(&(struct timespec){0, 1000000}, NULL);
    }

    g_xlib.XDestroyWindow(g_display, win);
    g_xlib.XSync(g_display, False);

    if (img_pic) g_xlib.XRenderFreePicture(g_display, img_pic);
    if (win_pic) g_xlib.XRenderFreePicture(g_display, win_pic);
    g_xlib.XFreeColormap(g_display, attrs.colormap);
    g_xlib.XFreePixmap(g_display, pix);
    g_xlib.XFreeGC(g_display, gc);
    if (bg_pic) g_xlib.XRenderFreePicture(g_display, bg_pic);
    g_xlib.XFreePixmap(g_display, bg_pix);
    g_xlib.XFreeGC(g_display, bg_gc);

    ximage->data = NULL;
    XDestroyImage(ximage);

    free(logo_data);

    return ERR_SUCCESS;
}

static t_err_codes x11_window_design(PX_Window* win, PX_WindowDesign* design) {
    if (!win || !design) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

    Colormap cmap = iwin->colormap;
    XColor xcolor;
    xcolor.pixel = 0;
    xcolor.red = ((design->bg_color >> 16) & 0xFF) * 257;
    xcolor.green = ((design->bg_color >> 8) & 0xFF) * 257;
    xcolor.blue = (design->bg_color & 0xFF) * 257;
    g_xlib.XAllocColor(iwin->display, cmap, &xcolor);
    g_xlib.XSetWindowBackground(iwin->display, iwin->window, xcolor.pixel);
    g_xlib.XClearWindow(iwin->display, iwin->window);

    return ERR_SUCCESS;
}

static t_err_codes x11_create_ctx(PX_Window* win) {
	if (!g_xlib.gl || !g_xlib.glXCreateContextAttribsARB || !g_xlib.glXMakeCurrent || !g_xlib.glXDestroyContext) return ERR_WS_UNINITIALIZED;
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

	switch (win->gpu_backend_api) { // Safety Shield
		case PX_RS_GPU_BACKEND_OPENGL: break;
		case PX_RS_GPU_BACKEND_VULKAN: break;
		default: return ERR_WS_INVALID_GPU_BACKEND;
	}

	PX_WContext* ctx = (PX_WContext*)calloc(1, sizeof(PX_WContext));
	if (!ctx) return ERR_ALLOC_FAILED;

	ctx->iwin = (void*)iwin;
	ctx->backend = win->gpu_backend_api;

	switch (win->gpu_backend_api) {
		case PX_RS_GPU_BACKEND_OPENGL: {
			if (!iwin->glx_fb_config) {
				free(ctx);
				return ERR_WS_CONTEXT_CREATION_FAILED;
			}

			#ifdef PX_RS_OPENGL_PROFILE_COMPATIBILITY
				#define GL_PROFILE GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
			#else
				#define GL_PROFILE GLX_CONTEXT_CORE_PROFILE_BIT_ARB
			#endif
			int ctx_attribs[] = {
				GLX_CONTEXT_MAJOR_VERSION_ARB, PX_RS_OPENGL_MAJ_VERSION,
				GLX_CONTEXT_MINOR_VERSION_ARB, PX_RS_OPENGL_MIN_VERSION,
				GLX_CONTEXT_PROFILE_MASK_ARB, GL_PROFILE,
				None
			};
			#undef GL_PROFILE

			GLXContext gl_ctx = g_xlib.glXCreateContextAttribsARB(iwin->display, iwin->glx_fb_config, 0, True, ctx_attribs);
			if (!gl_ctx) {
				free(ctx);
				return ERR_WS_CONTEXT_CREATION_FAILED;
			}

			if (!g_xlib.glXMakeCurrent(iwin->display, iwin->window, gl_ctx)) {
				g_xlib.glXDestroyContext(iwin->display, gl_ctx);
				free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
			}

			iwin->gl_ctx_valid = true;
			iwin->gl_ctx = gl_ctx;

			if (glx_is_ext_supported(iwin->display, g_screen, "GLX_EXT_swap_control")) {
				if (g_xlib.glXSwapIntervalEXT) g_xlib.glXSwapIntervalEXT(iwin->display, g_xlib.glXGetCurrentDrawable(), win->vsync_off ? 0 : 1);
			}

			ctx->opengl.ictx = (void*)(&iwin->gl_ctx);
			break;
		}

		case PX_RS_GPU_BACKEND_VULKAN: {
			ctx->vulkan.required_extensions = (const char**)g_vk_instance_extensions;
			ctx->vulkan.required_extension_count = (size_t)(sizeof(g_vk_instance_extensions) / sizeof(const char*));
			break;
		}

		default: { // Can't occur but good safety
			free(ctx);
			return ERR_WS_INVALID_GPU_BACKEND;
		}
	}

	win->ctx_handle = (uint64_t)((uintptr_t)ctx);
    return ERR_SUCCESS;
}

static t_err_codes x11_get_ctx(PX_Window* win, PX_WContext* out) {
	if (!win || !out) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

	PX_WContext* ctx = (PX_WContext*)((uintptr_t)win->ctx_handle);
	if (!ctx) return ERR_WS_NO_CONTEXT_FOUND;

	*out = *ctx;
	return ERR_SUCCESS;
}

static t_err_codes x11_swap_buffers(PX_Window* win) {
	if (!g_xlib.gl || !g_xlib.glXSwapBuffers) return ERR_WS_UNINITIALIZED;
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;
	if (win->gpu_backend_api != PX_RS_GPU_BACKEND_OPENGL) return ERR_SUCCESS; // Control of buffer swap is with X11 only in OpenGL

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->gl_ctx_valid) return ERR_WS_NO_WINDOW_FOUND;

    g_xlib.glXSwapBuffers(iwin->display, iwin->window);

    return ERR_SUCCESS;
}

static char* x11_open_file_selector_dialog(void) {
    const char* file = tinyfd_openFileDialog("Select File", "", 0, NULL, NULL, 0);
    if (!file) return NULL;
    
    size_t sz = strlen(file);
    char* f = (char*)malloc(sz + 1);
    memcpy(f, file, sz);
    f[sz] = '\0';
    return f;
}

static t_err_codes x11_set_mouse_locked(PX_Window* win, bool locked) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
	if (!iwin) return ERR_WS_NO_WINDOW_FOUND;
    if (locked) {
        Pixmap blank;
        XColor dummy;
        char data[1] = {0};
        blank = g_xlib.XCreateBitmapFromData(iwin->display, iwin->window, data, 1, 1);
        Cursor invisible_cursor = g_xlib.XCreatePixmapCursor(iwin->display, blank, blank, &dummy, &dummy, 0, 0);

        g_xlib.XGrabPointer(
            iwin->display, iwin->window, True, 
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync, 
            iwin->window, invisible_cursor, CurrentTime
        );
        g_xlib.XFreePixmap(iwin->display, blank);
		g_xlib.XFreeCursor(iwin->display, invisible_cursor);
    } else {
        g_xlib.XUngrabPointer(iwin->display, CurrentTime);
        g_xlib.XFlush(iwin->display);
    }
    return ERR_SUCCESS;
}

static t_err_codes x11_set_mouse_pos(PX_Window* win, PX_Vector2 pos) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
	if (!iwin) return ERR_WS_NO_WINDOW_FOUND;
    g_xlib.XWarpPointer(iwin->display, None, iwin->window, 0, 0, 0, 0, pos.x, pos.y);
    return ERR_SUCCESS;
}

static t_err_codes x11_set_fullscreen(PX_Window* win, bool enabled) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

	struct window* iwin = get_window(win->handle);
	if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

	XEvent xev;
	Atom wm_state = g_xlib.XInternAtom(iwin->display, "_NET_WM_STATE", False);
	Atom wm_fs = g_xlib.XInternAtom(iwin->display, "_NET_WM_STATE_FULLSCREEN", False);

	memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = iwin->window;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = enabled ? 1 : 0; 
    xev.xclient.data.l[1] = wm_fs;
    xev.xclient.data.l[2] = 0;

    g_xlib.XSendEvent(iwin->display, DefaultRootWindow(iwin->display), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	return ERR_SUCCESS;
}

// Vulkan Specific
static t_err_codes x11_vk_finish_ctx(PX_WContext* ctx, VkInstance instance, PFN_vkGetInstanceProcAddr GetInstanceProcAddr) {
	if (!ctx || instance == VK_NULL_HANDLE) return ERR_INVALID_ARGUMENTS;
	
	struct window* iwin = (struct window*)ctx->iwin;
    if (!iwin) return ERR_WS_NO_WINDOW_FOUND;

	PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)GetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
	if (!CreateXlibSurfaceKHR) return ERR_WS_VK_FUNCTION_NOT_FOUND;

	VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .dpy = iwin->display,
        .window = iwin->window,
    };

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkResult result = CreateXlibSurfaceKHR(instance, &create_info, NULL, &surface);
	if (result != VK_SUCCESS) return ERR_WS_CONTEXT_CREATION_FAILED;

	ctx->vulkan.surfaceKHR = (PX_GPU_Handle)surface;
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
	.get_ctx = x11_get_ctx,
    .swap_buffers = x11_swap_buffers,
    .open_file_selector_dialog = x11_open_file_selector_dialog,
    .set_mouse_locked = x11_set_mouse_locked,
    .set_mouse_pos = x11_set_mouse_pos,
	.set_fullscreen = x11_set_fullscreen,

	.vk_finish_ctx = x11_vk_finish_ctx
};

