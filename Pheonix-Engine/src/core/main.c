#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

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
// Windows
static PX_Window engine_window_main = (PX_Window){
    .width = 1000,
    .height = 800,
    .title = "Pheonix Engine",
    .handle = -1
};
// Fonts
static PX_Font* engine_font_ui = NULL;
// Mouse Info
static int engine_mouse_x = 0;
static int engine_mouse_y = 0;
// Rendering Objects
static PX_Dropdown engine_menu_dropdown = {0};
// Colors
static PX_Color4 engine_ui_black_panel_color = (PX_Color4){0x1A, 0x1A, 0x1A, 0xFF};

static void print_help(void) {
    printf("Usage: pheonix-engine [--COMMANDS]\n");
    printf("Commands:\n");
    printf("\tbuild-psdf <.json file containing SDF info> <output PSDF path>: Builds PSDF files from SDF files\n");
    printf("\thelp: Prints this help message\n");
}

static void parse_args(t_args* args, int argc, char** argv) {
    args->valid = true;
    args->help = false;
    args->build_psdf = false;
    args->build_psdf_json = NULL;
    args->build_psdf_out = NULL;
    
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
            args->build_psdf_json = argv[i + 1];
            args->build_psdf_out = argv[i + 2];
            i += 2;
        } else {
            fprintf(stderr, "Usage: pheonix-engine [--COMMANDS]\n\tUse --help for more info!\n");
            args->valid = false;
            break;
        }
    }
}

static char* enginef_helper_strdup(const char* s) {
    char* out = (char*)malloc(strlen(s) + 1);
    if (!out)
        return NULL;

    strcpy(out, s);
    out[strlen(s) + 1] = '\0';
    return out;
}

static void enginef_cleanup(void) {
    for (int i = 0; i < engine_menu_dropdown.item_count; i++) {
        if (engine_menu_dropdown.items[i].label)
            free(engine_menu_dropdown.items[i].label);

        for (int j = 0; j < engine_menu_dropdown.items[i].option_count; j++) {
            if (engine_menu_dropdown.items[i].options[j].label)
                free(engine_menu_dropdown.items[i].options[j].label);
        }
    }

    px_font_destroy(engine_font_ui);
    px_rs_shutdown_ui();
    px_ws_destroy(&engine_window_main);
    px_ws_shutdown();
}

static void enginef_init_dropdowns(void) {
    engine_menu_dropdown.font = engine_font_ui;
    engine_menu_dropdown.font_size = 16.0f;
    engine_menu_dropdown.pos = (PX_Vector2){0, 0};
    engine_menu_dropdown.width = engine_window_main_w;
    engine_menu_dropdown.height = 30;
    engine_menu_dropdown.color = engine_ui_black_panel_color;
    engine_menu_dropdown.hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xD4};
    engine_menu_dropdown.text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
    engine_menu_dropdown.hover_index = -1;
    engine_menu_dropdown.item_count = 4;
    engine_menu_dropdown.stext_pos = (PX_Vector2){4, 8};
    engine_menu_dropdown.spacing = 64;
    engine_menu_dropdown.noise = 0.03f;
    engine_menu_dropdown.cradius = 0.0f;

    const char* menu_labels[] = {"File", "Edit", "View", "Help"};
    const char* file_menu[] = {"New", "Open", "Save", "Save As", "Exit"};
    const char* edit_menu[] = {"Undo", "Redo"};
    const char* view_menu[] = {"Fullscreen"};
    const char* help_menu[] = {"About"};

    engine_menu_dropdown.items[0].option_count = 5;
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
        item->panel_color = engine_ui_black_panel_color;
        item->hover_color = (PX_Color4){0xD4, 0xD4, 0xD4, 0xFF};
        item->text_color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
        item->is_open = false;
        item->hover_index = -1;
        item->panel_noise = 0.03f;
        item->panel_cradius = 16.0f;
        item->panel_tran = (PX_Transform2){
            (PX_Vector2){x, 32},
            (PX_Scale2){100, item->spacing * item->option_count + 16}
        };
        item->stext_pos = (PX_Vector2){x + 6, 42};

        x += engine_menu_dropdown.spacing + px_rs_text_width(engine_menu_dropdown.font, item->label, engine_menu_dropdown.font_size);

        for (int j = 0; j < item->option_count; j++) {
            PX_DropdownOption* option = &item->options[j];
            const char** labels;

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
}

