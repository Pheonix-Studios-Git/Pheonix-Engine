#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <pheonix-engine.h>
#include <err-codes.h>
#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>
#include <font.h>
#include <editor.h>
#include <event.h>

typedef struct {
    bool valid;
    bool build_psdf;
    char* build_psdf_json;
    char* build_psdf_out;
    bool help;
	bool vsync;
	PX_EditorMode base_scene_type;
	PX_GPU_Backend gpu_backend;
} t_args;

// Main
static bool engine_running = false;

// Window Info
static int engine_window_main_w = 1000;
static int engine_window_main_h = 800;
static PX_WindowDesign engine_window_main_design = (PX_WindowDesign){
    .bg_color = 0xFFFFFFFF,
    .fg_color = 0x000000FF,
    .border_radius = 0,
    .border_thickness = 0,
    .border_color = 0x00000000,
    .title_bar_height = 0,
    .title_bar_color = 0x00000000,
    .title_text_color = 0x00000000
};

// Window Contexts
PX_WContext engine_window_context_main = {0};

// Windows
PX_Window engine_window_main = (PX_Window){
    .width = 1000,
    .height = 800,
    .title = "Pheonix Engine",
    .handle = -1,
    .vsync_off = false
};

// Fonts
static PX_Font* engine_font_ui = NULL;

// Mouse Info
static int engine_mouse_x = 0;
static int engine_mouse_y = 0;
static int engine_mouse_saved_x = 0;
static int engine_mouse_saved_y = 0;
static bool engine_mouse_locked = false;
static bool engine_mouse_ignore1 = false;

// Dropdowns
static PX_Dropdown engine_menu_dropdown = {0};
static PX_Dropdown engine_scene_panel_context_menu = {0};

// Colors
static PX_Color4 engine_2d_black_panel_color = (PX_Color4){0x1A, 0x1A, 0x1A, 0xFF};

// Identifiers
static PX_Event_Identifier engine_obj_identifiers[10];
static PX_Event_Identifier* engine_obj_identifiers_x[10];
static int engine_obj_identifier_count = 0;

// Engine 3D Objects
PX_EditorGrid_3D engine_3drenderer_editor_grid = {
    .visible = true,
    .half_size = 100,
    .color = (PX_Color4){0x24, 0x24, 0x24, 0xFF},
    .spacing = 5.0f
};
static size_t engine_3drenderer_gizmo_idx;
// Engine 2D Objects

PX_EditorGrid_2D engine_2drenderer_editor_grid = {
    .visible = true,
    .half_size = 100,
    .color = (PX_Color4){0x24, 0x24, 0x24, 0xFF},
    .spacing = 5.0f
};
static size_t engine_2drenderer_gizmo_idx;

// 3D Renderer
PX_Scene_3D engine_3drenderer_main_scene = {0};
static float engine_3drenderer_scene_cam_speed = 1.0f;
static bool engine_3drenderer_scene_cam_speed_doubled = false;
static bool engine_3drenderer_hover_on_gizmo = false;

// 2D Renderer
PX_Scene_2D engine_2drenderer_main_scene = {0};
static float engine_2drenderer_scene_cam_speed = 6.0f;
static bool engine_2drenderer_scene_cam_speed_doubled = false;
static bool engine_2drenderer_hover_on_gizmo = false;

// Anchors
static PX_AnchorRect engine_anchor_menubar = {
    .x = 0.0f, .y = 0.0f,
    .w = 1.0f, .h = 0.05f
};
static PX_AnchorRect engine_anchor_scene_panel = {
    .x = 0.0f, .y = 0.05f,
    .w = 0.25f, .h = 0.95f
};
static PX_AnchorRect engine_anchor_scene_editor = {
    .x = 0.25f, .y = 0.05f,
    .w = 0.75f, .h = 0.95f
};
static PX_AnchorRect engine_anchor_all = {
    .x = 0.0f, .y = 0.0f,
    .w = 1.0f, .h = 1.0f
};

static void print_help(void) {
    printf("Usage: pheonix-engine [--COMMANDS]\n");
    printf("Commands:\n");
    printf("\tbuild-psdf <.json file containing SDF info> <output PSDF path>: Builds PSDF files from SDF files\n");
	printf("\tbase-scene-type <3d|2d>: Specifies the starting scene type. Value can be only '3d' or '2d'\n");
	printf("\tgpu-backend <vulkan|opengl>: Specifies the GPU Backend API to use. Value can be only 'vulkan' or 'opengl'\n");
	printf("\tno-vsync: Turns off Vertical Synchronization\n");
    printf("\thelp: Prints this help message\n");
}

