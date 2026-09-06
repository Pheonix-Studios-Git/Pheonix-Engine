#include <stdbool.h>

#include <pheonix-engine.h>
#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/loader.h>
#include <editor.h>
#include <event-sys/menu-events.h>

#include <editor.h>

static char* save_path = NULL;
static PX_Dropdown* menu_dropdown = NULL;

static size_t loads[PX_RS_MAX_OBJECTS_PER_SCENE] = {0};
static size_t load_ptr = 0;

static bool fullscreened = false;

// FILE Options
static void handle_file_new_project(void) {
	PX_EditorState* state = editor_get_state();
	PX_EditorMode bmode = PX_EDITOR_MODE_3D;
	if (state) bmode = state->current_mode;

	for (size_t i = 0; i < load_ptr; i++) {
        PX_3D_Object* obj = &engine_3drenderer_main_scene.objects[loads[i]];
        if (obj->type == PX_RS_OBJECT_3D_TYPE_MESH) {
            px_rs_loader_destroy_load(&engine_3drenderer_main_scene, obj);
        }
    }
	load_ptr = 0;
    editor_new_project(&engine_3drenderer_main_scene, &engine_2drenderer_main_scene, "Untitled", bmode);
	enginef_init_3drenderer_main_scene();
	enginef_init_2drenderer_main_scene();
}
static void handle_file_open(void) {
    (void)px_ws_open_file_selector_dialog();
}
static void handle_file_save(void) {

}
static void handle_file_save_as(void) {

}
static void handle_file_import(void) {
    char* file = px_ws_open_file_selector_dialog();
    if (!file) return;
    size_t id = px_rs_loader_load_file(&engine_3drenderer_main_scene, file);
    if (id != 0) {
        loads[load_ptr++] = id-1;
    }
}
static void handle_file_quit(void) {
    PX_Event_GSignal signal = {0};
    signal.type = EVENT_GSIGNAL_CORE_QUIT;
    signal.core_quit = true;

    event_send_gsignal(&signal);

    // Cleanup
    for (size_t i = 0; i < load_ptr; i++) {
        PX_3D_Object* obj = &engine_3drenderer_main_scene.objects[loads[i]];
        if (obj->type == PX_RS_OBJECT_3D_TYPE_MESH) {
            px_rs_loader_destroy_load(&engine_3drenderer_main_scene, obj);
        }
    }
	load_ptr = 0;
}

// VIEW Options
static void handle_view_fullscreen(void) {
	t_err_codes out = px_ws_set_fullscreen(&engine_window_main, !fullscreened);
	if (out == ERR_SUCCESS)
		fullscreened = !fullscreened;
}
static void handle_view_toggle_3d_2d(void) {
	PX_EditorState* state = editor_get_state();
	if (!state) return;

	switch (state->current_mode) {
		case PX_EDITOR_MODE_3D: state->current_mode = PX_EDITOR_MODE_2D; break;
		case PX_EDITOR_MODE_2D: state->current_mode = PX_EDITOR_MODE_3D; break;
		default: break;
	}
}

void menu_evs_init(PX_Dropdown* menu_dd, char* path_to_save) {
    menu_dropdown = menu_dd;
    save_path = path_to_save;
}

void menu_evs_handle_events(PX_Event_GSignal* signal) {
    switch (signal->type) {
        case EVENT_GSIGNAL_UI_DROPDOWN_CLICK:
			if (!signal->ui_dropdown_click.clicked_node || !signal->ui_dropdown_click.dropdown) return;
            if (signal->ui_dropdown_click.dropdown != menu_dropdown) return;
            switch (signal->ui_dropdown_click.clicked_node->identifier) {
				// File (Identifier 1, Options 1000-1005)
				// File -> New (Identifier 1000, Options 10001-10003)
				case 10001: handle_file_new_project(); break;

				case 1001: handle_file_open(); break;
				case 1002: handle_file_save(); break;
				case 1003: handle_file_save_as(); break;
				case 1004: handle_file_import(); break;
				case 1005: handle_file_quit(); break;

				// Edit (Identifier 2, Options 2000-2001)

				// View (Identifier 3, Options 3000-3001)
				case 3000: handle_view_fullscreen(); break;
				case 3001: handle_view_toggle_3d_2d(); break;

				// Help (Identifier 4, Options 4000-4000)
				default: return;
            }
            break;
        default: return;
    }
}
