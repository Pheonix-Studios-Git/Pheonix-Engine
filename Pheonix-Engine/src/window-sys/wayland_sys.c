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
#include <dlfcn.h>

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

// Wayland Functions
typedef void (*PFN_wl_event_queue_destroy)(struct wl_event_queue *queue);
typedef const char *(*PFN_wl_event_queue_get_name)(const struct wl_event_queue *queue);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_flags)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, ...);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_array_flags)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, uint32_t flags, union wl_argument *args);
typedef void (*PFN_wl_proxy_marshal)(struct wl_proxy *p, uint32_t opcode, ...);
typedef void (*PFN_wl_proxy_marshal_array)(struct wl_proxy *p, uint32_t opcode, union wl_argument *args);
typedef struct wl_proxy *(*PFN_wl_proxy_create)(struct wl_proxy *factory, const struct wl_interface *interface);
typedef void *(*PFN_wl_proxy_create_wrapper)(void *proxy);
typedef void (*PFN_wl_proxy_wrapper_destroy)(void *proxy_wrapper);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_constructor)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, ...);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_constructor_versioned)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_array_constructor)(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface);
typedef struct wl_proxy *(*PFN_wl_proxy_marshal_array_constructor_versioned)(struct wl_proxy *proxy, uint32_t opcode, union wl_argument *args, const struct wl_interface *interface, uint32_t version);
typedef void (*PFN_wl_proxy_destroy)(struct wl_proxy *proxy);
typedef int (*PFN_wl_proxy_add_listener)(struct wl_proxy *proxy, void (**implementation)(void), void *data);
typedef const void *(*PFN_wl_proxy_get_listener)(struct wl_proxy *proxy);
typedef int (*PFN_wl_proxy_add_dispatcher)(struct wl_proxy *proxy, wl_dispatcher_func_t dispatcher_func, const void * dispatcher_data, void *data);
typedef void (*PFN_wl_proxy_set_user_data)(struct wl_proxy *proxy, void *user_data);
typedef void *(*PFN_wl_proxy_get_user_data)(struct wl_proxy *proxy);
typedef uint32_t (*PFN_wl_proxy_get_version)(struct wl_proxy *proxy);
typedef uint32_t (*PFN_wl_proxy_get_id)(struct wl_proxy *proxy);
typedef void (*PFN_wl_proxy_set_tag)(struct wl_proxy *proxy, const char * const *tag);
typedef const char * const *(*PFN_wl_proxy_get_tag)(struct wl_proxy *proxy);
typedef const char *(*PFN_wl_proxy_get_class)(struct wl_proxy *proxy);
typedef const struct wl_interface *(*PFN_wl_proxy_get_interface)(struct wl_proxy *proxy);
typedef struct wl_display *(*PFN_wl_proxy_get_display)(struct wl_proxy *proxy);
typedef void (*PFN_wl_proxy_set_queue)(struct wl_proxy *proxy, struct wl_event_queue *queue);
typedef struct wl_event_queue *(*PFN_wl_proxy_get_queue)(const struct wl_proxy *proxy);
typedef struct wl_display *(*PFN_wl_display_connect)(const char *name); 
typedef struct wl_display *(*PFN_wl_display_connect_to_fd)(int fd);
typedef void (*PFN_wl_display_disconnect)(struct wl_display *display);
typedef int (*PFN_wl_display_get_fd)(struct wl_display *display);
typedef int (*PFN_wl_display_dispatch)(struct wl_display *display);
typedef int (*PFN_wl_display_dispatch_queue)(struct wl_display *display, struct wl_event_queue *queue);
typedef int (*PFN_wl_display_dispatch_timeout)(struct wl_display *display, const struct timespec *timeout);
typedef int (*PFN_wl_display_dispatch_queue_timeout)(struct wl_display *display, struct wl_event_queue *queue, const struct timespec *timeout);
typedef int (*PFN_wl_display_dispatch_queue_pending)(struct wl_display *display, struct wl_event_queue *queue);
typedef int (*PFN_wl_display_dispatch_queue_pending_single)(struct wl_display *display, struct wl_event_queue *queue);
typedef int (*PFN_wl_display_dispatch_pending)(struct wl_display *display);
typedef int (*PFN_wl_display_dispatch_pending_single)(struct wl_display *display);
typedef int (*PFN_wl_display_get_error)(struct wl_display *display);
typedef uint32_t (*PFN_wl_display_get_protocol_error)(struct wl_display *display, const struct wl_interface **interface, uint32_t *id);
typedef int (*PFN_wl_display_flush)(struct wl_display *display);
typedef int (*PFN_wl_display_roundtrip_queue)(struct wl_display *display, struct wl_event_queue *queue);
typedef int (*PFN_wl_display_roundtrip)(struct wl_display *display);
typedef struct wl_event_queue *(*PFN_wl_display_create_queue)(struct wl_display *display);
typedef struct wl_event_queue *(*PFN_wl_display_create_queue_with_name)(struct wl_display *display, const char *name);
typedef int (*PFN_wl_display_prepare_read_queue)(struct wl_display *display, struct wl_event_queue *queue);
typedef int (*PFN_wl_display_prepare_read)(struct wl_display *display);
typedef void (*PFN_wl_display_cancel_read)(struct wl_display *display);
typedef int (*PFN_wl_display_read_events)(struct wl_display *display);
typedef void (*PFN_wl_display_set_max_buffer_size)(struct wl_display *display, size_t max_buffer_size);
typedef void (*PFN_wl_log_set_handler_client)(wl_log_func_t handler);

