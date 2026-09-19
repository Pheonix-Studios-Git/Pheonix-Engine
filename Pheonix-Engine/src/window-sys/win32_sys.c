#ifdef _WIN32

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <windows.h>

#include <pheonix-engine.h>
#include <window-sys.h>
#include <err-codes.h>
#include <event-sys.h>
#include <core/image.h>

#include <external/tinyfiledialogs.h>

#include <rendering-sys/opengl.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <rendering-sys/vulkan.h>

#include <window-sys/backends.h>

#include <GL/gl.h>
#include <GL/wglext.h>

struct keysym_map {
    WPARAM sym;
    PX_EKeycodes key;
};

struct window {
	wchar_t* title;
	HWND hwnd;

	bool gl_ctx_valid;
	HDC gl_hdc;
	HGLRC gl_ctx;

	PX_WE_Queue* queue;
};

struct winarray {
    struct window* win;
    struct winarray* next;
    struct winarray* prev;
    
    int64_t handle;
};

typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int* attribList);
typedef BOOL (WINAPI *FNWGLSWAPINTERVALEXTPROC)(int);

static struct winarray* g_windows = NULL;
static int64_t g_handle = 0;

static WNDCLASSW g_class;

static const char* g_vk_instance_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME
};

static const struct keysym_map g_keysym_map[] = {
    { '0', EKeycode_0 }, { '5', EKeycode_5 },
    { '1', EKeycode_1 }, { '6', EKeycode_6 },
    { '2', EKeycode_2 }, { '7', EKeycode_7 },
    { '3', EKeycode_3 }, { '8', EKeycode_8 },
    { '4', EKeycode_4 }, { '9', EKeycode_9 },

    { 'Q', EKeycode_Q }, { 'F', EKeycode_F },
    { 'W', EKeycode_W }, { 'G', EKeycode_G },
    { 'E', EKeycode_E }, { 'H', EKeycode_H },
    { 'R', EKeycode_R }, { 'J', EKeycode_J },
    { 'T', EKeycode_T }, { 'K', EKeycode_K },
    { 'Y', EKeycode_Y }, { 'L', EKeycode_L },
    { 'U', EKeycode_U }, { 'Z', EKeycode_Z },
    { 'I', EKeycode_I }, { 'X', EKeycode_X },
    { 'O', EKeycode_O }, { 'C', EKeycode_C },
    { 'P', EKeycode_P }, { 'V', EKeycode_V },
    { 'A', EKeycode_A }, { 'B', EKeycode_B },
    { 'S', EKeycode_S }, { 'N', EKeycode_N },
    { 'D', EKeycode_D }, { 'M', EKeycode_M },

    { VK_LWIN, EKeycode_Super },
    { VK_RWIN, EKeycode_Super },

    { VK_CAPITAL, EKeycode_Capslock },
    { VK_NUMLOCK, EKeycode_Numlock },

    { VK_TAB, EKeycode_Tab },
    { VK_SPACE, EKeycode_Space },
    { VK_BACK, EKeycode_Backspace },
    { VK_ESCAPE, EKeycode_Escape },

    { VK_INSERT, EKeycode_Insert },
    { VK_DELETE, EKeycode_Delete },
    { VK_HOME, EKeycode_Home },
    { VK_END, EKeycode_End },
    { VK_PRIOR, EKeycode_PageUp },
    { VK_NEXT, EKeycode_PageDown },

    { VK_UP, EKeycode_UpArrow },
    { VK_DOWN, EKeycode_DownArrow },
    { VK_LEFT, EKeycode_LeftArrow },
    { VK_RIGHT, EKeycode_RightArrow },

    { VK_NUMPAD0, EKeycode_Num0 },
    { VK_NUMPAD1, EKeycode_Num1 },
    { VK_NUMPAD2, EKeycode_Num2 },
    { VK_NUMPAD3, EKeycode_Num3 },
    { VK_NUMPAD4, EKeycode_Num4 },
    { VK_NUMPAD5, EKeycode_Num5 },
    { VK_NUMPAD6, EKeycode_Num6 },
    { VK_NUMPAD7, EKeycode_Num7 },
    { VK_NUMPAD8, EKeycode_Num8 },
    { VK_NUMPAD9, EKeycode_Num9 },

    { VK_ADD, EKeycode_NumPlus },
    { VK_SUBTRACT, EKeycode_NumDash },
    { VK_MULTIPLY, EKeycode_NumAsterik },
    { VK_DIVIDE, EKeycode_NumSlash },
    { VK_DECIMAL, EKeycode_NumPoint },

    { VK_F1, EKeycode_F1 },
    { VK_F2, EKeycode_F2 },
    { VK_F3, EKeycode_F3 },
    { VK_F4, EKeycode_F4 },
    { VK_F5, EKeycode_F5 },
    { VK_F6, EKeycode_F6 },
    { VK_F7, EKeycode_F7 },
    { VK_F8, EKeycode_F8 },
    { VK_F9, EKeycode_F9 },
    { VK_F10, EKeycode_F10 },
    { VK_F11, EKeycode_F11 },
    { VK_F12, EKeycode_F12 },

    { VK_OEM_3, EKeycode_Backtick },
    { VK_OEM_MINUS, EKeycode_Dash },
    { VK_OEM_PLUS, EKeycode_Equals },
    { VK_OEM_4, EKeycode_SqBracketOpen },
    { VK_OEM_6, EKeycode_SqBracketClose },
    { VK_OEM_5, EKeycode_Backslash },
    { VK_OEM_1, EKeycode_Semicolon },
    { VK_OEM_7, EKeycode_SingleQuotes },
    { VK_OEM_COMMA, EKeycode_Comma },
    { VK_OEM_PERIOD, EKeycode_Fullstop },
    { VK_OEM_2, EKeycode_Slash },
};

