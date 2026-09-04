#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dlfcn.h>

#include <cglm/cglm.h>

#define __PHEONIX_ENGINE__WINDOW_SYS__VULKAN_SPECIFIC_INC__
#include <window-sys.h>

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
	PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;

	PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;

	PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
	PFN_vkCreateDevice CreateDevice;
	PFN_vkDestroyDevice DestroyDevice;
	PFN_vkGetDeviceQueue GetDeviceQueue;
	
	PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
	PFN_vkDeviceWaitIdle DeviceWaitIdle;

	bool loaded;
};

struct vulkan_physical_device {
	VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory_properties;

	VkQueueFamilyProperties* queue_families;
	uint32_t queue_family_count;

	uint32_t graphics_queue_family_idx;
	uint32_t present_queue_family_idx;

	VkDeviceSize video_memory_in_bytes;
};

struct vulkan_device {
    VkDevice device;

    VkQueue graphics_queue;
	VkQueue present_queue;
};

struct base_renderer {
    VkInstance instance;
	VkSurfaceKHR surface;

	struct vulkan_physical_device physical_device;
	struct vulkan_device device;
};

static struct vulkan_loader gr_vk_loader = {0};

static struct base_renderer gr_vk_base_raw = {0};
static struct base_renderer* gr_vk_base = &gr_vk_base_raw;

static const char* pxvk_beautify_bytes_unit(uint64_t bytes) {
    static const char* units[] = {
        "B",
        "KiB",
        "MiB",
        "GiB",
        "TiB",
        "PiB",
        "EiB"
    };

	double v = (double)bytes;

    uint32_t unit = 0;
    while (v >= 1024.0f && unit < 6) {
        v /= 1024.0f;
        unit++;
    }

    return units[unit];
}

static double pxvk_beautify_bytes_value(uint64_t bytes) {
    double v = (double)bytes;

    uint32_t unit = 0;
    while (v >= 1024.0f && unit < 6) {
        v /= 1024.0f;
        unit++;
    }

    return v;
}

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

static size_t pxvk_score_gpu(VkPhysicalDeviceMemoryProperties memory_properties, VkPhysicalDeviceProperties properties, VkPhysicalDeviceFeatures features, uint32_t queue_family_count, uint64_t* out_vram) {
    size_t score = 0;
	if (queue_family_count < 1) return 0; // Can't use it without queues!

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
    
	if (out_vram) *out_vram = (uint64_t)device_local_memory;
	size_t vram_gib = device_local_memory / (1024ULL * 1024ULL * 1024ULL);
    score += vram_gib * 100;

	if (features.samplerAnisotropy) score += 100;
    if (features.textureCompressionBC) score += 75;
    if (features.geometryShader) score += 50;
    if (features.tessellationShader) score += 25;
    if (features.wideLines) score += 10;

	score += queue_family_count * 10;

    return score;
}

