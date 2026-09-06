#pragma once

#define __PHEONIX_ENGINE__APPLICATION_NAME__ "Pheonix Engine"
#define __PHEONIX_ENGINE__VERSION__ 0x0100

#include <rendering-sys.h>
#include <font.h>
#include <window-sys.h>

extern const PX_Color4 engine_2d_theme_dark_text_color;
extern const PX_Color4 engine_2d_theme_dark_text_hover_color;
extern const PX_Color4 engine_2d_theme_dark_panel_color1;
extern const float engine_2d_theme_dark_cradius;
extern const float engine_2d_theme_dark_noise;

extern PX_Scene_3D engine_3drenderer_main_scene;
extern PX_Scene_2D engine_2drenderer_main_scene;
extern PX_Window engine_window_main;

void enginef_init_3drenderer_main_scene(void);
void enginef_init_2drenderer_main_scene(void);
PX_Transform2 enginef_convert_anchor_to_transform(PX_AnchorRect r);