static int64_t append_window(struct window* win) {
    struct winarray* node = (struct winarray*)calloc(1, sizeof(struct winarray));
    if (!node) return -1;
    
    node->win = win;
    node->next = NULL;

    if (g_windows) {
        struct winarray* nxt = g_windows;
        for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
            if (nxt->next) nxt = nxt->next;
            else break;
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

static struct window* get_window(int64_t handle) {
    if (!g_windows) return NULL;

    struct winarray* nxt = g_windows;
    for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
        if (nxt->handle == handle) return nxt->win;
        if (nxt->next) nxt = nxt->next;
        else break;
    }

    return NULL;
}

static void remove_window(int64_t handle) {
    if (!g_windows) return;

    struct winarray* nxt = g_windows;
    bool found = false;
    for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
        if (nxt->handle == handle) {
            found = true;
            break;
        }

        if (nxt->next) nxt = nxt->next;
        else break;
    }

    if (found) {
        if (nxt->prev) nxt->prev->next = nxt->next;
        else g_windows = nxt->next;

        if (nxt->next) nxt->next->prev = nxt->prev;

        free(nxt);
    }
}

static void destroy_all_windows(void) {
    if (!g_windows) return;

    struct winarray* nxt = g_windows;
    for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
        if (nxt->win) {
			if (nxt->win->gl_ctx_valid) {
				if (nxt->win->gl_ctx) {
					wglMakeCurrent(NULL, NULL);
					wglDeleteContext(nxt->win->gl_ctx);
				}
				if (nxt->win->gl_hdc) ReleaseDC(nxt->win->hwnd, nxt->win->gl_hdc);
			}
            if (nxt->win->hwnd) DestroyWindow(nxt->win->hwnd);
			if (nxt->win->title) free(nxt->win->title);
            free(nxt->win);
        }
        if (nxt->next) nxt = nxt->next;
        else break;
    }

    struct winarray* cur = nxt;
    for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
        if (nxt->prev) nxt = nxt->prev;
        free(cur);
        cur = nxt;
    }
}

