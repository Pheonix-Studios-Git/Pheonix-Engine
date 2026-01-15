#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <err-codes.h>
#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>

typedef struct {
    bool valid;
    bool help;
} t_args;

static void print_help(void) {
    printf("Usage: pheonix-engine [--COMMANDS]\n");
    printf("Commands:\n");
    printf("\thelp: Prints this help message\n");
}

static void parse_args(t_args* args, int argc, char** argv) {
    args->valid = true;
    args->help = false;
    
    for (int i = 1; i < argc; i++) {
        char* opt = argv[i];

        if (strcmp(opt, "--help") == 0) {
            args->help = true;
        } else {
            printf("Usage: pheonix-engine [--COMMANDS]\n\tUse --help for more info!\n");
            args->valid = false;
            break;
        }
    }
}

int main(int argc, char** argv) {
    t_args passed_args = {0};
    parse_args(&passed_args, argc, argv);

    if (!passed_args.valid) {
        return ERR_USAGE;
    }

    if (passed_args.help) {
        print_help();
        return ERR_SUCCESS;
    }

    int main_win_w = 100;
    int main_win_h = 50;

    PX_Window main_win = {
        .width = main_win_w,
        .height = main_win_h,
        .title = "Pheonix Engine",
        .handle = -1
    };

    PX_WindowDesign main_win_design = {0};
    main_win_design.bg_color = 0xFFFFFF;
    main_win_design.fg_color = 0x000000;

    t_err_codes last_err = ERR_SUCCESS;
    last_err = px_ws_init();
    if (last_err != ERR_SUCCESS)
        return last_err;

    last_err = px_ws_create(&main_win);
    if (last_err != ERR_SUCCESS) {
        px_ws_shutdown();
        return last_err;
    }

    last_err = px_ws_show_splash(&main_win);
    if (last_err != ERR_SUCCESS) {
        px_ws_destroy(&main_win);
        px_ws_shutdown();
        return last_err;
    }

    px_ws_window_design(&main_win, &main_win_design);
    
    px_ws_create_ctx(&main_win);
    last_err = px_rs_init_ui((PX_Scale2){main_win_w, main_win_h});
    if (last_err != ERR_SUCCESS) {
        px_ws_destroy(&main_win);
        px_ws_shutdown();
        return last_err;
    }

    bool running = true;
    while (running) {
        px_rs_ui_frame_update();
        px_rs_draw_panel((PX_Scale2){main_win_w, 10}, (PX_Vector2){0, 0}, (PX_Color4){0x2B, 0x2B, 0x2B, 0xFF});

        last_err = px_ws_poll(&main_win);
        if (last_err != ERR_SUCCESS) {
            px_rs_shutdown_ui();
            px_ws_destroy(&main_win);
            px_ws_shutdown();
            return last_err;
        }

        PX_WEvent ev;
        while (px_ws_pop_event(&main_win, &ev)) {
            switch (ev.type) {
                case PX_WE_CLOSE: running = false; break;
                case PX_WE_RESIZE:
                    main_win_w = ev.w;
                    main_win_h = ev.h;
                    px_rs_ui_resize((PX_Scale2){ev.w, ev.h});
                default: break;
            }
        } 

        px_ws_swap_buffers(&main_win);
    }

    px_rs_shutdown_ui();
    px_ws_destroy(&main_win);
    px_ws_shutdown();

    return ERR_SUCCESS;
}