static void parse_args(t_args* args, int argc, char** argv) {
    args->valid = true;
    args->help = false;
    args->build_psdf = false;
    args->build_psdf_json = NULL;
    args->build_psdf_out = NULL;
	args->base_scene_type = PX_EDITOR_MODE_3D;
	args->gpu_backend = PX_RS_GPU_BACKEND_OPENGL;
	args->vsync = true;
    
    for (int i = 1; i < argc; i++) {
        char* opt = argv[i];

        if (strcmp(opt, "--help") == 0) {
            args->help = true;
        } else if (strcmp(opt, "--build-psdf") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Usage: pheonix-engine --build-psdf <json> <output>\n\tUse --help for more info!\n");
                args->valid = false;
                break;
            }
           
            args->build_psdf = true;
            args->build_psdf_json = argv[++i];
            args->build_psdf_out = argv[++i];
        } else if (strcmp(opt, "--base-scene-type") == 0) {
			if (i + 1 >= argc) {
                fprintf(stderr, "Usage: pheonix-engine --base-scene-type <3d|2d>\n\tUse --help for more info!\n");
                args->valid = false;
                break;
            }

			char* type = argv[++i];
			if (strcmp(type, "3d") == 0) {
				args->base_scene_type = PX_EDITOR_MODE_3D;
			} else if (strcmp(type, "2d") == 0) {
				args->base_scene_type = PX_EDITOR_MODE_2D;
			} else {
				fprintf(stderr, "Usage: pheonix-engine --base-scene-type <3d|2d>\n\tBase Scene Type can only be '3d' or '2d'\n\tUse --help for more info!\n");
                args->valid = false;
                break;
			}
		} else if (strcmp(opt, "--gpu-backend") == 0) {
			if (i + 1 >= argc) {
                fprintf(stderr, "Usage: pheonix-engine --gpu-backend <vulkan|opengl>\n\tUse --help for more info!\n");
                args->valid = false;
                break;
            }

			char* type = argv[++i];
			if (strcmp(type, "vulkan") == 0) {
				args->gpu_backend = PX_RS_GPU_BACKEND_VULKAN;
			} else if (strcmp(type, "opengl") == 0) {
				args->gpu_backend = PX_RS_GPU_BACKEND_OPENGL;
			} else {
				fprintf(stderr, "Usage: pheonix-engine --gpu-backend <vulkan|opengl>\n\tGPU Backend API can only be 'vulkan' or 'opengl'\n\tUse --help for more info!\n");
                args->valid = false;
                break;
			}
		} else if (strcmp(opt, "--no-vsync") == 0) {
			args->vsync = false;
		} else {
            fprintf(stderr, "Usage: pheonix-engine [--COMMANDS]\n\tUse --help for more info!\n");
            args->valid = false;
            break;
        }
    }
}

static char* enginef_helper_strdup(const char* s) {
	size_t len = strlen(s);

    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;

    memcpy(out, s, len);
	out[len] = '\0';
    return out;
}

static void enginef_cleanup(void) {
    for (int i = 0; i < engine_menu_dropdown.item_count; i++) {
        if (engine_menu_dropdown.items[i].label) free(engine_menu_dropdown.items[i].label);
        for (int j = 0; j < engine_menu_dropdown.items[i].option_count; j++) {
            if (engine_menu_dropdown.items[i].options[j].label) free(engine_menu_dropdown.items[i].options[j].label);
        }
    }
	for (int i = 0; i < engine_scene_panel_context_menu.item_count; i++) {
        if (engine_scene_panel_context_menu.items[i].label) free(engine_scene_panel_context_menu.items[i].label);
        for (int j = 0; j < engine_scene_panel_context_menu.items[i].option_count; j++) {
            if (engine_scene_panel_context_menu.items[i].options[j].label) free(engine_scene_panel_context_menu.items[i].options[j].label);
        }
    }

    px_font_destroy(engine_font_ui);
    px_rs_shutdown();
    px_ws_destroy(&engine_window_main);
    px_ws_shutdown();
}

PX_Transform2 enginef_convert_anchor_to_transform(PX_AnchorRect r) {
    return (PX_Transform2){
        .pos = {
            .x = r.x * engine_window_main_w,
            .y = r.y * engine_window_main_h
        },
        .scale = {
            .w = r.w * engine_window_main_w,
            .h = r.h * engine_window_main_h
        },
		.rot = 0
    };
}