static PX_EKeycodes win32_map_keysym(WPARAM sym, LPARAM lParam) {
	switch (sym) {
		case VK_RETURN: {
			if (lParam & (1UL << 24)) return EKeycode_NumEnter;
        	return EKeycode_Enter;
		}

		case VK_SHIFT: {
			UINT scan = (UINT)((lParam >> 16) & 0xff);
			UINT extended = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);

			if (extended == VK_RSHIFT) return EKeycode_RShift;
			return EKeycode_LShift;
		}
		case VK_CONTROL: {
			if (lParam & (1UL << 24)) return EKeycode_RControl;
			return EKeycode_LControl;
		}
		case VK_MENU: {
			if (lParam & (1UL << 24)) return EKeycode_RAlternate;
			return EKeycode_LAlternate;
		}

		default: break;
	}

    for (size_t i = 0; i < sizeof(g_keysym_map)/sizeof(g_keysym_map[0]); i++) {
        if (g_keysym_map[i].sym == sym)
            return g_keysym_map[i].key;
    }
    return EKeycode_Unknown;
}

static PX_EKeycodes win32_map_mousesym(UINT message, WPARAM wParam) {
    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK: return EKeycode_MouseLButton;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK: return EKeycode_MouseMButton;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK: return EKeycode_MouseRButton;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK: {
            switch (GET_XBUTTON_WPARAM(wParam)) {
                case XBUTTON1: return EKeycode_MouseX1;
                case XBUTTON2: return EKeycode_MouseX2;
                default: return EKeycode_Unknown;
            }
		}
        case WM_MOUSEWHEEL: {
            if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) return EKeycode_MouseScrollUp;
            if (GET_WHEEL_DELTA_WPARAM(wParam) < 0) return EKeycode_MouseScrollDown;
            return EKeycode_Unknown;
		}
        case WM_MOUSEHWHEEL: {
            if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) return EKeycode_MouseScrollRight;
            if (GET_WHEEL_DELTA_WPARAM(wParam) < 0) return EKeycode_MouseScrollLeft;
            return EKeycode_Unknown;
		}

        default: return EKeycode_Unknown;
    }
}

static LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    struct window* iwin = (struct window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	
	switch (msg) {
		case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;

            iwin = (struct window*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)iwin);

            iwin->hwnd = hwnd;
            return TRUE;
        }

        case WM_SIZE: {
            if (!iwin) break;
            PX_WEvent we = {
				.type = PX_WE_RESIZE,
				.w = (int)LOWORD(lParam),
				.h = (int)HIWORD(lParam)
			};

            px_we_queue_push(iwin->queue, &we);
            return 0;
        }

        case WM_CLOSE: {
            if (!iwin) break;
            PX_WEvent we = {
				.type = PX_WE_CLOSE
			};
			
            px_we_queue_push(iwin->queue, &we);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (!iwin) break;
            PX_WEvent we = {
				.type = PX_WE_KEYDOWN,
				.keycode = win32_map_keysym(wParam, lParam)
			};

            px_we_queue_push(iwin->queue, &we);
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (!iwin) break;

            PX_WEvent we = {
				.type = PX_WE_KEYUP,
				.keycode = win32_map_keysym(wParam, lParam)
			};

        	px_we_queue_push(iwin->queue, &we);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_XBUTTONDOWN: {
            if (!iwin) break;

            PX_WEvent we = {
				.type = PX_WE_MOUSE_DOWN,
				.keycode = win32_map_mousesym(msg, wParam),
				.x = (int)(short)LOWORD(lParam),
				.y = (int)(short)HIWORD(lParam)
			};
            px_we_queue_push(iwin->queue, &we);

            if (msg == WM_XBUTTONDOWN) return TRUE;
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_XBUTTONUP: {
            if (!iwin) break;

            PX_WEvent we = {
				.type = PX_WE_MOUSE_UP,
				.keycode = win32_map_mousesym(msg, wParam),
				.x = (int)(short)LOWORD(lParam),
				.y = (int)(short)HIWORD(lParam)
			};
            px_we_queue_push(iwin->queue, &we);

            if (msg == WM_XBUTTONUP) return TRUE;
            return 0;
        }
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_XBUTTONDBLCLK: {
            if (msg == WM_XBUTTONDBLCLK) return TRUE;
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!iwin) break;
            PX_WEvent we = {
				.type = PX_WE_MOUSE_MOVE,
				.x = (int)(short)LOWORD(lParam),
				.y = (int)(short)HIWORD(lParam)
			};

            px_we_queue_push(iwin->queue, &we);
            return 0;
        }
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            if (!iwin) break;
            PX_WEvent we = {
				.type = PX_WE_MOUSE_DOWN,
				.keycode = win32_map_mousesym(msg, wParam)
			};

            POINT point = {
                .x = (int)(short)LOWORD(lParam),
                .y = (int)(short)HIWORD(lParam)
            };
            ScreenToClient(hwnd, &point);

            we.x = point.x;
            we.y = point.y;
            px_we_queue_push(iwin->queue, &we);

            return 0;
        }

        case WM_SETFOCUS: return 0;
        case WM_KILLFOCUS: return 0;
        case WM_DESTROY: return 0;
        case WM_NCDESTROY: {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
		}

		default: break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static t_err_codes win32_init(void) {
	g_class = (WNDCLASSW){
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc=win32_window_proc,
		.hInstance = g_main_hinstance,
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.lpszMenuName = L"Pheonix Engine",
		.lpszClassName = L"Pheonix Engine"
	};

	if (!RegisterClassW(&g_class)) {
		return ERR_WS_INIT_FAILED;
	}
	return ERR_SUCCESS;
}

