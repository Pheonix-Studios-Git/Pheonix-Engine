#include <stdbool.h>

#include <pheonix-engine.h>
#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>
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
static void handle_file_new(void) {
	for (size_t i = 0; i < load_ptr; i++) {
        PX_3D_Object* obj = &engine_3drenderer_main_scene.objects[loads[i]];
        if (obj->type == OBJECT_3D_TYPE_MESH) {
            px_rs_loader_destroy_load(&engine_3drenderer_main_scene, obj);
        }
    }
	load_ptr = 0;
    editor_new_project(&engine_3drenderer_main_scene, "Untitled");
	enginef_init_3drenderer_main_scene();
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
        if (obj->type == OBJECT_3D_TYPE_MESH) {
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

void menu_evs_init(PX_Dropdown* menu_dd, char* path_to_save) {
    menu_dropdown = menu_dd;
    save_path = path_to_save;
}

void menu_evs_handle_events(PX_Event_GSignal* signal) {
    switch (signal->type) {
        case EVENT_GSIGNAL_UI_DROPDOWN_CLICK:
            if (signal->ui_dropdown_click.dropdown != menu_dropdown) return;
            switch (signal->ui_dropdown_click.opened_index) {
                case 0: // FILE
                    switch (signal->ui_dropdown_click.clicked_option) {
                        case 0: handle_file_new(); break;
                        case 1: handle_file_open(); break;
                        case 2: handle_file_save(); break;
                        case 3: handle_file_save_as(); break;
                        case 4: handle_file_import(); break;
                        case 5: handle_file_quit(); break;
                        default: return;
                    }
                    break;
                case 1: return;
                case 2: // VIEW
					switch (signal->ui_dropdown_click.clicked_option) {
						case 0: handle_view_fullscreen(); break;
						default: return;
					}
                case 3:
                default: return;
            }
            break;
        default: return;
    }
}