static void enginef_init_dropdowns(void) {
    PX_Transform2 menubar_t = enginef_convert_anchor_to_transform(engine_anchor_menubar);
    engine_menu_dropdown.font = engine_font_ui;
    engine_menu_dropdown.font_size = 16.0f;
    engine_menu_dropdown.pos = menubar_t.pos;
    engine_menu_dropdown.width = menubar_t.scale.w;
    engine_menu_dropdown.height = menubar_t.scale.h;
    engine_menu_dropdown.color = engine_2d_black_panel_color;
    engine_menu_dropdown.hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xD4};
    engine_menu_dropdown.text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
    engine_menu_dropdown.hover_index = -1;
    engine_menu_dropdown.item_count = 4;
    engine_menu_dropdown.stext_pos = (PX_Vector2){4, 8};
    engine_menu_dropdown.spacing = 64;
    engine_menu_dropdown.noise = 0.03f;
    engine_menu_dropdown.cradius = 0.0f;
	engine_menu_dropdown.visible = true;

    const char* menu_labels[] = {"File", "Edit", "View", "Help"};
    const char* file_menu[] = {"New", "Open", "Save", "Save As", "Import", "Exit"};
    const char* edit_menu[] = {"Undo", "Redo"};
    const char* view_menu[] = {"Fullscreen"};
    const char* help_menu[] = {"About"};

    engine_menu_dropdown.items[0].option_count = 6;
    engine_menu_dropdown.items[1].option_count = 2;
    engine_menu_dropdown.items[2].option_count = 1;
    engine_menu_dropdown.items[3].option_count = 1;

    int x = engine_menu_dropdown.stext_pos.x;
    for (int i = 0; i < 4; i++) {
        PX_DropdownItem* item = &engine_menu_dropdown.items[i];
        item->label = enginef_helper_strdup(menu_labels[i]);
        item->height = 16;
        item->width = px_rs_text_width(engine_font_ui, menu_labels[i], engine_menu_dropdown.font_size);
        item->spacing = 16;
        item->font_size = 14.0f;;
        item->panel_color = engine_2d_black_panel_color;
        item->hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xFF};
        item->text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
        item->is_open = false;
        item->hover_index = -1;
        item->panel_noise = 0.03f;
        item->panel_cradius = 16.0f;
        item->panel_tran = (PX_Transform2){
            (PX_Vector2){0, engine_menu_dropdown.height}, // How away from main panel
            (PX_Scale2){100, item->spacing * item->option_count + 16}
        };
        item->stext_pos = (PX_Vector2){6, 10};

        x += item->width + engine_menu_dropdown.spacing;

        for (int j = 0; j < item->option_count; j++) {
            PX_DropdownOption* option = &item->options[j];
            const char** labels = NULL;

            switch(i) {
                case 0: labels = file_menu; break;
                case 1: labels = edit_menu; break;
                case 2: labels = view_menu; break;
                case 3: labels = help_menu; break;
                default: break;
            }

            option->label = enginef_helper_strdup(labels[j]);
            option->height = 8;
            option->width = px_rs_text_width(engine_font_ui, labels[j], engine_menu_dropdown.font_size);
        }
    }

    engine_obj_identifiers[engine_obj_identifier_count++] = (PX_Event_Identifier){
        .ptr = (void*)&engine_menu_dropdown,
        .identifier = "menubar"
    };
    engine_obj_identifiers_x[engine_obj_identifier_count-1] = (PX_Event_Identifier*)&engine_obj_identifiers[engine_obj_identifier_count-1];

	engine_scene_panel_context_menu.font = engine_font_ui;
    engine_scene_panel_context_menu.font_size = 18.0f;
    engine_scene_panel_context_menu.pos = (PX_Vector2){0};
    engine_scene_panel_context_menu.width = 100;
    engine_scene_panel_context_menu.height = 100;
    engine_scene_panel_context_menu.color = engine_2d_black_panel_color;
    engine_scene_panel_context_menu.hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xD4};
    engine_scene_panel_context_menu.text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
    engine_scene_panel_context_menu.hover_index = -2;
    engine_scene_panel_context_menu.item_count = 1;
    engine_scene_panel_context_menu.stext_pos = (PX_Vector2){4, 8};
    engine_scene_panel_context_menu.spacing = 22;
    engine_scene_panel_context_menu.noise = 0.03f;
    engine_scene_panel_context_menu.cradius = 25.0f;
	engine_scene_panel_context_menu.visible = false;

    const char* scene_panel_context_menu_labels[] = {"Create"};
    const char* create_context_menu[] = {"Panel"};

    engine_scene_panel_context_menu.items[0].option_count = 1;

    for (int i = 0; i < 1; i++) {
        PX_DropdownItem* item = &engine_scene_panel_context_menu.items[i];
        item->label = enginef_helper_strdup(scene_panel_context_menu_labels[i]);
        item->height = 16;
        item->width = px_rs_text_width(engine_font_ui, menu_labels[i], engine_scene_panel_context_menu.font_size);
        item->spacing = 16;
        item->font_size = 14.0f;;
        item->panel_color = engine_2d_black_panel_color;
        item->hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xFF};
        item->text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
        item->is_open = false;
        item->hover_index = -1;
        item->panel_noise = 0.03f;
        item->panel_cradius = 16.0f;
        item->panel_tran = (PX_Transform2){
            (PX_Vector2){engine_scene_panel_context_menu.width + 16, 0},
            (PX_Scale2){100, item->spacing * item->option_count + 16}
        };
        item->stext_pos = (PX_Vector2){6, 10};

        x += engine_scene_panel_context_menu.spacing + px_rs_text_width(engine_scene_panel_context_menu.font, item->label, engine_scene_panel_context_menu.font_size);

        for (int j = 0; j < item->option_count; j++) {
            PX_DropdownOption* option = &item->options[j];
            const char** labels = NULL;

            switch(i) {
                case 0: labels = create_context_menu; break;
                default: break;
            }

            option->label = enginef_helper_strdup(labels[j]);
            option->height = 8;
            option->width = px_rs_text_width(engine_font_ui, labels[j], engine_scene_panel_context_menu.font_size);
        }
    }

    engine_obj_identifiers[engine_obj_identifier_count++] = (PX_Event_Identifier){
        .ptr = (void*)&engine_scene_panel_context_menu,
        .identifier = "scene-panel-context-menu"
    };
    engine_obj_identifiers_x[engine_obj_identifier_count-1] = (PX_Event_Identifier*)&engine_obj_identifiers[engine_obj_identifier_count-1];
}