static void win32_shutdown(void) {
    destroy_all_windows();

	if (g_class.lpszClassName) UnregisterClassW(g_class.lpszClassName, g_main_hinstance);
	memset(&g_class, 0, sizeof(WNDCLASSW));
}

static t_err_codes win32_create(PX_Window* win, PX_GPU_Backend gpu_backend_api) {
	if (!win) return ERR_INVALID_ARGUMENTS;
    win->handle = -1;
	win->gpu_backend_api = gpu_backend_api;

	struct window* iwin = (struct window*)calloc(1, sizeof(struct window));
    if (!iwin) return ERR_ALLOC_FAILED;
	iwin->queue = &win->queue;

	const char* title = win->title ? win->title : "Pheonix Engine - Unknown Window";

	int buffer_size = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
	if (buffer_size <= 0) {
		free(iwin);
		return ERR_WS_WINDOW_CREATION_FAILED;
	}
	
	wchar_t* wide_title = (wchar_t*)calloc(buffer_size, sizeof(wchar_t));
	if (!wide_title) {
		free(iwin);
		return ERR_ALLOC_FAILED;
	}

	if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, buffer_size) == 0) {
		free(wide_title);
		free(iwin);
		return ERR_WS_WINDOW_CREATION_FAILED;
	}

	iwin->title = wide_title;

	HWND hwnd = CreateWindowExW(0, L"Pheonix Engine", wide_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, win->width, win->height, NULL, NULL, g_main_hinstance, iwin);
	if (hwnd == 0) {
		free(iwin);
		free(wide_title);
		return ERR_WS_WINDOW_CREATION_FAILED;
	}

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	iwin->hwnd = hwnd;

	int64_t handle = append_window(iwin);
	if (handle < 0) {
		DestroyWindow(hwnd);
		free(iwin);
		free(wide_title);
		return ERR_WS_WINDOW_CREATION_FAILED;
	}

	win->handle = handle;
	return ERR_SUCCESS;
}

static void win32_destroy(PX_Window* win) {
    if (!win) return;

	struct window* iwin = get_window(win->handle);
    if (!iwin) return;

	if (iwin->gl_ctx_valid) {
		if (iwin->gl_ctx) {
			wglMakeCurrent(NULL, NULL);
            wglDeleteContext(iwin->gl_ctx);
		}
		if (iwin->gl_hdc) ReleaseDC(iwin->hwnd, iwin->gl_hdc);
	}
	if (iwin->hwnd) DestroyWindow(iwin->hwnd);
	if (iwin->title) free(iwin->title);
	free(iwin);

	remove_window(win->handle);
    win->handle = -1;
}

static t_err_codes win32_show(PX_Window* win) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

    ShowWindow(iwin->hwnd, SW_SHOW);
    UpdateWindow(iwin->hwnd);

    return ERR_SUCCESS;
}

