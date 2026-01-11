#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <err-codes.h>
#include <window-sys.h>

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

    PX_Window main_win = {
        .width = 100,
        .height = 50,
        .title = "Pheonix Engine",
        .handle = -1
    };

    t_err_codes last_err = ERR_SUCCESS;
    last_err = px_ws_init();
    if (last_err != ERR_SUCCESS)
        return last_err;

    last_err = px_ws_create(&main_win);
    if (last_err != ERR_SUCCESS) {
        px_ws_shutdown();
        return last_err;
    }

    bool running = true;
    while (running) {
        last_err = px_ws_poll(&main_win);
        if (last_err != ERR_SUCCESS) {
            px_ws_destroy(&main_win);
            px_ws_shutdown();
            return last_err;
        }

        PX_WEvent ev;
        while (px_ws_pop_event(&main_win, &ev)) {
            switch (ev.type) {
                case PX_WE_CLOSE: running = false; break;
                case PX_WE_KEYDOWN: printf("Key Down: %d\n", ev.code); break;
                case PX_WE_KEYUP: printf("Key Up: %d\n", ev.code); break;
                case PX_WE_MOUSE_DOWN: printf("Mouse Down: %d at (%d,%d)\n", ev.code, ev.x, ev.y); break;
                case PX_WE_MOUSE_UP: printf("Mouse Up: %d at (%d,%d)\n", ev.code, ev.x, ev.y); break;
                case PX_WE_MOUSE_MOVE: printf("Mouse Move at (%d,%d)\n", ev.x, ev.y); break;
                default: break;
            }
        }
    }

    px_ws_destroy(&main_win);
    px_ws_shutdown();

    return ERR_SUCCESS;
}
