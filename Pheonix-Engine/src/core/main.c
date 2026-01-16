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

typedef struct {
    bool valid;
    bool build_psdf;
    char* build_psdf_json;
    char* build_psdf_out;
    bool help;
} t_args;

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

static void engine_cleanup(void) {
    px_font_destroy(engine_font_ui);
    px_rs_shutdown_ui();
    px_ws_destroy(&engine_window_main);
    px_ws_shutdown();
}

static void engine_core_render(void) {
    px_rs_draw_panel((PX_Scale2){engine_window_main_w, 30}, (PX_Vector2){0, 0}, (PX_Color4){0x2B, 0x2B, 0x2B, 0xFF});
    px_rs_render_text("File", 16.0f, (PX_Vector2){4, 10}, (PX_Color4){0xFF, 0x00, 0x00, 0xFF}, engine_font_ui);
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

    // Load Fonts
    engine_font_ui = px_font_load("assets/fonts/psdf/roboto.psdf");
    if (!engine_font_ui) {
        fprintf(stderr, "Error: Failed to load UI font\n");
        px_rs_shutdown_ui();
        px_ws_destroy(&engine_window_main);
        px_ws_shutdown();
        return ERR_COULD_NOT_OPEN_FILE;
    }

    // Render
    bool running = true;
    while (running) {
        px_rs_ui_frame_update();
        engine_core_render();

        last_err = px_ws_poll(&engine_window_main);
        if (last_err != ERR_SUCCESS) {
            fprintf(stderr, "Error: Failed to poll events!\n");
            engine_cleanup();
            return last_err;
        }

        PX_WEvent ev;
        while (px_ws_pop_event(&engine_window_main, &ev)) {
            switch (ev.type) {
                case PX_WE_CLOSE: running = false; break;
                case PX_WE_RESIZE:
                    engine_window_main_w = ev.w;
                    engine_window_main_h = ev.h;
                    engine_window_main.width = ev.w;
                    engine_window_main.height = ev.h;
                    px_rs_ui_resize((PX_Scale2){ev.w, ev.h});
                default: break;
            }
        } 

        px_ws_swap_buffers(&engine_window_main);
    }

    // Cleanup
    engine_cleanup();

    return ERR_SUCCESS;
}
