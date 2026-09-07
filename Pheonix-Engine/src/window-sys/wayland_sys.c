#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <window-sys.h>
#include <err-codes.h>
#include <event-sys.h>
#include <core/image.h>

#include <wayland-client.h>

#define HAVE_WAYLAND
#include <external/tinyfiledialogs.h>

#include <rendering-sys/opengl.h>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <rendering-sys/vulkan.h>

#include <window-sys/backends.h>

struct window {
	struct wl_display* display;
};

struct winarray {
    struct window* win;
    struct winarray* next;
    struct winarray* prev;
    
    int64_t handle;
};

static void wl_reg_handle_global(void* data, struct wl_registry* wl_registry, uint32_t name, const char* interface, uint32_t version);
static void wl_output_handle_mode(void* data, struct wl_output* wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);

static PX_Scale2 g_display_scale = (PX_Scale2){0};

static struct winarray* g_windows = NULL;
static struct wl_display* g_display = NULL;
static struct wl_registry* g_registry = NULL;
static struct wl_compositor* g_compositor = NULL;
static struct wl_shm* g_shm = NULL;
static struct wl_shm_pool* g_shm_pool = NULL;
static uint64_t g_shm_pool_size = 0;
static int64_t g_shm_fd = -1;

static int64_t g_handle = 0;

static const struct wl_registry_listener g_registry_listener = {
	.global = wl_reg_handle_global,
	.global_remove = NULL // Unused
};
static const struct wl_output_listener g_output_listener = {
	.geometry = NULL, // Unused
	.mode = wl_output_handle_mode,
	.done = NULL, // Unused
	.scale = NULL, // Unused
	.name = NULL, // Unused
	.description = NULL, // Unused
};

static int64_t append_window(struct window* win) {
    struct winarray* node = (struct winarray*)malloc(sizeof(struct winarray));
    if (!node) return -1;
    
    node->win = win;
    node->next = NULL;

    if (g_windows) {
        struct winarray* nxt = g_windows;
        for (size_t i = 0; i < PX_WS_MAX_WINDOWS; i++) {
            if (nxt->next) nxt = nxt->next;
            else  break;
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

static void remove_window(int handle) {
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
            // TODO: Destroy Context and Window
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

static void wl_reg_handle_global(void *data, struct wl_registry* wl_registry, uint32_t name, const char* interface, uint32_t version) {
	if (!wl_registry || !interface) return;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		wl_registry_bind(wl_registry, name, &wl_output_interface, version);
	}
}
static void wl_output_handle_mode(void* data, struct wl_output* wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    if (!wl_output) return;

    if (flags & WL_OUTPUT_MODE_CURRENT) {
		g_display_scale = (PX_Scale2){width, height};
    }
}

static void random_name(char* buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int64_t wl_create_shm(void) {
	size_t retries = 256;
	do {
		char name[] = "/wl_shm-PheonixEngine_WS_Wayland-XXXXXX";
		random_name(((char*)name + sizeof(name)) - 7); // Point to 'XXXXXX' area

		int64_t fd = (int64_t)shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}

		retries--;
	} while (retries > 0 && errno == EEXIST);
	
	return -1;
}

static int64_t wl_allocate_shm_file(size_t size) {
	int64_t fd = create_shm_file();
	if (fd < 0) return -1;
	
	int ret;
	do {
		ret = ftruncate((int)fd, size);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static t_err_codes wayland_init(void) {
    struct wl_display* display = wl_display_connect(NULL);
	if (!display) return ERR_WS_INIT_FAILED;
	struct wl_registry* reg = wl_display_get_registry(display);
	if (!reg) {
		wl_display_disconnect(display);
		return ERR_WS_INIT_FAILED;
	}
	
	if (wl_registry_add_listener(reg, &g_registry_listener, NULL) < 0) goto cleanup;
	if (wl_display_roundtrip(display) < 0) goto cleanup;
    if (wl_display_roundtrip(display) < 0) goto cleanup;
	if (g_display_scale.w < 1 || g_display_scale.h < 1 || !g_shm || !g_compositor) goto cleanup;

	uint32_t size = g_display_scale.w * g_display_scale.h * sizeof(uint32_t);
	int64_t shm_fd = wl_allocate_shm_file(size);
	if (shm_fd < 0) goto cleanup;

	uint32_t* pool_data = (uint32_t*)mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, (int)shm_fd, 0);
	if (!pool_data) {
		close(shm_fd);
		goto cleanup;
	}

	struct wl_shm_pool* pool = wl_shm_create_pool(g_shm, (int32_t)shm_fd, size);
	if (!pool) {
		munmap(pool_data, size);
		close(shm_fd);
		goto cleanup;
	}

	g_display = display;
	g_registry = reg;
	g_shm_fd = shm_fd;
	g_shm_pool = pool;
	g_shm_pool_size = size;
	return ERR_SUCCESS;

	cleanup: {
		wl_registry_destroy(reg);
		wl_display_disconnect(display);
		return ERR_WS_INIT_FAILED;
	}
}

static void wayland_shutdown(void) {
    if (g_windows) destroy_all_windows();

	if (g_registry) {
		wl_registry_destroy(g_registry);
		g_registry = NULL;
	}
    if (g_display) {
        wl_display_disconnect(g_display);
        g_display = NULL;
    }

	if (g_shm_pool) {
        munmap(g_shm_pool, g_shm_pool_size);
        g_shm_pool = NULL;
    }

	if (g_shm_fd) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
	
}

static t_err_codes x11_create(PX_Window* win, PX_GPU_Backend gpu_backend_api) {
	if (!win) return ERR_INVALID_ARGUMENTS;
    win->handle = -1;
	win->gpu_backend_api = gpu_backend_api;

    struct window* iwin = (struct window*)malloc(sizeof(struct window));
    if (!iwin) return ERR_ALLOC_FAILED;

	iwin->display = g_display;
}
