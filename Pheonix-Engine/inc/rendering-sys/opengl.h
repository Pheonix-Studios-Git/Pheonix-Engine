#pragma once

#include <rendering-sys.h>

#ifndef __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_NO_INC__

// Includes OpenGL in order
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#endif

#define __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_IMP__

t_err_codes px_rs_gl_init(void);
t_err_codes px_rs_gl_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos);
t_err_codes px_rs_gl_init_ui(PX_Scale2 screen_scale);
void px_rs_gl_shutdown_ui(void);
void px_rs_gl_shutdown_3d(void);
void px_rs_gl_shutdown(void);
void px_rs_gl_frame_start(void);
void px_rs_gl_frame_end(void);
void px_rs_gl_ui_frame_update(void);
void px_rs_gl_3d_frame_update(void);
void px_rs_gl_frame_update(void);
void px_rs_gl_ui_resize(PX_Scale2 screen_scale);
void px_rs_gl_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos);
t_err_codes px_rs_gl_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius);
int px_rs_gl_text_width(PX_Font* font, const char* text, float pixel_height);
t_err_codes px_rs_gl_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font);
t_err_codes px_rs_gl_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color);
t_err_codes px_rs_gl_draw_dropdown(PX_Dropdown* dd);
t_err_codes px_rs_gl_draw_editor_objects(PX_Scene* scene);
t_err_codes px_rs_gl_draw_scene(PX_Scene* scene);
void px_rs_gl_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale);