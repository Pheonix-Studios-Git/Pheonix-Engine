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
	PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
	PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
	PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;

	bool loaded;
};

struct base_renderer {
    VkInstance instance;

	VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceFeatures physical_device_features;
	VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
};

static struct vulkan_loader gr_vk_loader = {0};

static struct base_renderer gr_vk_base_raw = {0};
static struct base_renderer* gr_vk_base = &gr_vk_base_raw;

static t_err_codes pxvk_load_extensions_and_layers(const char** needed_layers, size_t needed_layer_count, const char** needed_extensions, size_t needed_ext_count) {
	if ((needed_layer_count > 0 && !needed_layers) || (needed_ext_count > 0 && !needed_extensions)) return ERR_INVALID_ARGUMENTS;

    uint32_t extension_count = 0;
    VkResult result;

    result = gr_vk_loader.EnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extension_count, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) return ERR_VK_EXTENSION_ENUMERATION_FAILED;

    VkExtensionProperties* extensions = NULL;
    if (extension_count > 0) {
        extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
        if (!extensions) return ERR_ALLOC_FAILED;

        result = gr_vk_loader.EnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extension_count, extensions);
        if (result != VK_SUCCESS) {
            free(extensions);
            return ERR_VK_EXTENSION_ENUMERATION_FAILED;
        }
    }

	size_t loaded_ext = 0;
    for (uint32_t i = 0; i < extension_count; i++) {
        printf("[Vulkan] Found Vulkan Extension: %s\n", extensions[i].extensionName);
		for (size_t j = 0; j < needed_ext_count; j++) {
			if (strcmp(extensions[i].extensionName, needed_extensions[j]) == 0) {
				loaded_ext++;
				printf("\tLoaded Vulkan Extension\n");
			}
		}
    }
    free(extensions);

	if (loaded_ext != needed_ext_count) return ERR_VK_LAYER_ENUMERATION_FAILED;

    uint32_t layer_count = 0;
    result = gr_vk_loader.EnumerateInstanceLayerProperties(&layer_count, VK_NULL_HANDLE);
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

	size_t loaded_layers = 0;
    for (uint32_t i = 0; i < layer_count; i++) {
        printf("[Vulkan] Vulkan layer: %s\n", layers[i].layerName);
		for (size_t j = 0; j < needed_layer_count; j++) {
			if (strcmp(layers[i].layerName, needed_layers[j]) == 0) {
				loaded_layers++;
				printf("\tLoaded Vulkan Layer\n");
			}
		}
    }
    free(layers);

	if (loaded_layers != needed_layer_count) return ERR_VK_LAYER_ENUMERATION_FAILED;
    return ERR_SUCCESS;
}

static size_t pxvk_score_gpu(VkPhysicalDeviceMemoryProperties memory_properties, VkPhysicalDeviceProperties properties, VkPhysicalDeviceFeatures features) {
    size_t score = 0;
    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 500;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 250;
            break;

        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 100;
            break;

        default:
            break;
    }

	VkDeviceSize device_local_memory = 0;
    for (uint32_t i = 0; i < memory_properties.memoryHeapCount; ++i) {
        if (memory_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            device_local_memory += memory_properties.memoryHeaps[i].size;
        }
    }
    size_t vram_gib = device_local_memory / (1024ULL * 1024ULL * 1024ULL);
    score += vram_gib * 100;

	if (features.samplerAnisotropy) score += 100;
    if (features.textureCompressionBC) score += 75;
    if (features.geometryShader) score += 50;
    if (features.tessellationShader) score += 25;
    if (features.wideLines) score += 10;

    return score;
}