static void enginef_event_mouse_click(void) {
    // Dropdowns
    event_click_dropdown(&engine_menu_dropdown, false);
	event_click_dropdown(&engine_scene_panel_context_menu, true);

    // Scene Panel
    editor_click_scene_panel(
        (PX_Vector2){engine_mouse_x, engine_mouse_y},
        enginef_convert_anchor_to_transform(engine_anchor_scene_panel),
        engine_font_ui, 16.0f,
        8, 16
    );
}

static void enginef_event_hover_check(void) {
    // Dropdowns
    event_hover_dropdown(&engine_menu_dropdown);
	event_hover_dropdown(&engine_scene_panel_context_menu);
}

static void enginef_core_render(void) {
    // Scene Panel
    editor_draw_scene_panel(
		engine_anchor_all,
        (PX_Vector2){engine_mouse_x, engine_mouse_y},
        enginef_convert_anchor_to_transform(engine_anchor_scene_panel),
        (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF},
        (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF},
        engine_2d_black_panel_color,
        (PX_Color4){0xD4, 0xD4, 0xD4, 0xFF},
        0.03f, 0.0f,
        engine_font_ui, 16.0f,
        8, 16
    );

    // Dropdowns
    PX_Transform2 menubar_t = enginef_convert_anchor_to_transform(engine_anchor_menubar);
    engine_menu_dropdown.width = menubar_t.scale.w;
    engine_menu_dropdown.height = menubar_t.scale.h;
    engine_menu_dropdown.pos = menubar_t.pos;
    px_rs_draw_dropdown(engine_anchor_all, &engine_menu_dropdown);
	px_rs_draw_dropdown(engine_anchor_all, &engine_scene_panel_context_menu);

    // Gizmos
    if (engine_3drenderer_main_scene.active_object != NULL) {
        if (engine_3drenderer_gizmo_idx > 0 && engine_3drenderer_gizmo_idx < engine_3drenderer_main_scene.editor_object_count) {
            PX_3D_Editor_Object* gizmo = &engine_3drenderer_main_scene.editor_objects[engine_3drenderer_gizmo_idx];
            gizmo->local_transform = engine_3drenderer_main_scene.active_object->local_transform;
            gizmo->world_transform = engine_3drenderer_main_scene.active_object->world_transform;
            gizmo->active = true;
        }
    }
	if (engine_2drenderer_main_scene.active_object != NULL) {
        if (engine_2drenderer_gizmo_idx > 0 && engine_2drenderer_gizmo_idx < engine_2drenderer_main_scene.editor_object_count) {
            PX_2D_Editor_Object* gizmo = &engine_2drenderer_main_scene.editor_objects[engine_2drenderer_gizmo_idx];
            gizmo->local_transform = engine_2drenderer_main_scene.active_object->local_transform;
            gizmo->world_transform = engine_2drenderer_main_scene.active_object->world_transform;
            gizmo->active = true;
        }
    }
}