static t_err_codes pxvk_load_best_gpu(struct vulkan_physical_device* out, VkSurfaceKHR surfaceKHR) {
	if (!out) return ERR_INVALID_ARGUMENTS;
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
	struct vulkan_physical_device best_device;

	for (uint32_t i = 0; i < device_count; ++i) {
		const char* cont_without_score_error = "Unknown";

		VkPhysicalDeviceProperties properties;
		gr_vk_loader.GetPhysicalDeviceProperties(devices[i], &properties);

		VkPhysicalDeviceFeatures features;
		gr_vk_loader.GetPhysicalDeviceFeatures(devices[i], &features);

		VkPhysicalDeviceMemoryProperties mem_properties;
		gr_vk_loader.GetPhysicalDeviceMemoryProperties(devices[i], &mem_properties);

		printf("[Vulkan] Found GPU %u: %s\n", i, properties.deviceName);

		printf("\tAPI: %u.%u.%u\n", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
		printf("\tDriver: %u\n", properties.driverVersion);
		printf("\tVendor: 0x%04x\n", properties.vendorID);
		printf("\tDevice: 0x%04x\n", properties.deviceID);

		VkSurfaceCapabilitiesKHR capabilities;
		result = gr_vk_loader.GetPhysicalDeviceSurfaceCapabilitiesKHR(devices[i], surfaceKHR, &capabilities);
		if (result != VK_SUCCESS) continue;

		uint32_t format_count = 0;
		result = gr_vk_loader.GetPhysicalDeviceSurfaceFormatsKHR(devices[i], surfaceKHR, &format_count, VK_NULL_HANDLE);
		if (result != VK_SUCCESS || format_count == 0) continue;

		uint32_t present_mode_count = 0;
		result = gr_vk_loader.GetPhysicalDeviceSurfacePresentModesKHR(devices[i], surfaceKHR, &present_mode_count, VK_NULL_HANDLE);
		if (result != VK_SUCCESS || present_mode_count == 0) continue;

		uint32_t queue_count;
		gr_vk_loader.GetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, VK_NULL_HANDLE);
		if (queue_count == 0) {
			cont_without_score_error = "No Queue Families";
			continue_without_score: {
				printf("[Vulkan] Found GPU %u: %s\n", i, properties.deviceName);

				printf("\tAPI: %u.%u.%u\n", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
				printf("\tDriver: %u\n", properties.driverVersion);
				printf("\tVendor: 0x%04x\n", properties.vendorID);
				printf("\tDevice: 0x%04x\n", properties.deviceID);
				printf("\tScore: 0 (%s)\n", cont_without_score_error);
				continue;
			}
		}

		VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)*queue_count);
		if (!queue_families) {
			cont_without_score_error = "Memory Allocation Failed";
			goto continue_without_score;
		}

		gr_vk_loader.GetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queue_families);
		
		int64_t graphics_queue = -1; // To use -1 as a invalid signal
		int64_t present_queue = -1;

		printf("\tQueue Families:\n");
		for (uint32_t j = 0; j < queue_count; j++) {
			VkQueueFamilyProperties* qfp = &queue_families[j];

			printf("\tQueue Family %u:\n", j);
			printf("\t\tQueues: %u\n", qfp->queueCount);
			printf("\t\tFlags: 0x%04x\n", qfp->queueFlags);
			if (qfp->queueCount < 1) continue;

			if (qfp->queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_queue = (int64_t)j;
		
			VkBool32 present_supported = VK_FALSE;
			VkResult result = gr_vk_loader.GetPhysicalDeviceSurfaceSupportKHR(devices[i], j, surfaceKHR, &present_supported);
			if (result != VK_SUCCESS) continue;
			
			if (present_supported) present_queue = (int64_t)j;
			if (present_queue >= 0 && graphics_queue >= 0) break;
		}

		if (graphics_queue < 0 || present_queue < 0) {
			free(queue_families);
			continue;
		}

		uint64_t vram = 0;
		size_t score = pxvk_score_gpu(mem_properties, properties, features, queue_count, &vram);
		if (score > best_score) {
			best_score = score;
			best_device.device = devices[i];
			best_device.properties = properties;
			best_device.features = features;
			best_device.memory_properties = mem_properties;
			best_device.video_memory_in_bytes = (VkDeviceSize)vram;
			best_device.queue_family_count = queue_count;
			best_device.graphics_queue_family_idx = (uint32_t)graphics_queue;
			best_device.present_queue_family_idx = (uint32_t)present_queue;
			best_device.queue_families = queue_families;
		} else {
			free(queue_families);
		}

		printf("\tVideo RAM: %.2f %s\n", pxvk_beautify_bytes_value(vram), pxvk_beautify_bytes_unit(vram));
		printf("\tScore: %zu\n", score);
	}
	free(devices);

	if (best_score == 0) {
		fprintf(stderr, "[Vulkan] No Applicable GPU Found!\n");
		return ERR_VK_PHYSICAL_DEVICE_ENUMERATION_FAILED;
	}
	
	*out = best_device;
    printf("[Vulkan] Selected GPU: %s (Score: %zu)\n", best_device.properties.deviceName, best_score);
	
	return ERR_SUCCESS;
}