struct window {
	struct wl_display* display;
};

struct winarray {
    struct window* win;
    struct winarray* next;
    struct winarray* prev;
    
    int64_t handle;
};

struct wayland_lib {
	PFN_wl_event_queue_destroy event_queue_destroy;
	PFN_wl_event_queue_get_name event_queue_get_name;

	PFN_wl_proxy_marshal_flags proxy_marshal_flags;
	PFN_wl_proxy_marshal_array_flags proxy_marshal_array_flags;
	PFN_wl_proxy_marshal proxy_marshal;
	PFN_wl_proxy_marshal_array proxy_marshal_array;
	PFN_wl_proxy_create proxy_create;
	PFN_wl_proxy_create_wrapper proxy_create_wrapper;
	PFN_wl_proxy_wrapper_destroy proxy_wrapper_destroy;
	PFN_wl_proxy_marshal_constructor proxy_marshal_constructor;
	PFN_wl_proxy_marshal_constructor_versioned proxy_marshal_constructor_versioned;
	PFN_wl_proxy_marshal_array_constructor proxy_marshal_array_constructor;
	PFN_wl_proxy_marshal_array_constructor_versioned proxy_marshal_array_constructor_versioned;
	PFN_wl_proxy_destroy proxy_destroy;
	PFN_wl_proxy_add_listener proxy_add_listener;
	PFN_wl_proxy_get_listener proxy_get_listener;
	PFN_wl_proxy_add_dispatcher proxy_add_dispatcher;
	PFN_wl_proxy_set_user_data proxy_set_user_data;
	PFN_wl_proxy_get_user_data proxy_get_user_data;
	PFN_wl_proxy_get_version proxy_get_version;
	PFN_wl_proxy_get_id proxy_get_id;
	PFN_wl_proxy_set_tag proxy_set_tag;
	PFN_wl_proxy_get_tag proxy_get_tag;
	PFN_wl_proxy_get_class proxy_get_class;
	PFN_wl_proxy_get_interface proxy_get_interface;
	PFN_wl_proxy_get_display proxy_get_display;
	PFN_wl_proxy_set_queue proxy_set_queue;
	PFN_wl_proxy_get_queue proxy_get_queue;