static void enginef_core_handle_core_signals(PX_Event_GSignal* core_signal, bool core_signal_active) {
    if (!core_signal || !core_signal_active) return;

    switch (core_signal->type) {
        case EVENT_GSIGNAL_CORE_QUIT:
            engine_running = false;
            break;
        default: break;
    }
}

static void enginef_core_handle_gsignals(PX_Event_GSignal* signal, bool core_signal_active) {
    if (!signal || core_signal_active) return;

    switch (signal->type) {
        case EVENT_GSIGNAL_3D_HOVER: {
            PX_Event_GSignal_3dHover s = signal->mouse_hover_on_3d;
            switch (s.id) {
                case 1: {
                    if (s.objType != PX_RS_OBJECT_3D_EDITOR_GIZMO) break;
                    engine_3drenderer_hover_on_gizmo = true;
                    break;
                }
                default: break;
            }
			break;
        }
		case EVENT_GSIGNAL_2D_HOVER: {
            PX_Event_GSignal_2dHover s = signal->mouse_hover_on_2d;
            switch (s.id) {
                case 1: {
                    if (s.objType != PX_RS_OBJECT_2D_EDITOR_GIZMO) break;
                    engine_2drenderer_hover_on_gizmo = true;
                    break;
                }
                default: break;
            }
			break;
        }
        default: break;
    }
}

void enginef_init_3drenderer_main_scene(void) {
    PX_3D_Editor_Object gridlines = {
        .active = true,
        .name = "Grid Lines",
        .type = PX_RS_OBJECT_3D_EDITOR_GRID,
        .has_children = false,
        .static_object = true,
        .local_transform = (PX_Transform3){.pos=(PX_Vector3){0, 0, 0}, .scale=(PX_Scale3){1, 1, 1}, .rot=(PX_Orientation3){.w=1}},
        .world_transform = (PX_Transform3){.pos=(PX_Vector3){0, 0, 0}, .scale=(PX_Scale3){1, 1, 1}, .rot=(PX_Orientation3){.w=1}},
        .ex_data_type = PX_RS_OBJECT_3D_EDITOR_GRID,
        .ex_data = &engine_3drenderer_editor_grid,
        .id = 0
    };
    engine_3drenderer_main_scene.editor_objects[engine_3drenderer_main_scene.editor_object_count++] = gridlines;
    PX_3D_Editor_Object gizmo = {
        .active = false,
        .name = "Test Gizmo",
        .type = PX_RS_OBJECT_3D_EDITOR_GIZMO,
        .has_children = false,
        .static_object = true,
        .local_transform = (PX_Transform3){.pos=(PX_Vector3){0, 0, 0}, .scale=(PX_Scale3){1, 1, 1}, .rot=(PX_Orientation3){.w=1}},
        .world_transform = (PX_Transform3){.pos=(PX_Vector3){0, 0, 0}, .scale=(PX_Scale3){1, 1, 1}, .rot=(PX_Orientation3){.w=1}},
        .ex_data_type = PX_RS_OBJECT_3D_EDITOR_GIZMO,
        .ex_data = &engine_3drenderer_hover_on_gizmo,
        .id = 1
    };
    engine_3drenderer_gizmo_idx = engine_3drenderer_main_scene.editor_object_count;
    engine_3drenderer_main_scene.editor_objects[engine_3drenderer_main_scene.editor_object_count++] = gizmo;
}

