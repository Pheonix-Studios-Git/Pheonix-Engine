#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <err-codes.h>
#include <event-sys.h>
#include <window-sys.h>
#include <rendering-sys.h>

#define MAX_GLOBAL_SIGNALS 64

static PX_Scale2 mwindow_s = {0};
static PX_Vector2 mouse_pos = {0};
static PX_Event_GSignal gsignal_queue[MAX_GLOBAL_SIGNALS];
static int gsignals_count = 0;

void event_sys_init(PX_Scale2 main_window_scale, PX_Vector2 mouse_position) {
    mwindow_s = main_window_scale;
    mouse_pos = mouse_position;
}

void event_resize(PX_Scale2 main_window_scale) {
    mwindow_s = main_window_scale;
}

void event_mouse_move(PX_Vector2 mouse_position) {
    mouse_pos = mouse_position;
}

static bool is_mouse_on(PX_Transform2 tran) {
    PX_Vector2 pos = tran.pos;
    PX_Scale2 scale = tran.scale;
    return (bool)(
        mouse_pos.x >= pos.x &&
        mouse_pos.x <= pos.x + scale.w &&
        mouse_pos.y >= pos.y &&
        mouse_pos.y <= pos.y + scale.h
    );
}

void event_hover_dropdown(PX_Dropdown* dd) {
    int open_index = -1;
    for (int i = 0; i < dd->item_count; i++) {
        (&dd->items[i])->hover_index = -1;
        if ((&dd->items[i])->is_open) {
            open_index = i;
        }
    }

    PX_Scale2 dd_scale = (PX_Scale2){dd->width, dd->height};
    PX_Transform2 dd_tran = (PX_Transform2){dd->pos, dd_scale};
    dd->hover_index = -1;

    if (!is_mouse_on(dd_tran)  && open_index == -1)
        return;

    if (is_mouse_on(dd_tran)) {
        // It is on dropdown
        int x = dd->stext_pos.x;
        for (int i = 0; i < dd->item_count; i++) {
            PX_DropdownItem* item = &dd->items[i];

            PX_Transform2 tran = (PX_Transform2){(PX_Vector2){x, dd->stext_pos.y}, (PX_Scale2){item->width, item->height}};
            if (is_mouse_on(tran)) {
                dd->hover_index = i;
                return;
            }

            x += dd->spacing + px_rs_text_width(dd->font, item->label, dd->font_size);
        }
    } else {
        // It is on a panel
        PX_DropdownItem* opened_panel = &dd->items[open_index];
        int y = opened_panel->stext_pos.y;
        for (int i = 0; i < opened_panel->option_count; i++) {
            PX_DropdownOption* op = &opened_panel->options[i];

            PX_Transform2 tran = (PX_Transform2){(PX_Vector2){opened_panel->stext_pos.x, y}, (PX_Scale2){op->width, op->height}};
            if (is_mouse_on(tran)) {
                opened_panel->hover_index = i;
            }
            y += opened_panel->spacing;
        }
    }
}

void event_click_dropdown(PX_Dropdown* dd) {
    int open_index = -1;
    for (int i = 0; i < dd->item_count; i++) {
        if ((&dd->items[i])->is_open) {
            open_index = i;
            break;
        }
    }

    if (open_index > -1) {
        PX_DropdownItem* opened_panel = &dd->items[open_index];
        int y = opened_panel->stext_pos.y;

        for (int j = 0; j < opened_panel->option_count; j++) {
            PX_DropdownOption* op = &opened_panel->options[j];
            PX_Transform2 op_tran = (PX_Transform2){(PX_Vector2){opened_panel->stext_pos.x, y}, (PX_Scale2){op->width, op->height}};
            if (is_mouse_on(op_tran)) {
                PX_Event_GSignal signal = {0};
                signal.ui_dropdown_click = (PX_Event_GSignal_UIDropdownClick){open_index, j};
                event_send_gsignal(EVENT_GSIGNAL_UI_DROPDOWN_CLICK, &signal);
                opened_panel->is_open = false;
                return;
            }
            y += opened_panel->spacing;
        }
    }

    PX_Scale2 dd_scale = (PX_Scale2){dd->width, dd->height};
    PX_Transform2 dd_tran = (PX_Transform2){dd->pos, dd_scale};

    if (!is_mouse_on(dd_tran) && open_index == -1)
        return;

    if (is_mouse_on(dd_tran)) {
        // It is on dropdown
        int x = dd->stext_pos.x;
        for (int i = 0; i < dd->item_count; i++) {
            PX_DropdownItem* item = &dd->items[i];

            PX_Transform2 tran = (PX_Transform2){(PX_Vector2){x, dd->stext_pos.y}, (PX_Scale2){item->width, item->height}};
            if (is_mouse_on(tran) && open_index != i) {
                item->is_open = true;
                (&dd->items[open_index])->is_open = false;
                open_index = -1;
            } else {
                item->is_open = false;
            }

            x += dd->spacing + px_rs_text_width(dd->font, item->label, dd->font_size);
        }
    }

    if (open_index > -1) (&dd->items[open_index])->is_open = false;
}

void event_send_gsignal(PX_Event_GSignals type, PX_Event_GSignal* signal) {
    if (!signal || type == EVENT_GSIGNAL_UNKNOWN || gsignals_count >= MAX_GLOBAL_SIGNALS) return;
    memcpy(&gsignal_queue[gsignals_count], signal, sizeof(PX_Event_GSignal));
    gsignals_count++;
}

void event_pop_gsignal(PX_Event_GSignal* out) {
    if (gsignals_count < 1 || gsignals_count > MAX_GLOBAL_SIGNALS) {
        PX_Event_GSignal zeroed = {0};
        memcpy(out, &zeroed, sizeof(PX_Event_GSignal));
        return;
    }
    gsignals_count--;
    memcpy(out, &gsignal_queue[gsignals_count], sizeof(PX_Event_GSignal));
}

