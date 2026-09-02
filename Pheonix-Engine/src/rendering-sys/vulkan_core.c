#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dlfcn.h>

#include <cglm/cglm.h>

#include <pheonix-engine.h>
#include <rendering-sys.h>
#include <font.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>
#include <decoders/unicode.h>
#include <event-sys.h>

#include <rendering-sys/vulkan.h>
#include <rendering-sys/internal.h>

struct vulkan_loader {
	void* lib;
	PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
	PFN_vkCreateInstance CreateInstance;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;

	bool loaded;
};

struct base_renderer {
    VkInstance instance;
};

static struct vulkan_loader gr_vk_loader = {0};

static struct base_renderer gr_vk_base_raw = {0};
static struct base_renderer* gr_vk_base = &gr_vk_base_raw;

static t_err_codes pxvk_load_extensions_and_layers(void) {
    uint32_t extension_count = 0;
    VkResult result;

    result = gr_vk_loader.EnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    if (result != VK_SUCCESS) return ERR_VK_EXTENSION_ENUMERATION_FAILED;

    VkExtensionProperties* extensions = NULL;
    if (extension_count > 0) {
        extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
        if (!extensions) return ERR_ALLOC_FAILED;

        result = gr_vk_loader.EnumerateInstanceExtensionProperties(NULL, &extension_count, extensions);
        if (result != VK_SUCCESS) {
            free(extensions);
            return ERR_VK_EXTENSION_ENUMERATION_FAILED;
        }
    }

    for (uint32_t i = 0; i < extension_count; i++) {
        printf("Vulkan extension: %s\n", extensions[i].extensionName);
    }
    free(extensions);

    uint32_t layer_count = 0;
    result = gr_vk_loader.EnumerateInstanceLayerProperties(&layer_count, NULL);
    if (result != VK_SUCCESS) return ERR_VK_LAYER_ENUMERATION_FAILED;

    VkLayerProperties* layers = NULL;
    if (layer_count > 0) {
        layers = malloc(sizeof(VkLayerProperties) * layer_count);
        if (!layers) return ERR_ALLOC_FAILED;

        result = gr_vk_loader.EnumerateInstanceLayerProperties(&layer_count, layers);
        if (result != VK_SUCCESS) {
            free(layers);
            return ERR_VK_LAYER_ENUMERATION_FAILED;
        }
    }

    for (uint32_t i = 0; i < layer_count; i++) {
        printf("Vulkan layer: %s\n", layers[i].layerName);
    }
    free(layers);

    return ERR_SUCCESS;
}

t_err_codes px_rs_vk_init(void) {
	if (gr_vk_loader.loaded && gr_vk_loader.lib) return ERR_SUCCESS;

	struct vulkan_loader vkli = {0};
	vkli.lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!vkli.lib) return ERR_VK_LIB_LOAD_FAILED;

	vkli.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vkli.lib, "vkGetInstanceProcAddr");
	if (!vkli.GetInstanceProcAddr) goto fail;

	vkli.CreateInstance = (PFN_vkCreateInstance)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
	if (!vkli.CreateInstance) goto fail;

    vkli.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
	if (!vkli.EnumerateInstanceExtensionProperties) goto fail;

    vkli.EnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
	if (!vkli.EnumerateInstanceLayerProperties) goto fail;

	gr_vk_loader = vkli;
	gr_vk_loader.loaded = true;
	t_err_codes late_err_code = ERR_SUCCESS;

	late_err_code = pxvk_load_extensions_and_layers();
	if (late_err_code != ERR_SUCCESS) goto late_fail;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.pApplicationName = __PHEONIX_ENGINE__APPLICATION_NAME__,
		.applicationVersion = __PHEONIX_ENGINE__VERSION__,
		.pEngineName = __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_NAME__,
		.engineVersion = __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_VERSION__,
		.apiVersion = VK_API_VERSION_1_3
	};
	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = NULL
	};

	return ERR_SUCCESS;

	fail: {
		dlclose(vkli.lib);
		return ERR_VK_LIB_LOAD_FAILED;
	}

	late_fail: {
		gr_vk_loader = (struct vulkan_loader){0};

		dlclose(vkli.lib);
		return late_err_code;
	}
}

void px_rs_vk_shutdown(void) {
	if (!gr_vk_loader.loaded || !gr_vk_loader.lib) return;

	dlclose(gr_vk_loader.lib);
	gr_vk_loader = (struct vulkan_loader){0};
}

t_err_codes px_rs_vk_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_init_ui(PX_Scale2 screen_scale) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_shutdown_ui(void) {}
void px_rs_vk_shutdown_3d(void) {}
void px_rs_vk_frame_start(void) {}
void px_rs_vk_frame_end(void) {}
void px_rs_vk_ui_frame_update(void) {}
void px_rs_vk_3d_frame_update(void) {}
void px_rs_vk_frame_update(void) {}
void px_rs_vk_ui_resize(PX_Scale2 screen_scale) {}
void px_rs_vk_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {}
t_err_codes px_rs_vk_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_dropdown(PX_Dropdown* dd) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_editor_objects(PX_Scene* scene) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_scene(PX_Scene* scene) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {}
t_err_codes px_rs_vk_create_texture(PX_Texture* texture) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_set_sampler(PX_Texture* texture, PX_Sampler* sampler) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_destroy_texture(PX_Texture* texture) {}
void px_rs_vk_destroy_sampler(PX_Sampler* sampler) {}