void enginef_init_2drenderer_main_scene(void) {
    PX_2D_Editor_Object gridlines = {
        .active = true,
        .name = "Grid Lines",
        .type = PX_RS_OBJECT_2D_EDITOR_GRID,
        .has_children = false,
        .static_object = true,
        .local_transform = (PX_Transform2){.pos=(PX_Vector2){0, 0}, .scale=(PX_Scale2){1, 1}, .rot=0},
        .world_transform = (PX_Transform2){.pos=(PX_Vector2){0, 0}, .scale=(PX_Scale2){1, 1}, .rot=0},
        .ex_data_type = PX_RS_OBJECT_2D_EDITOR_GRID,
        .ex_data = &engine_2drenderer_editor_grid,
        .id = 0
    };
    engine_2drenderer_main_scene.editor_objects[engine_2drenderer_main_scene.editor_object_count++] = gridlines;
    PX_2D_Editor_Object gizmo = {
        .active = false,
        .name = "Test Gizmo",
        .type = PX_RS_OBJECT_2D_EDITOR_GIZMO,
        .has_children = false,
        .static_object = true,
        .local_transform = (PX_Transform2){.pos=(PX_Vector2){0, 0}, .scale=(PX_Scale2){1, 1}, .rot=0},
        .world_transform = (PX_Transform2){.pos=(PX_Vector2){0, 0}, .scale=(PX_Scale2){1, 1}, .rot=0},
        .ex_data_type = PX_RS_OBJECT_2D_EDITOR_GIZMO,
        .ex_data = &engine_2drenderer_hover_on_gizmo,
        .id = 1
    };
    engine_2drenderer_gizmo_idx = engine_2drenderer_main_scene.editor_object_count;
    engine_2drenderer_main_scene.editor_objects[engine_2drenderer_main_scene.editor_object_count++] = gizmo;
}

static double enginef_core_get_time() {
    time_t ts;
    time(&ts);

    return (double)ts;
}

