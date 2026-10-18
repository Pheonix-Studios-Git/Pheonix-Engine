#pragma once

#define __PHEONIX_ENGINE__APPLICATION_NAME__ "Pheonix Engine"
#define __PHEONIX_ENGINE__VERSION__ 0x0100

#ifdef _WIN32
	#define PX_OS_PATH_SLASH "\\"
#else
	#define PX_OS_PATH_SLASH "/"
#endif

#define PX_OS_BASE_DIR ".." PX_OS_PATH_SLASH ".." PX_OS_PATH_SLASH ".."
#define PX_OS_BASE_DIR_LEN 8

#include <utils.h>
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

extern char* engine_exec_path;

void enginef_deinit_3drenderer_main_scene(void);
void enginef_deinit_2drenderer_main_scene(void);
void enginef_init_3drenderer_main_scene(void);
void enginef_init_2drenderer_main_scene(void);

#ifdef _WIN32
	#include <windows.h>

	extern HINSTANCE g_main_hinstance;
#endif