	PFN_wl_display_connect display_connect;
	PFN_wl_display_connect_to_fd display_connect_to_fd;
	PFN_wl_display_disconnect display_disconnect;
	PFN_wl_display_get_fd display_get_fd;
	PFN_wl_display_dispatch display_dispatch;
	PFN_wl_display_dispatch_queue display_dispatch_queue;
	PFN_wl_display_dispatch_timeout display_dispatch_timeout;
	PFN_wl_display_dispatch_queue_timeout display_dispatch_queue_timeout;
	PFN_wl_display_dispatch_queue_pending display_dispatch_queue_pending;
	PFN_wl_display_dispatch_queue_pending_single display_dispatch_queue_pending_single;
	PFN_wl_display_dispatch_pending display_dispatch_pending;
	PFN_wl_display_dispatch_pending_single display_dispatch_pending_single;
	PFN_wl_display_get_error display_get_error;
	PFN_wl_display_get_protocol_error display_get_protocol_error;
	PFN_wl_display_flush display_flush;
	PFN_wl_display_roundtrip_queue display_roundtrip_queue;
	PFN_wl_display_roundtrip display_roundtrip;
	PFN_wl_display_create_queue display_create_queue;
	PFN_wl_display_create_queue_with_name display_create_queue_with_name;
	PFN_wl_display_prepare_read_queue display_prepare_read_queue;
	PFN_wl_display_prepare_read display_prepare_read;
	PFN_wl_display_cancel_read display_cancel_read;
	PFN_wl_display_read_events display_read_events;
	PFN_wl_display_set_max_buffer_size display_set_max_buffer_size;

	PFN_wl_log_set_handler_client log_set_handler_client;

	const struct wl_interface* registry_interface;
	const struct wl_interface* compositor_interface;
	const struct wl_interface* shm_interface;
	const struct wl_interface* shm_pool_interface;
	const struct wl_interface* output_interface;

	void* lib;
};

static void wl_reg_handle_global(void* data, struct wl_registry* wl_registry, uint32_t name, const char* interface, uint32_t version);
static void wl_output_handle_mode(void* data, struct wl_output* wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh);

static struct wayland_lib g_wl_lib = {0};

static PX_Scale2 g_display_scale = (PX_Scale2){0};

static struct winarray* g_windows = NULL;
static struct wl_display* g_display = NULL;
static struct wl_registry* g_registry = NULL;
static struct wl_compositor* g_compositor = NULL;
static struct wl_output* g_output = NULL;
static struct wl_shm* g_shm = NULL;
static struct wl_shm_pool* g_shm_pool = NULL;
static uint8_t* g_shm_pool_data = NULL;
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