static t_err_codes pxvk_load_best_gpu(VkPhysicalDevice* outDev, VkPhysicalDeviceProperties* outProp, VkPhysicalDeviceFeatures* outFeatures, VkPhysicalDeviceMemoryProperties* outMemProp) {
	if (!outDev || !outProp || !outFeatures || !outMemProp) return ERR_INVALID_ARGUMENTS;
	uint32_t device_count = 0;

	VkResult result = gr_vk_loader.EnumeratePhysicalDevices(gr_vk_base->instance, &device_count, VK_NULL_HANDLE);
	if (result != VK_SUCCESS || device_count == 0) {
		fprintf(stderr, "[Vulkan] No Physical Devices found!\n");
		return ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILED;
	}

	VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
	if (!devices) return ERR_ALLOC_FAILED;

	result = gr_vk_loader.EnumeratePhysicalDevices(gr_vk_base->instance, &device_count, devices);
	if (result != VK_SUCCESS) {
		free(devices);
		fprintf(stderr, "[Vulkan] Failed to enumerate Physical Devices!\n");

		return ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILED;
	}

	size_t best_score = 0;
	VkPhysicalDevice best_device;
	VkPhysicalDeviceProperties best_properties;
	VkPhysicalDeviceFeatures best_features;
	VkPhysicalDeviceMemoryProperties best_mem_properties;

	for (uint32_t i = 0; i < device_count; ++i) {
		VkPhysicalDeviceProperties properties;
		gr_vk_loader.GetPhysicalDeviceProperties(devices[i], &properties);

		VkPhysicalDeviceFeatures features;
		gr_vk_loader.GetPhysicalDeviceFeatures(devices[i], &features);

		VkPhysicalDeviceMemoryProperties mem_properties;
		gr_vk_loader.GetPhysicalDeviceMemoryProperties(devices[i], &mem_properties);

		size_t score = pxvk_score_gpu(mem_properties, properties, features);
		if (score > best_score) {
			best_score = score;
			best_device = devices[i];
			best_properties = properties;
			best_features = features;
			best_mem_properties = mem_properties;
		}

		printf("[Vulkan] Found GPU %u: %s\n", i, properties.deviceName);

		printf("\tAPI: %u.%u.%u\n", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
		printf("\tDriver: %u\n", properties.driverVersion);
		printf("\tVendor: 0x%04x\n", properties.vendorID);
		printf("\tDevice: 0x%04x\n", properties.deviceID);
		printf("\tScore: %lu\n", score);
	}
	free(devices);

	if (best_score == 0) {
		fprintf(stderr, "[Vulkan] No Applicable GPU Found!\n");
		return ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILED;
	}
	
	*outDev = best_device;
    *outProp = best_properties;
    *outFeatures = best_features;
	*outMemProp = best_mem_properties;
    printf("[Vulkan] Selected GPU: %s (Score: %d)\n", best_properties.deviceName, best_score);
	
	return ERR_SUCCESS;
}

t_err_codes px_rs_vk_init(void) {
	if (gr_vk_loader.loaded && gr_vk_loader.lib) return ERR_SUCCESS;

	struct vulkan_loader vkli = {0};
	vkli.lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!vkli.lib) {
		const char* err = dlerror();
		fprintf(stderr, "[Vulkan] Failed to load libvulkan.so.1\n");
		fprintf(stderr, "[Vulkan] dlerror: %s\n", err ? err : "unknown error");

		return ERR_VK_LIB_LOAD_FAILED;
	}

	vkli.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vkli.lib, "vkGetInstanceProcAddr");
	if (!vkli.GetInstanceProcAddr) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetInstanceProcAddr'\n");
		goto fail;
	}

	vkli.CreateInstance = (PFN_vkCreateInstance)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
	if (!vkli.CreateInstance) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkCreateInstance'\n");
		goto fail;
	}

    vkli.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
	if (!vkli.EnumerateInstanceExtensionProperties) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkEnumerateInstanceExtensionProperties'\n");
		goto fail;
	}

    vkli.EnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkli.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
	if (!vkli.EnumerateInstanceLayerProperties) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkEnumerateInstanceLayerProperties'\n");
		goto fail;
	}

	gr_vk_loader = vkli;
	gr_vk_loader.loaded = true;

	const char* layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	t_err_codes late_err_code = ERR_SUCCESS;

	late_err_code = pxvk_load_extensions_and_layers((const char**)layers, 1, NULL, 0);
	if (late_err_code != ERR_SUCCESS) goto late_fail;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = VK_NULL_HANDLE,
		.pApplicationName = __PHEONIX_ENGINE__APPLICATION_NAME__,
		.applicationVersion = __PHEONIX_ENGINE__VERSION__,
		.pEngineName = __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_NAME__,
		.engineVersion = __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_VERSION__,
		.apiVersion = VK_API_VERSION_1_3
	};
	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = VK_NULL_HANDLE,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = VK_NULL_HANDLE
	};

	VkResult result = vkli.CreateInstance(&instance_info, VK_NULL_HANDLE, &gr_vk_base->instance);
	if (result != VK_SUCCESS) {
		late_err_code = ERR_VK_INSTANCE_CREATION_FAILED;
		goto late_fail;
	}

	gr_vk_loader.DestroyInstance = (PFN_vkDestroyInstance)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkDestroyInstance");
	if (!gr_vk_loader.DestroyInstance) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkDestroyInstance'\n");

		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkEnumeratePhysicalDevices");
	if (!gr_vk_loader.EnumeratePhysicalDevices) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkEnumeratePhysicalDevices'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceFeatures");
	if (!gr_vk_loader.GetPhysicalDeviceFeatures) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceFeatures'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceProperties");
	if (!gr_vk_loader.GetPhysicalDeviceProperties) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceProperties'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceMemoryProperties");
	if (!gr_vk_loader.GetPhysicalDeviceMemoryProperties) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceMemoryProperties'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	late_err_code = pxvk_load_best_gpu(&gr_vk_base->physical_device, &gr_vk_base->physical_device_properties, &gr_vk_base->physical_device_features, &gr_vk_base->physical_device_memory_properties);
	if (late_err_code != ERR_SUCCESS) {
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		goto late_fail;
	}

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

	if (gr_vk_base->instance != VK_NULL_HANDLE && gr_vk_loader.DestroyInstance) gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);

	dlclose(gr_vk_loader.lib);
	gr_vk_loader = (struct vulkan_loader){0};
}

t_err_codes px_rs_vk_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_init_2d(PX_Scale2 screen_scale) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_shutdown_2d(void) {}
void px_rs_vk_shutdown_3d(void) {}
void px_rs_vk_frame_start(void) {}
void px_rs_vk_frame_end(void) {}
void px_rs_vk_2d_frame_update(void) {}
void px_rs_vk_3d_frame_update(void) {}
void px_rs_vk_frame_update(void) {}
void px_rs_vk_2d_resize(PX_Scale2 screen_scale) {}
void px_rs_vk_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {}
t_err_codes px_rs_vk_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_dropdown(PX_Dropdown* dd) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_editor_objects_3d(PX_Scene_3D* scene) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_scene_3d(PX_Scene_3D* scene) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_editor_objects_2d(PX_Scene_2D* scene) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_draw_scene_2d(PX_Scene_2D* scene) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {}
t_err_codes px_rs_vk_create_texture(PX_Texture* texture) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data) { return ERR_UNIMPLEMENTED; }
t_err_codes px_rs_vk_set_sampler(PX_Texture* texture, PX_Sampler* sampler) { return ERR_UNIMPLEMENTED; }
void px_rs_vk_destroy_texture(PX_Texture* texture) {}
void px_rs_vk_destroy_sampler(PX_Sampler* sampler) {}