int main(int argc, char** argv) {
    // Important Variables
    t_err_codes last_err = ERR_SUCCESS;

    // Parse Args
    t_args passed_args = {0};
    parse_args(&passed_args, argc, argv);

    if (!passed_args.valid) {
        return ERR_USAGE;
    }

    if (passed_args.build_psdf) {
        if (!passed_args.build_psdf_json || !passed_args.build_psdf_out)
            return ERR_USAGE;
        PX_SDFBuildDesc psdf_desc = {
            .pixel_size = 64, // 128 - HIGH DPI
            .atlas_size = 512, // 1024 - EXT
            .sdf_range = 8, // 16 - EXT
            .ascii_only = true // false - EXT
        };
        return px_sdf_build_font(passed_args.build_psdf_json, passed_args.build_psdf_out, &psdf_desc);
    }

    if (passed_args.help) {
        print_help();
        return ERR_SUCCESS;
    }

    // Initialize SubSystems
    last_err = px_ws_init();
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize window system! (%u)\n", last_err);
        return last_err;
    } 

    last_err = px_ws_show_splash();
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to display splash screen! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    last_err = px_ws_create(&engine_window_main, passed_args.gpu_backend);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to create window! (%u)\n", last_err);
        px_ws_shutdown();
        return last_err;
    }

    px_ws_window_design(&engine_window_main, &engine_window_main_design);
    engine_window_main.vsync_off = !passed_args.vsync;
    px_ws_create_ctx(&engine_window_main);

	last_err = px_ws_get_ctx(&engine_window_main, &engine_window_context_main);
	if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to retrieve Window-GPU Context! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

	last_err = px_rs_change_backend(passed_args.gpu_backend);
	if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to change GPU API Backend! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    last_err = px_rs_init(&engine_window_context_main);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize rendering system! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    last_err = px_rs_init_2d(engine_anchor_all);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize UI rendering system! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    last_err = px_rs_init_3d(engine_anchor_scene_editor);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize 3D rendering system! (%u)\n", last_err);
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    event_sys_init((PX_Scale2){engine_window_main_w, engine_window_main_h}, (PX_Vector2){0});

    // Load Fonts
    engine_font_ui = px_font_load("assets/fonts/psdf/roboto.psdf");
    if (!engine_font_ui) {
        fprintf(stderr, "Error: Failed to load UI font\n");
        px_rs_shutdown();
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return ERR_COULD_NOT_OPEN_FILE;
    }

    // Load Objects
    // Dropdowns
    enginef_init_dropdowns();
    menu_evs_init(&engine_menu_dropdown, NULL);
    // Project
    editor_new_project(&engine_3drenderer_main_scene, &engine_2drenderer_main_scene, "Untitled", passed_args.base_scene_type);
	PX_EditorState* editor_state = editor_get_state();

    // Load scenes
    enginef_init_3drenderer_main_scene();
	enginef_init_2drenderer_main_scene();

    // Configure Scene Cam
    px_rs_config_scene_cam_3d(-1.0f, engine_3drenderer_scene_cam_speed);
	px_rs_config_scene_cam_2d(engine_2drenderer_scene_cam_speed);

    // Frame stuff
    double last_time = enginef_core_get_time();
    double fps_timer = 0.0;
    int frames = 0;

    // Render
    engine_running = true;
    printf("\n");
    while (engine_running) {
        double current_time = enginef_core_get_time();
        double delta_time = current_time - last_time;
        last_time = current_time;

        fps_timer += delta_time;
        frames++;

        if (fps_timer >= 0.1) {
            double fps = frames / fps_timer;

            printf("\rFPS: %.2f", fps);
            fflush(stdout);

            frames = 0;
            fps_timer = 0.0;
        }

        engine_3drenderer_hover_on_gizmo = false;

        px_rs_frame_start();
        px_rs_frame_update();

        // Draw 3D
		if (editor_state->current_mode == PX_EDITOR_MODE_3D) {
			px_rs_draw_scene_3d(&engine_3drenderer_main_scene);
			px_rs_draw_editor_objects_3d(&engine_3drenderer_main_scene);
		}
        
        // Draw 2D
		if (editor_state->current_mode == PX_EDITOR_MODE_2D) {
			px_rs_draw_scene_2d(engine_anchor_scene_editor, &engine_2drenderer_main_scene);
			px_rs_draw_editor_objects_2d(engine_anchor_scene_editor, &engine_2drenderer_main_scene);
		}
        enginef_core_render();

        px_rs_handle_mouse_move((PX_Vector2){engine_mouse_x, engine_mouse_y}, (PX_Scale2){engine_window_main_w, engine_window_main_h});
		enginef_event_hover_check();

        // Global Signals
        PX_Event_GSignal core_signal = {0};
        bool core_signal_active = false;
        event_handle_gsignals((PX_Event_Identifier**)&engine_obj_identifiers_x, engine_obj_identifier_count, &core_signal, &core_signal_active);
        // Global Core Signals
        enginef_core_handle_core_signals(&core_signal, core_signal_active);
        // Global checks
        enginef_core_handle_gsignals(&core_signal, core_signal_active);

        last_err = px_ws_poll(&engine_window_main);
        if (last_err != ERR_SUCCESS) {
            fprintf(stderr, "Error: Failed to poll events! (%u)\n", last_err);
            enginef_cleanup();
            return last_err;
        }

        PX_WEvent ev;
        while (px_ws_pop_event(&engine_window_main, &ev)) {
            switch (ev.type) {
                case PX_WE_CLOSE:
                    PX_Event_GSignal csignal = {
                        .core_quit = true,
                        .type = EVENT_GSIGNAL_CORE_QUIT,
                    };
                    event_send_gsignal(&csignal);
                    break; // Cleansup properly
                case PX_WE_RESIZE:
                    engine_window_main_w = ev.w;
                    engine_window_main_h = ev.h;
                    engine_window_main.width = ev.w;
                    engine_window_main.height = ev.h;
                    px_rs_2d_resize(engine_anchor_all);
                    PX_Transform2 scene_editor = enginef_convert_anchor_to_transform(engine_anchor_scene_editor);
                    px_rs_3d_resize(engine_anchor_scene_editor);
                    event_resize((PX_Scale2){ev.w, ev.h});
                    if (engine_mouse_locked) {
                        px_ws_set_mouse_pos(&engine_window_main, (PX_Vector2){.x=engine_mouse_saved_x,.y=engine_mouse_saved_y});
                        engine_mouse_ignore1 = true;
                    }
                    break;
                case PX_WE_MOUSE_MOVE:
                    event_mouse_move((PX_Vector2){ev.x, ev.y});
                    engine_mouse_x = ev.x;
                    engine_mouse_y = ev.y;
                    if (engine_mouse_ignore1) {
                        engine_mouse_ignore1 = false;
                        break;
                    }
                    if (!engine_mouse_locked) break;
                    PX_Vector2 mDelta = {.x=ev.x-engine_mouse_saved_x, .y=ev.y-engine_mouse_saved_y};

                    if (editor_state->current_mode == PX_EDITOR_MODE_3D) px_rs_update_scene_cam_3d(mDelta, EKeycode_Unknown);
					else if (editor_state->current_mode == PX_EDITOR_MODE_2D) px_rs_update_scene_cam_2d(mDelta, EKeycode_Unknown);

                    px_ws_set_mouse_pos(&engine_window_main, (PX_Vector2){.x=engine_mouse_saved_x,.y=engine_mouse_saved_y});
                    engine_mouse_ignore1 = true;
                    break;
                case PX_WE_MOUSE_DOWN:
                    switch (ev.keycode) {
                        case EKeycode_MouseLButton: {
                            enginef_event_mouse_click();
                            break;
                        }
                        case EKeycode_MouseRButton: {
                            if (is_mouse_on_anchor(engine_anchor_scene_editor)) {
								engine_mouse_locked = true;
								engine_mouse_saved_x = ev.x;
								engine_mouse_saved_y = ev.y;
								px_ws_set_mouse_locked(&engine_window_main, true);
							} else if (is_mouse_on_anchor(engine_anchor_scene_panel)) {
								engine_scene_panel_context_menu.pos = (PX_Vector2){ev.x, ev.y};
								engine_scene_panel_context_menu.visible = true;
							}
                            break;
                        }
                        
						case EKeycode_MouseScrollUp:
						case EKeycode_MouseScrollDown: {
							if (editor_state->current_mode != PX_EDITOR_MODE_2D) break;
							px_rs_update_scene_cam_2d((PX_Vector2){ev.x, ev.y}, ev.keycode);
						}
						default: break;
                    }
                    break;
                case PX_WE_MOUSE_UP:
                    switch (ev.keycode) {
                        case EKeycode_MouseRButton: {
                            if (engine_mouse_locked) {
                                engine_mouse_locked = false;
                                engine_mouse_saved_x = 0;
                                engine_mouse_saved_y = 0;
                                px_ws_set_mouse_locked(&engine_window_main, false);
                            }
                            break;
                        }
                        
						case EKeycode_MouseScrollUp:
						case EKeycode_MouseScrollDown: {
							if (editor_state->current_mode != PX_EDITOR_MODE_2D) break;
							px_rs_update_scene_cam_2d((PX_Vector2){ev.x, ev.y}, ev.keycode);
						}
						default: break;
                    }
                    break;
                case PX_WE_KEYDOWN:
                    switch (ev.keycode) {
                        case EKeycode_Escape: {
                            if (engine_mouse_locked) {
                                engine_mouse_locked = false;
                                engine_mouse_saved_x = 0;
                                engine_mouse_saved_y = 0;
                                px_ws_set_mouse_locked(&engine_window_main, false);
                            }
                            break;
                        }
                        case EKeycode_LShift: {
                            if (!(
                                engine_mouse_x >= engine_window_main_w / 4 &&
                                engine_mouse_y >= engine_menu_dropdown.height
                            )) break;
                            if (editor_state->current_mode == PX_EDITOR_MODE_3D) {
								px_rs_config_scene_cam_3d(-1.0f, engine_3drenderer_scene_cam_speed*2.0f);
								engine_3drenderer_scene_cam_speed_doubled = true;
							} else if (editor_state->current_mode == PX_EDITOR_MODE_2D) {
								px_rs_config_scene_cam_2d(engine_2drenderer_scene_cam_speed*2.0f);
								engine_2drenderer_scene_cam_speed_doubled = true;
							}
                            break;
                        }
                        default: {
                            if (editor_state->current_mode == PX_EDITOR_MODE_3D)
								px_rs_update_scene_cam_3d((PX_Vector2){ev.x, ev.y}, ev.keycode);
							else if (editor_state->current_mode == PX_EDITOR_MODE_2D)
								px_rs_update_scene_cam_2d((PX_Vector2){ev.x, ev.y}, ev.keycode);
                            break;
                        }
                    }
                    break;
                case PX_WE_KEYUP:
                    switch (ev.keycode) {
                        case EKeycode_LShift: {
                            if (editor_state->current_mode == PX_EDITOR_MODE_3D && engine_3drenderer_scene_cam_speed_doubled) {
								px_rs_config_scene_cam_3d(-1.0f, engine_3drenderer_scene_cam_speed);
								engine_3drenderer_scene_cam_speed_doubled = false;
							} else if (editor_state->current_mode == PX_EDITOR_MODE_2D && engine_2drenderer_scene_cam_speed_doubled) {
								px_rs_config_scene_cam_2d(engine_2drenderer_scene_cam_speed);
								engine_2drenderer_scene_cam_speed_doubled = false;
							}
                            break;
                        }
                        default: break;
                    }
                    break;
                default: break;
            }
        } 

        px_rs_frame_end();
        px_ws_swap_buffers(&engine_window_main);
    }
    printf("\n");

    if (engine_mouse_locked) {
        px_ws_set_mouse_locked(&engine_window_main, false);
        engine_mouse_locked = false;
    }

    // Cleanup
    enginef_cleanup();

    return ERR_SUCCESS;
}