static t_err_codes pxvk_create_device(VkDevice* out, const char** layers, size_t layer_count, const char** extensions, size_t ext_count) {
	if (!out) return ERR_INVALID_ARGUMENTS;

    uint32_t graphics_idx = gr_vk_base->physical_device.graphics_queue_family_idx;
	uint32_t present_idx = gr_vk_base->physical_device.present_queue_family_idx;

    float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_infos[2] = {0};
	uint32_t queue_info_count = 0;

    queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .queueFamilyIndex = graphics_idx,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

	if (graphics_idx != present_idx) {
		queue_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.queueFamilyIndex = present_idx,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};
	}

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .queueCreateInfoCount = queue_info_count,
        .pQueueCreateInfos = (VkDeviceQueueCreateInfo*)queue_infos,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = VK_NULL_HANDLE
    };

    VkResult result = gr_vk_loader.CreateDevice(gr_vk_base->physical_device.device, &device_info, VK_NULL_HANDLE, out);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "[Vulkan] Failed to create logical device: %d\n", result);
        return ERR_VK_DEVICE_CREATION_FAILED;
    }

    return ERR_SUCCESS;
}

t_err_codes px_rs_vk_init(PX_WContext* ctx) {
	if (ctx->backend != PX_RS_GPU_BACKEND_VULKAN) return ERR_RS_INVALID_BACKEND;
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

	late_err_code = pxvk_load_extensions_and_layers((const char**)layers, 1, ctx->vulkan.required_extensions, ctx->vulkan.required_extension_count);
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
		.enabledExtensionCount = ctx->vulkan.required_extension_count,
		.ppEnabledExtensionNames = ctx->vulkan.required_extensions
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

	gr_vk_loader.GetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceQueueFamilyProperties");
	if (!gr_vk_loader.GetPhysicalDeviceQueueFamilyProperties) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceQueueFamilyProperties'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
	if (!gr_vk_loader.GetPhysicalDeviceSurfaceSupportKHR) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceSurfaceSupportKHR'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	if (!gr_vk_loader.GetPhysicalDeviceSurfaceCapabilitiesKHR) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceSurfaceCapabilitiesKHR'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	if (!gr_vk_loader.GetPhysicalDeviceSurfaceFormatsKHR) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceSurfaceFormatsKHR'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	if (!gr_vk_loader.GetPhysicalDeviceSurfacePresentModesKHR) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetPhysicalDeviceSurfacePresentModesKHR'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkGetDeviceProcAddr");
	if (!gr_vk_loader.GetDeviceProcAddr) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetDeviceProcAddr'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.CreateDevice = (PFN_vkCreateDevice)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkCreateDevice");
	if (!gr_vk_loader.CreateDevice) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkCreateDevice'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.DestroyDevice = (PFN_vkDestroyDevice)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkDestroyDevice");
	if (!gr_vk_loader.DestroyDevice) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkDestroyDevice'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkDestroySurfaceKHR");
	if (!gr_vk_loader.DestroySurfaceKHR) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkDestroySurfaceKHR'\n");
		
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	late_err_code = px_ws_vk_finish_ctx(ctx, gr_vk_base->instance, gr_vk_loader.GetInstanceProcAddr);
	if (late_err_code != ERR_SUCCESS) {
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		goto late_fail;
	}

	gr_vk_base->surface = (VkSurfaceKHR)ctx->vulkan.surfaceKHR;

	late_err_code = pxvk_load_best_gpu(&gr_vk_base->physical_device, gr_vk_base->surface);
	if (late_err_code != ERR_SUCCESS) {
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		goto late_fail;
	}

	const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	late_err_code = pxvk_create_device(&gr_vk_base->device.device, VK_NULL_HANDLE, 0, (const char**)device_extensions, 1);
	if (late_err_code != ERR_SUCCESS) {
		if (gr_vk_base->physical_device.queue_families) free(gr_vk_base->physical_device.queue_families);
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		goto late_fail;
	}

	gr_vk_loader.DeviceWaitIdle = (PFN_vkDeviceWaitIdle)gr_vk_loader.GetInstanceProcAddr(gr_vk_base->instance, "vkDeviceWaitIdle");
	if (!gr_vk_loader.DeviceWaitIdle) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkDeviceWaitIdle'\n");
		
		if (gr_vk_base->physical_device.queue_families) free(gr_vk_base->physical_device.queue_families);		
		gr_vk_loader.DestroyDevice(gr_vk_base->device.device, VK_NULL_HANDLE);
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetDeviceQueue = (PFN_vkGetDeviceQueue)gr_vk_loader.GetDeviceProcAddr(gr_vk_base->device.device, "vkGetDeviceQueue");
	if (!gr_vk_loader.GetDeviceQueue) {
		fprintf(stderr, "[Vulkan] Failed to find symbol 'vkGetDeviceQueue'\n");

		if (gr_vk_base->physical_device.queue_families) free(gr_vk_base->physical_device.queue_families);		
		gr_vk_loader.DestroyDevice(gr_vk_base->device.device, VK_NULL_HANDLE);
		gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);
		late_err_code = ERR_VK_LIB_LOAD_FAILED;
		goto late_fail;
	}

	gr_vk_loader.GetDeviceQueue(gr_vk_base->device.device, gr_vk_base->physical_device.graphics_queue_family_idx, 0, &gr_vk_base->device.graphics_queue);
	gr_vk_loader.GetDeviceQueue(gr_vk_base->device.device, gr_vk_base->physical_device.present_queue_family_idx, 0, &gr_vk_base->device.present_queue);

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

	if (gr_vk_base->device.device != VK_NULL_HANDLE && gr_vk_loader.DeviceWaitIdle) {
        gr_vk_loader.DeviceWaitIdle(gr_vk_base->device.device);
    }

	if (gr_vk_base->device.device != VK_NULL_HANDLE && gr_vk_loader.DestroyDevice) {
        gr_vk_loader.DestroyDevice(gr_vk_base->device.device, VK_NULL_HANDLE);
        gr_vk_base->device = (struct vulkan_device){0};
    }

	if (gr_vk_base->physical_device.queue_families) {
		free(gr_vk_base->physical_device.queue_families);
		gr_vk_base->physical_device.queue_families = VK_NULL_HANDLE;
		gr_vk_base->physical_device.queue_family_count = 0;
		gr_vk_base->physical_device.graphics_queue_family_idx = 0;
	}
	
	if ((gr_vk_base->instance != VK_NULL_HANDLE && gr_vk_base->physical_device.device != VK_NULL_HANDLE) && (gr_vk_loader.DestroySurfaceKHR && gr_vk_base->surface != VK_NULL_HANDLE)) {
        gr_vk_loader.DestroySurfaceKHR(gr_vk_base->instance, gr_vk_base->surface, VK_NULL_HANDLE);
        gr_vk_base->surface = VK_NULL_HANDLE;
    }

	if (gr_vk_base->instance != VK_NULL_HANDLE && gr_vk_loader.DestroyInstance) gr_vk_loader.DestroyInstance(gr_vk_base->instance, VK_NULL_HANDLE);

	dlclose(gr_vk_loader.lib);
	gr_vk_loader = (struct vulkan_loader){0};
}

t_err_codes px_rs_vk_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) { return ERR_SUCCESS; }
t_err_codes px_rs_vk_init_2d(PX_Scale2 screen_scale) { return ERR_SUCCESS; }
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