static t_err_codes win32_hide(PX_Window* win) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

    ShowWindow(iwin->hwnd, SW_HIDE);
    UpdateWindow(iwin->hwnd);

    return ERR_SUCCESS;
}

static t_err_codes win32_poll_events(PX_Window* win) {
    if (!win) return ERR_INVALID_ARGUMENTS;
    if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

	MSG msg;
	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return ERR_SUCCESS;
}

static t_err_codes win32_engine_splash(void) {
	return ERR_SUCCESS;
}

static t_err_codes win32_window_design(PX_Window* win, PX_WindowDesign* design) {
	(void)win;
	(void)design;
    return ERR_SUCCESS;
}

static t_err_codes win32_create_ctx(PX_Window* win) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

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
			HDC hdc = GetDC(iwin->hwnd);
			if (!hdc) {
				free(ctx);
				return ERR_WS_CONTEXT_CREATION_FAILED;
			}

			PIXELFORMATDESCRIPTOR pfd = {
                .nSize = sizeof(PIXELFORMATDESCRIPTOR),
                .nVersion = 1,
                .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                .iPixelType = PFD_TYPE_RGBA,
                .cColorBits = 32,
                .cDepthBits = 24,
                .cStencilBits = 8,
                .iLayerType = PFD_MAIN_PLANE
            };

			int pixel_format = ChoosePixelFormat(hdc, &pfd);
            if (!pixel_format) {
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }
            if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }

			HGLRC temp_ctx = wglCreateContext(hdc);
            if (!temp_ctx) {
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }

            if (!wglMakeCurrent(hdc, temp_ctx)) {
                wglDeleteContext(temp_ctx);
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }

            
			PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			if (!wglCreateContextAttribsARB) {
				fprintf(stderr, "[Win32] Failed to get 'wglCreateContextAttribsARB' function!\n");

				wglMakeCurrent(NULL, NULL);
                wglDeleteContext(temp_ctx);
				ReleaseDC(iwin->hwnd, hdc);
				free(ctx);
				return ERR_WS_CONTEXT_CREATION_FAILED;
			}

			#ifdef PX_RS_OPENGL_PROFILE_COMPATIBILITY
            	int profile = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
			#else
            	int profile = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
			#endif

            int ctx_attribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB,
                PX_RS_OPENGL_MAJ_VERSION,
                WGL_CONTEXT_MINOR_VERSION_ARB,
                PX_RS_OPENGL_MIN_VERSION,
                WGL_CONTEXT_PROFILE_MASK_ARB,
                profile,
                0
            };

            HGLRC gl_ctx = wglCreateContextAttribsARB(hdc, NULL, ctx_attribs);
            if (!gl_ctx) {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(temp_ctx);
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }

            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(temp_ctx);

            if (!wglMakeCurrent(hdc, gl_ctx)) {
                wglDeleteContext(gl_ctx);
                ReleaseDC(iwin->hwnd, hdc);
                free(ctx);
                return ERR_WS_CONTEXT_CREATION_FAILED;
            }

            iwin->gl_ctx_valid = true;
            iwin->gl_ctx = gl_ctx;
			iwin->gl_hdc = hdc;

            PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
            if (wglSwapIntervalEXT) wglSwapIntervalEXT(win->vsync_off ? 0 : 1);

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

static t_err_codes win32_get_ctx(PX_Window* win, PX_WContext* out) {
	if (!win || !out) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

	PX_WContext* ctx = (PX_WContext*)((uintptr_t)win->ctx_handle);
	if (!ctx) return ERR_WS_NO_CONTEXT_FOUND;

	*out = *ctx;
	return ERR_SUCCESS;
}

static t_err_codes win32_swap_buffers(PX_Window* win) {
    if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;
	if (win->gpu_backend_api != PX_RS_GPU_BACKEND_OPENGL) return ERR_SUCCESS; // Control of buffer swap is with Win32 only in OpenGL

    struct window* iwin = get_window(win->handle);
	if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;
    if (!iwin->gl_ctx_valid || !iwin->gl_hdc) return ERR_WS_NO_CONTEXT_FOUND;

    if (!SwapBuffers(iwin->gl_hdc)) return ERR_FAILURE;
    return ERR_SUCCESS;
}

