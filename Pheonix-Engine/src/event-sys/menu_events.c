#include <stdlib.h>
#include <stdbool.h>

#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>
#include <editor.h>
#include <event-sys/menu-events.h>

static char* save_path = NULL;
static PX_Dropdown* menu_dropdown = NULL;

// FILE Options
static void handle_file_open(void) {

}
static void handle_file_save(void) {

}
static void handle_file_save_as(void) {

}
static void handle_file_quit(void) {
    PX_Event_GSignal signal = {0};
    signal.type = EVENT_GSIGNAL_CORE_QUIT;
    signal.core_quit = true;

    event_send_gsignal(&signal);
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
                        case 1: handle_file_open(); break;
                        case 2: handle_file_save(); break;
                        case 3: handle_file_save_as(); break;
                        case 4: handle_file_quit(); break;
                        default: return;
                    }
                    break;
                case 1:
                case 2:
                case 3:
                default: return;
            }
            break;
        default: return;
    }
}