static void enginef_event_mouse_click(void) {
    // Dropdowns
    event_click_dropdown(&engine_menu_dropdown);
}

static void enginef_event_hover_check(void) {
    // Dropdowns
    event_hover_dropdown(&engine_menu_dropdown);
}

static void enginef_core_render(void) {
    // Core call
    px_rs_frame_start();

    // Scene Panel
    editor_draw_scene_panel(
        (PX_Transform2){(PX_Vector2){0, engine_menu_dropdown.height}, (PX_Scale2){(int)(engine_window_main_w / 4), engine_window_main_h}},
        (PX_Color4){0x0A, 0x0A, 0x0A, 0x80},
        (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF},
        engine_ui_black_panel_color,
        0.03f, 0.0f,
        engine_font_ui, 16.0f,
        8, 32
    );

    // Dropdowns
    engine_menu_dropdown.width = engine_window_main_w;
    px_rs_draw_dropdown(&engine_menu_dropdown);
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
        fprintf(stderr, "Error: Failed to initialize window system!\n");
        return last_err;
    }

    last_err = px_ws_create(&engine_window_main);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to create window!\n");
        px_ws_shutdown();
        return last_err;
    }

    px_ws_window_design(&engine_window_main, &engine_window_main_design);

    last_err = px_ws_show_splash(&engine_window_main);
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to display splash screen!\n");
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }
    
    px_ws_create_ctx(&engine_window_main);
    last_err = px_rs_init_ui((PX_Scale2){engine_window_main_w, engine_window_main_h});
    if (last_err != ERR_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize rendering system!\n");
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return last_err;
    }

    event_sys_init((PX_Scale2){engine_window_main_w, engine_window_main_h}, (PX_Vector2){0});

    // Load Fonts
    engine_font_ui = px_font_load("assets/fonts/psdf/roboto.psdf");
    if (!engine_font_ui) {
        fprintf(stderr, "Error: Failed to load UI font\n");
        px_rs_shutdown_ui();
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return ERR_COULD_NOT_OPEN_FILE;
    }

    // Load Objects
    // Dropdowns
    enginef_init_dropdowns();
    // Project
    editor_new_project("Untitled");

    // Render
    engine_running = true;
    while (engine_running) {
        px_rs_ui_frame_update();
        enginef_core_render();
        enginef_event_hover_check();

        last_err = px_ws_poll(&engine_window_main);
        if (last_err != ERR_SUCCESS) {
            fprintf(stderr, "Error: Failed to poll events!\n");
            enginef_cleanup();
            return last_err;
        }

        PX_WEvent ev;
        while (px_ws_pop_event(&engine_window_main, &ev)) {
            switch (ev.type) {
                case PX_WE_CLOSE: engine_running = false; break;
                case PX_WE_RESIZE:
                    engine_window_main_w = ev.w;
                    engine_window_main_h = ev.h;
                    engine_window_main.width = ev.w;
                    engine_window_main.height = ev.h;
                    px_rs_ui_resize((PX_Scale2){ev.w, ev.h});
                    event_resize((PX_Scale2){ev.w, ev.h});
                    break;
                case PX_WE_MOUSE_MOVE:
                    engine_mouse_x = ev.x;
                    engine_mouse_y = ev.y;
                    event_mouse_move((PX_Vector2){ev.x, ev.y});
                    break;
                case PX_WE_MOUSE_DOWN:
                    enginef_event_mouse_click();
                    break;
                default: break;
            }
        } 

        px_rs_frame_end();
        px_ws_swap_buffers(&engine_window_main);
    }

    // Cleanup
    enginef_cleanup();

    return ERR_SUCCESS;
}
