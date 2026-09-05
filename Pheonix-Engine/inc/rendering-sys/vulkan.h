#pragma once

#ifndef __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_NO_INC__

// Includes Vulkan in order
#include <vulkan/vulkan.h>

#endif

#define __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_NAME__ "Pheonix Vulkan Rendering Engine"
#define __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_ENGINE_VERSION__ 0x0100
#define __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_IMP__

t_err_codes px_rs_vk_init(PX_WContext* ctx);
t_err_codes px_rs_vk_init_3d(PX_AnchorRect viewport);
t_err_codes px_rs_vk_init_2d(PX_AnchorRect viewport);
void px_rs_vk_shutdown_2d(void);
void px_rs_vk_shutdown_3d(void);
void px_rs_vk_shutdown(void);
void px_rs_vk_frame_start(void);
void px_rs_vk_frame_end(void);
void px_rs_vk_2d_frame_update(void);
void px_rs_vk_3d_frame_update(void);
void px_rs_vk_frame_update(void);
void px_rs_vk_2d_resize(PX_AnchorRect viewport);
void px_rs_vk_3d_resize(PX_AnchorRect viewport);
PX_GPU_Handle px_rs_vk_get_flat_fbo(void);
PX_GPU_Handle px_rs_vk_get_blank_tex(void);
PX_Scale2 px_rs_vk_get_flat_fbo_scale(void);
void px_rs_vk_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale);
t_err_codes px_rs_vk_create_texture(PX_Texture* texture);
t_err_codes px_rs_vk_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data);
t_err_codes px_rs_vk_set_sampler(PX_Texture* texture, PX_Sampler* sampler);
void px_rs_vk_destroy_texture(PX_Texture* texture);
void px_rs_vk_destroy_sampler(PX_Sampler* sampler);