#include <stdlib.h>
#include <cglm/cglm.h>

#include <decoders/unicode.h>
#include <event-sys/keycodes.h>
#include <loaders/sdf-loader.h>
#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/internal.h>

#define __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_NO_INC__
#include <rendering-sys/opengl.h>
#define __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_NO_INC__
#include <rendering-sys/vulkan.h>

struct scene_cam_3d gscene_cam_3d = {0};
struct scene_cam_2d gscene_cam_2d = {0};

static PX_GPU_Backend current_backend = __PHEONIX_ENGINE__RENDERING_SYS__DEFAULT_BACKEND__;
static bool backend_initialized = false;

t_err_codes px_rs_init(PX_WContext* ctx) {
	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init(ctx); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init(ctx); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	return code;
}

t_err_codes px_rs_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init_3d(screen_scale, screen_pos); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init_3d(screen_scale, screen_pos); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	return code;
}

t_err_codes px_rs_init_2d(PX_Scale2 screen_scale) {
	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init_2d(screen_scale); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init_2d(screen_scale); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	return code;
}

void px_rs_shutdown_2d(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_2d(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_2d(); break;
		default: return;
	}
}

void px_rs_shutdown_3d(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_3d(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_3d(); break;
		default: return;
	}
}

void px_rs_shutdown(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown(); break;
		default: return;
	}

	backend_initialized = false;
}

void px_rs_frame_start(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_start(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_start(); break;
		default: return;
	}
}

void px_rs_frame_end(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_end(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_end(); break;
		default: return;
	}
}

void px_rs_2d_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_2d_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_2d_frame_update(); break;
		default: return;
	}
}

void px_rs_3d_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_frame_update(); break;
		default: return;
	}
}

void px_rs_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_update(); break;
		default: return;
	}
}

void px_rs_2d_resize(PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_2d_resize(screen_scale); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_2d_resize(screen_scale); break;
		default: return;
	}
}

void px_rs_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_resize(screen_scale, screen_pos); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_resize(screen_scale, screen_pos); break;
		default: return;
	}
}

void px_rs_update_scene_cam_3d(PX_Vector2 mdelta, PX_EKeycodes key) {
    gscene_cam_3d.yaw += mdelta.x * gscene_cam_3d.mouse_sens;
    gscene_cam_3d.pitch -= mdelta.y * gscene_cam_3d.mouse_sens;

    if (gscene_cam_3d.pitch > 89.0f) gscene_cam_3d.pitch = 89.0f;
    if (gscene_cam_3d.pitch < -89.0f) gscene_cam_3d.pitch = -89.0f;

    vec3 forward;
    forward[0] = cos(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    forward[1] = sin(glm_rad(gscene_cam_3d.pitch));
    forward[2] = sin(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    glm_normalize(forward);

    vec3 right;
    glm_vec3_cross(forward, gscene_cam_3d.up, right);
    glm_normalize(right);

    float speed = gscene_cam_3d.move_speed;

    switch (key) {
        case EKeycode_W:
            glm_vec3_muladds(forward, speed, gscene_cam_3d.position);
            break;

        case EKeycode_S:
            glm_vec3_muladds(forward, -speed, gscene_cam_3d.position);
            break;

        case EKeycode_D:
            glm_vec3_muladds(right, speed, gscene_cam_3d.position);
            break;

        case EKeycode_A:
            glm_vec3_muladds(right, -speed, gscene_cam_3d.position);
            break;

        case EKeycode_Space:
            gscene_cam_3d.position[1] += speed;
            break;

        case EKeycode_LControl:
            gscene_cam_3d.position[1] -= speed;
            break;

        default:
            break;
    }
}

void px_rs_update_scene_cam_2d(PX_Vector2 mdelta, PX_EKeycodes key) {
    float speed = gscene_cam_2d.move_speed;

    switch (key) {
        case EKeycode_W:
            gscene_cam_2d.position[1] += speed;
            break;

        case EKeycode_S:
            gscene_cam_2d.position[1] -= speed;
            break;

        case EKeycode_D:
            gscene_cam_2d.position[0] += speed;
            break;

        case EKeycode_A:
            gscene_cam_2d.position[0] -= speed;
            break;

        default: break;
    }
}

void px_rs_config_scene_cam(float mouse_sensitivity, float speed) {
    if (mouse_sensitivity > 0.0f) gscene_cam_3d.mouse_sens = mouse_sensitivity;
    if (speed > 0.0f) gscene_cam_3d.move_speed = speed;
}

t_err_codes px_rs_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_panel(tran, color, noise, cradius);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_panel(tran, color, noise, cradius);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

int px_rs_text_width(PX_Font* font, const char* text, float pixel_height) {
    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));
    float pen_x = 0.0f;

    for (const char* p = text; *p; ) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        pen_x += g->advance * scale;
    }

    return (int)(pen_x + 0.5f);
}

t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_render_text(text, pixel_height, pos, color, font);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_render_text(text, pixel_height, pos, color, font);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_line(start, end, thickness, color);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_line(start, end, thickness, color);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_dropdown(PX_Dropdown* dd) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_dropdown(dd);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_dropdown(dd);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_editor_objects_3d(PX_Scene_3D* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_editor_objects_3d(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_editor_objects_3d(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_editor_objects_2d(PX_Scene_2D* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_editor_objects_2d(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_editor_objects_2d(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_scene_3d(PX_Scene_3D* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_scene_3d(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_scene_3d(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_scene_2d(PX_Scene_2D* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_scene_2d(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_scene_2d(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

void px_rs_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_handle_mouse_move(mpos, screen_scale); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_handle_mouse_move(mpos, screen_scale); break;
		default: return;
	}
}

t_err_codes px_rs_change_backend(PX_GPU_Backend new_backend) {
	switch (new_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: break;
		case PX_RS_GPU_BACKEND_VULKAN: break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (new_backend == current_backend) return ERR_SUCCESS;

	if (backend_initialized) {
		px_rs_shutdown();
	}
	
	current_backend = new_backend;
	return ERR_SUCCESS;
}

t_err_codes px_rs_create_texture(PX_Texture* texture) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_create_texture(texture);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_create_texture(texture);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_upload_texture(texture, source_format, mip_level, data);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_upload_texture(texture, source_format, mip_level, data);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_set_sampler(PX_Texture* texture, PX_Sampler* sampler) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_set_sampler(texture, sampler);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_set_sampler(texture, sampler);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

void px_rs_destroy_texture(PX_Texture* texture) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_texture(texture); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_texture(texture); break;
		default: return;
	}
}

void px_rs_destroy_sampler(PX_Sampler* sampler) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_sampler(sampler); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_sampler(sampler); break;
		default: return;
	}
}
