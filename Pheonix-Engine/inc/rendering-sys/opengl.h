#pragma once

#include <rendering-sys.h>
#include <font.h>

#ifndef __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_NO_INC__

// Includes OpenGL in order
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#endif

#define __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_IMP__

t_err_codes px_rs_gl_init(PX_WContext* ctx);
t_err_codes px_rs_gl_init_3d(PX_AnchorRect viewport);
t_err_codes px_rs_gl_init_2d(PX_AnchorRect viewport);
void px_rs_gl_shutdown_2d(void);
void px_rs_gl_shutdown_3d(void);
void px_rs_gl_shutdown(void);
void px_rs_gl_frame_start(void);
void px_rs_gl_frame_end(void);
void px_rs_gl_2d_frame_update(void);
void px_rs_gl_3d_frame_update(void);
void px_rs_gl_frame_update(void);
void px_rs_gl_2d_resize(PX_AnchorRect viewport);
void px_rs_gl_3d_resize(PX_AnchorRect viewport);
PX_GPU_Handle px_rs_gl_get_flat_fbo(void);
PX_GPU_Handle px_rs_gl_get_blank_tex(void);
PX_Scale2 px_rs_gl_get_flat_fbo_scale(void);
void px_rs_gl_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale);
t_err_codes px_rs_gl_create_texture(PX_Texture* texture);
t_err_codes px_rs_gl_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data);
t_err_codes px_rs_gl_set_sampler(PX_Texture* texture, PX_Sampler* sampler);
void px_rs_gl_destroy_texture(PX_Texture* texture);
void px_rs_gl_destroy_sampler(PX_Sampler* sampler);