static char* win32_open_file_selector_dialog(void) {
    const char* file = tinyfd_openFileDialog("Select File", "", 0, NULL, NULL, 0);
    if (!file) return NULL;
    
    size_t sz = strlen(file);
    char* f = (char*)malloc(sz + 1);
	if (!f) return NULL;

    memcpy(f, file, sz);
    f[sz] = '\0';
    return f;
}

static t_err_codes win32_set_mouse_locked(PX_Window* win, bool locked) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

    if (locked) {
        RECT rect;
        if (!GetClientRect(iwin->hwnd, &rect)) return ERR_FAILURE;

        POINT top_left = {rect.left, rect.top};
        POINT bottom_right = {rect.right, rect.bottom};

        ClientToScreen(iwin->hwnd, &top_left);
        ClientToScreen(iwin->hwnd, &bottom_right);

        rect.left = top_left.x;
        rect.top = top_left.y;
        rect.right = bottom_right.x;
        rect.bottom = bottom_right.y;

        ClipCursor(&rect);
        ShowCursor(FALSE);
    } else {
        ClipCursor(NULL);
        ShowCursor(TRUE);
    }

    return ERR_SUCCESS;
}

static t_err_codes win32_set_mouse_pos(PX_Window* win, PX_Vector2 pos) {
	if (!win) return ERR_INVALID_ARGUMENTS;
	if (win->handle < 0) return ERR_WS_INVALID_WINDOW_HANDLE;

    struct window* iwin = get_window(win->handle);
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

    POINT point = {
		.x=(LONG)pos.x,
		.y=(LONG)pos.y
	};

    if (!ClientToScreen(iwin->hwnd, &point)) return ERR_FAILURE;
    if (!SetCursorPos(point.x, point.y)) return ERR_FAILURE;
    return ERR_SUCCESS;
}

static t_err_codes win32_set_fullscreen(PX_Window* win, bool enabled) {
	(void)win;
	(void)enabled;
	return ERR_SUCCESS;
}

// Vulkan Specific
static t_err_codes win32_vk_finish_ctx(PX_WContext* ctx, VkInstance instance, PFN_vkGetInstanceProcAddr GetInstanceProcAddr) {
	if (!ctx || instance == VK_NULL_HANDLE) return ERR_INVALID_ARGUMENTS;
	
	struct window* iwin = (struct window*)ctx->iwin;
    if (!iwin || !iwin->hwnd) return ERR_WS_NO_WINDOW_FOUND;

	PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)GetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
	if (!CreateWin32SurfaceKHR) return ERR_WS_VK_FUNCTION_NOT_FOUND;

	VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
		.hinstance = g_class.hInstance,
		.hwnd = iwin->hwnd
    };

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkResult result = CreateWin32SurfaceKHR(instance, &create_info, NULL, &surface);
	if (result != VK_SUCCESS) return ERR_WS_CONTEXT_CREATION_FAILED;

	ctx->vulkan.surfaceKHR = (PX_GPU_Handle)surface;
	return ERR_SUCCESS;
}

const t_px_ws_backend px_ws_backend_win32 = {
    .init = win32_init,
    .shutdown = win32_shutdown,
    .create = win32_create,
    .destroy = win32_destroy,
    .show = win32_show,
    .hide = win32_hide,
    .poll_events = win32_poll_events,
    .show_splash = win32_engine_splash,
    .window_design = win32_window_design,
    .create_ctx = win32_create_ctx,
	.get_ctx = win32_get_ctx,
    .swap_buffers = win32_swap_buffers,
    .open_file_selector_dialog = win32_open_file_selector_dialog,
    .set_mouse_locked = win32_set_mouse_locked,
    .set_mouse_pos = win32_set_mouse_pos,
	.set_fullscreen = win32_set_fullscreen,

	.vk_finish_ctx = win32_vk_finish_ctx
};

#endif