static void wl_reg_handle_global(void* data, struct wl_registry* wl_registry, uint32_t name, const char* interface, uint32_t version) {
	if (!wl_registry || !interface || !g_wl_lib.lib) return;

	if (strcmp(interface, g_wl_lib.compositor_interface->name) == 0) {
		g_compositor = (struct wl_compositor*)((void*)g_wl_lib.proxy_marshal_flags((struct wl_proxy*)wl_registry, WL_REGISTRY_BIND,  g_wl_lib.compositor_interface, version, 0, name, interface, version, NULL));
	} else if (strcmp(interface, g_wl_lib.shm_interface->name) == 0) {
		g_shm = (struct wl_shm*)((void*)g_wl_lib.proxy_marshal_flags((struct wl_proxy*)wl_registry, WL_REGISTRY_BIND, g_wl_lib.shm_interface, version, 0, name, interface, version, NULL));
	} else if (strcmp(interface, g_wl_lib.output_interface->name) == 0) {
		g_output = (struct wl_output*)((void*)g_wl_lib.proxy_marshal_flags((struct wl_proxy*)wl_registry, WL_REGISTRY_BIND, g_wl_lib.output_interface, version, 0, name, interface, version, NULL));
		if (g_wl_lib.proxy_add_listener((struct wl_proxy*)g_output, (void (**)(void))&g_output_listener, NULL) < 0) g_output = NULL;
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
	int64_t fd = wl_create_shm();
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
	// Load lib
	g_wl_lib.lib = dlopen("libwayland-client.so.0", RTLD_LAZY);
	if (!g_wl_lib.lib) {
		fprintf(stderr, "[Wayland] Failed to load library 'libwayland-client.so.0': %s\n", dlerror());
		return ERR_WS_INIT_FAILED;
	}

	#define WL_LOAD_SYMBOL(lib, fn, name) { \
		*(void**)(&(fn)) = dlsym((lib), (name)); \
		if (!(fn)) { \
			fprintf(stderr, "[Wayland] Failed to find symbol '%s'\n", (name)); \
			goto lib_load_fail; \
		} \
	}

	#define WL_LOAD_DATA_SYMBOL(lib, var, name) { \
		*(void**)(&(var)) = dlsym((lib), (name)); \
		if (!(var)) { \
			fprintf(stderr, "[Wayland] Failed to find symbol '%s'\n", (name)); \
			goto lib_load_fail; \
		} \
	}

	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.event_queue_destroy, "wl_event_queue_destroy");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.event_queue_get_name, "wl_event_queue_get_name");

	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_flags, "wl_proxy_marshal_flags");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_array_flags, "wl_proxy_marshal_array_flags");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_array, "wl_proxy_marshal_array");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal, "wl_proxy_marshal");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_create, "wl_proxy_create");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_create_wrapper, "wl_proxy_create_wrapper");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_wrapper_destroy, "wl_proxy_wrapper_destroy");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_constructor, "wl_proxy_marshal_constructor");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_constructor_versioned, "wl_proxy_marshal_constructor_versioned");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_array_constructor, "wl_proxy_marshal_array_constructor");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_marshal_array_constructor_versioned, "wl_proxy_marshal_array_constructor_versioned");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_destroy, "wl_proxy_destroy");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_add_listener, "wl_proxy_add_listener");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_listener, "wl_proxy_get_listener");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_add_dispatcher, "wl_proxy_add_dispatcher");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_set_user_data, "wl_proxy_set_user_data");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_user_data, "wl_proxy_get_user_data");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_version, "wl_proxy_get_version");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_id, "wl_proxy_get_id");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_set_tag, "wl_proxy_set_tag");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_tag, "wl_proxy_get_tag");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_class, "wl_proxy_get_class");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_interface, "wl_proxy_get_interface");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_display, "wl_proxy_get_display");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_set_queue, "wl_proxy_set_queue");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.proxy_get_queue, "wl_proxy_get_queue");

	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_connect, "wl_display_connect");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_connect_to_fd, "wl_display_connect_to_fd");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_disconnect, "wl_display_disconnect");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_get_fd, "wl_display_get_fd");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch, "wl_display_dispatch");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_queue, "wl_display_dispatch_queue");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_timeout, "wl_display_dispatch_timeout");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_queue_timeout, "wl_display_dispatch_queue_timeout");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_queue_pending, "wl_display_dispatch_queue_pending");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_queue_pending_single, "wl_display_dispatch_queue_pending_single");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_pending, "wl_display_dispatch_pending");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_dispatch_pending_single, "wl_display_dispatch_pending_single");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_get_error, "wl_display_get_error");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_get_protocol_error, "wl_display_get_protocol_error");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_flush, "wl_display_flush");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_roundtrip_queue, "wl_display_roundtrip_queue");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_roundtrip, "wl_display_roundtrip");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_create_queue, "wl_display_create_queue");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_create_queue_with_name, "wl_display_create_queue_with_name");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_prepare_read_queue, "wl_display_prepare_read_queue");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_prepare_read, "wl_display_prepare_read");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_cancel_read, "wl_display_cancel_read");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_read_events, "wl_display_read_events");
	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.display_set_max_buffer_size, "wl_display_set_max_buffer_size");

	WL_LOAD_SYMBOL(g_wl_lib.lib, g_wl_lib.log_set_handler_client, "wl_log_set_handler_client");

	WL_LOAD_DATA_SYMBOL(g_wl_lib.lib, g_wl_lib.registry_interface, "wl_registry_interface");
	WL_LOAD_DATA_SYMBOL(g_wl_lib.lib, g_wl_lib.compositor_interface, "wl_compositor_interface");
	WL_LOAD_DATA_SYMBOL(g_wl_lib.lib, g_wl_lib.shm_interface, "wl_shm_interface");
	WL_LOAD_DATA_SYMBOL(g_wl_lib.lib, g_wl_lib.shm_pool_interface, "wl_shm_pool_interface");
	WL_LOAD_DATA_SYMBOL(g_wl_lib.lib, g_wl_lib.output_interface, "wl_output_interface");

	#undef WL_LOAD_SYMBOL
	#undef WL_LOAD_DATA_SYMBOL

	// Wayland init
    struct wl_display* display = g_wl_lib.display_connect(NULL);
	if (!display) goto init_failed;
	struct wl_registry* reg = (struct wl_registry*)g_wl_lib.proxy_marshal_flags((struct wl_proxy*)display, WL_DISPLAY_GET_REGISTRY,  g_wl_lib.registry_interface, g_wl_lib.proxy_get_version((struct wl_proxy*)display), 0, NULL);
	if (!reg) {
		g_wl_lib.display_disconnect(display);
		goto init_failed;
	}
	
	if (g_wl_lib.proxy_add_listener((struct wl_proxy*)reg, (void (**)(void))&g_registry_listener, NULL) < 0) goto cleanup;
	if (g_wl_lib.display_roundtrip(display) < 0) goto cleanup;
    if (g_wl_lib.display_roundtrip(display) < 0) goto cleanup;
	if (g_display_scale.w < 1 || g_display_scale.h < 1 || !g_shm || !g_compositor || !g_output) goto cleanup;

	uint32_t size = g_display_scale.w * g_display_scale.h * sizeof(uint32_t);
	int64_t shm_fd = wl_allocate_shm_file(size);
	if (shm_fd < 0) goto cleanup;

	uint8_t* pool_data = (uint8_t*)mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, (int)shm_fd, 0);
	if (!pool_data) {
		close(shm_fd);
		goto cleanup;
	}

	struct wl_shm_pool* pool = (struct wl_shm_pool*)((void*)g_wl_lib.proxy_marshal_flags((struct wl_proxy*)g_shm, WL_SHM_CREATE_POOL, g_wl_lib.shm_pool_interface, g_wl_lib.proxy_get_version((struct wl_proxy*)g_shm), 0, NULL, shm_fd, size));
	if (pool == MAP_FAILED) {
		munmap(pool_data, size);
		close(shm_fd);
		goto cleanup;
	}

	g_display = display;
	g_registry = reg;
	g_shm_fd = shm_fd;
	g_shm_pool = pool;
	g_shm_pool_size = size;
	g_shm_pool_data = pool_data;
	return ERR_SUCCESS;

	cleanup: {
		g_wl_lib.proxy_destroy((struct wl_proxy*)reg);
		g_wl_lib.display_disconnect(display);
	}
	init_failed:
	lib_load_fail: {
		if (g_wl_lib.lib) dlclose(g_wl_lib.lib);
		memset(&g_wl_lib, 0, sizeof(struct wayland_lib));
		return ERR_WS_INIT_FAILED;
	}
}

