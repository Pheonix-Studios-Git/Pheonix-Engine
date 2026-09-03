#pragma once

#include <stdbool.h>

#include <rendering-sys.h>
#include <font.h>
#include <err-codes.h>

#define PX_EDITOR_CUR_VERSION 1.0f

typedef enum {
	PX_EDITOR_MODE_3D,
    PX_EDITOR_MODE_2D
} PX_EditorMode;

typedef struct {
    bool initialized;
    float renderer_version;
    float editor_version;

    bool saved;
    char* project_dir;
    char* project_name;

	PX_EditorMode current_mode;

    PX_Scene_3D* scene_3d;
	PX_Scene_2D* scene_2d;
} PX_EditorState;

t_err_codes editor_new_project(PX_Scene_3D* cScene3D, PX_Scene_2D* cScene2D, char* proj_name, PX_EditorMode base_mode);
PX_EditorState* editor_get_state(void);
void editor_draw_scene_panel(PX_Vector2 mpos, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 text_color, PX_Color4 color, PX_Color4 Hcolor, float noise, float cradius, PX_Font* font, float font_size, int xspacing, int yspacing);
void editor_click_scene_panel(PX_Vector2 mpos, PX_Transform2 transform, PX_Font* font, float font_size, int xspacing, int yspacing);
