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

struct scene_cam gscene_cam = {0};

static PX_GPU_Backend current_backend = __PHEONIX_ENGINE__RENDERING_SYS__DEFAULT_BACKEND__;

t_err_codes px_rs_init(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_init();
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_init();
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_init_3d(screen_scale, screen_pos);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_init_3d(screen_scale, screen_pos);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_init_ui(PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_init_ui(screen_scale);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_init_ui(screen_scale);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

void px_rs_shutdown_ui(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_ui();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_ui();
		default: return;
	}
}

void px_rs_shutdown_3d(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_3d();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_3d();
		default: return;
	}
}

void px_rs_shutdown(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown();
		default: return;
	}
}

void px_rs_frame_start(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_start();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_start();
		default: return;
	}
}

void px_rs_frame_end(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_end();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_end();
		default: return;
	}
}

void px_rs_ui_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_ui_frame_update();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_ui_frame_update();
		default: return;
	}
}

void px_rs_3d_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_frame_update();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_frame_update();
		default: return;
	}
}

void px_rs_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_update();
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_update();
		default: return;
	}
}

void px_rs_ui_resize(PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_ui_resize(screen_scale);
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_ui_resize(screen_scale);
		default: return;
	}
}

void px_rs_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_resize(screen_scale, screen_pos);
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_resize(screen_scale, screen_pos);
		default: return;
	}
}

void px_rs_update_scene_cam(PX_Vector2 mdelta, PX_EKeycodes key) {
    gscene_cam.yaw += mdelta.x * gscene_cam.mouse_sens;
    gscene_cam.pitch -= mdelta.y * gscene_cam.mouse_sens;

    if (gscene_cam.pitch > 89.0f) gscene_cam.pitch = 89.0f;
    if (gscene_cam.pitch < -89.0f) gscene_cam.pitch = -89.0f;

    vec3 forward;
    forward[0] = cos(glm_rad(gscene_cam.yaw)) * cos(glm_rad(gscene_cam.pitch));
    forward[1] = sin(glm_rad(gscene_cam.pitch));
    forward[2] = sin(glm_rad(gscene_cam.yaw)) * cos(glm_rad(gscene_cam.pitch));
    glm_normalize(forward);

    vec3 right;
    glm_vec3_cross(forward, gscene_cam.up, right);
    glm_normalize(right);

    float speed = gscene_cam.move_speed;

    switch (key) {
        case EKeycode_W:
            glm_vec3_muladds(forward, speed, gscene_cam.position);
            break;

        case EKeycode_S:
            glm_vec3_muladds(forward, -speed, gscene_cam.position);
            break;

        case EKeycode_D:
            glm_vec3_muladds(right, speed, gscene_cam.position);
            break;

        case EKeycode_A:
            glm_vec3_muladds(right, -speed, gscene_cam.position);
            break;

        case EKeycode_Space:
            gscene_cam.position[1] += speed;
            break;

        case EKeycode_LControl:
            gscene_cam.position[1] -= speed;
            break;

        default:
            break;
    }
}

void px_rs_config_scene_cam(float mouse_sensitivity, float speed) {
    if (mouse_sensitivity > 0.0f) gscene_cam.mouse_sens = mouse_sensitivity;
    if (speed > 0.0f) gscene_cam.move_speed = speed;
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

t_err_codes px_rs_draw_editor_objects(PX_Scene* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_editor_objects(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_editor_objects(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_draw_scene(PX_Scene* scene) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_draw_scene(scene);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_draw_scene(scene);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

void px_rs_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_handle_mouse_move(mpos, screen_scale);
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_handle_mouse_move(mpos, screen_scale);
		default: return;
	}
}

t_err_codes px_rs_change_backend(PX_GPU_Backend new_backend, PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
	switch (new_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: break;
		case PX_RS_GPU_BACKEND_VULKAN: break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (new_backend == current_backend) return ERR_SUCCESS;
	px_rs_shutdown();
	
	current_backend = new_backend;
	
	t_err_codes err = px_rs_init();
	if (err != ERR_SUCCESS) return err;

	err = px_rs_init_3d(screen_scale, screen_pos);
	if (err != ERR_SUCCESS) return err;

	err = px_rs_init_ui(screen_scale);
	if (err != ERR_SUCCESS) return err;

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
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_texture(texture);
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_texture(texture);
		default: return;
	}
}

void px_rs_destroy_sampler(PX_Sampler* sampler) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_sampler(sampler);
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_sampler(sampler);
		default: return;
	}
}