static void wayland_shutdown(void) {
    if (g_windows) destroy_all_windows();

	if (!g_wl_lib.lib) return;

	if (g_shm_pool) {
		g_wl_lib.proxy_marshal_flags((struct wl_proxy*)g_shm_pool, WL_SHM_POOL_DESTROY, NULL, g_wl_lib.proxy_get_version((struct wl_proxy*)g_shm_pool), WL_MARSHAL_FLAG_DESTROY);
        if (g_shm_pool_data) munmap(g_shm_pool_data, g_shm_pool_size);
		g_shm_pool_data = NULL;
        g_shm_pool = NULL;
    }

	if (g_registry) {
		g_wl_lib.proxy_destroy((struct wl_proxy*)g_registry);
		g_registry = NULL;
	}
    if (g_display) {
        g_wl_lib.display_disconnect(g_display);
        g_display = NULL;
    }

	if (g_shm_fd) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
	
	dlclose(g_wl_lib.lib);
	memset(&g_wl_lib, 0, sizeof(struct wayland_lib));
}

static t_err_codes wayland_create(PX_Window* win, PX_GPU_Backend gpu_backend_api) {
	if (!win) return ERR_INVALID_ARGUMENTS;
    win->handle = -1;
	win->gpu_backend_api = gpu_backend_api;

    struct window* iwin = (struct window*)malloc(sizeof(struct window));
    if (!iwin) return ERR_ALLOC_FAILED;

	iwin->display = g_display;
}
