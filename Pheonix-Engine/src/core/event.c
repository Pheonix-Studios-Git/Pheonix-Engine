#include <stdbool.h>
#include <string.h>

#include <pheonix-engine.h>

#include <err-codes.h>
#include <event-sys.h>
#include <window-sys.h>
#include <rendering-sys.h>
#include <font.h>

#define MAX_GLOBAL_SIGNALS 64

static PX_Scale2 mwindow_s = {0};
static PX_Vector2 mouse_pos = {0};
static PX_Event_GSignal gsignal_queue[MAX_GLOBAL_SIGNALS];
static int gsignals_count = 0;

static PX_Transform2 combine_transform2(PX_Transform2 parent, PX_Transform2 local) {
    PX_Transform2 out = {0};

    out.scale.w = local.scale.w;
    out.scale.h = local.scale.h;
	out.rot = parent.rot + local.rot;

	float c = cosf(parent.rot);
    float s = sinf(parent.rot);

	float rotated_x = local.pos.x * c - local.pos.y * s;
    float rotated_y = local.pos.x * s + local.pos.y * c;

    out.pos.x = parent.pos.x + rotated_x;
    out.pos.y = parent.pos.y + rotated_y;
    return out;
}

static PX_Transform2 dropdown_transform(PX_Dropdown* dd) {
    return (PX_Transform2){dd->pos, (PX_Scale2){dd->width, dd->height}, 0.0f};
}

static PX_Transform2 dropdown_item_transform(PX_Dropdown* dd, PX_DropdownItem* item, int x) {
    PX_Transform2 dd_tran = dropdown_transform(dd);
    PX_Transform2 item_tran = {(PX_Vector2){x, dd->stext_pos.y}, (PX_Scale2){item->width, item->height }, 0.0f};
    return combine_transform2(dd_tran, item_tran);
}

static PX_Transform2 dropdown_panel_transform(PX_Dropdown* dd, PX_DropdownItem* item, int x) {
    PX_Transform2 dd_tran = dropdown_transform(dd);

    PX_Transform2 item_tran = {(PX_Vector2){x, dd->stext_pos.y}, (PX_Scale2){item->width, item->height}, 0.0f};
    PX_Transform2 panel_tran = combine_transform2(item_tran, item->panel_tran);

    return combine_transform2(dd_tran, panel_tran);
}

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

bool is_mouse_on(PX_Transform2 tran) {
    PX_Vector2 pos = tran.pos;
    PX_Scale2 scale = tran.scale;
    return (bool)(
        mouse_pos.x >= pos.x &&
        mouse_pos.x <= pos.x + scale.w &&
        mouse_pos.y >= pos.y &&
        mouse_pos.y <= pos.y + scale.h
    );
}

bool is_mouse_on_anchor(PX_AnchorRect anchor) {
	PX_Transform2 tran = enginef_convert_anchor_to_transform(anchor);

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
	dd->hover_index = -1;

    PX_Transform2 dd_tran = dropdown_transform(dd);
    bool mouse_on_dropdown = is_mouse_on(dd_tran);
    if (!mouse_on_dropdown && open_index == -1) return;

    if (mouse_on_dropdown) {
        // It is on dropdown
        int x = dd->stext_pos.x;
        for (int i = 0; i < dd->item_count; i++) {
            PX_DropdownItem* item = &dd->items[i];

            PX_Transform2 tran = dropdown_item_transform(dd, item, x);
            if (is_mouse_on(tran)) {
                dd->hover_index = i;
                return;
            }

            x += item->width + dd->spacing;
        }
    } else {
        // It is on a panel
        PX_DropdownItem* item = &dd->items[open_index];

		int x = dd->stext_pos.x;
		for (int i = 0; i < open_index; i++) {
			x += dd->items[i].width + dd->spacing;
		}

		PX_Transform2 panel_tran = dropdown_panel_transform(dd, item, x);
		int y = item->stext_pos.y;

		for (int i = 0; i < item->option_count; i++) {
			PX_DropdownOption* option = &item->options[i];
			PX_Transform2 option_tran = {
				(PX_Vector2){panel_tran.pos.x + item->stext_pos.x, panel_tran.pos.y + y},
				(PX_Scale2){option->width, option->height},
				0.0f
			};

			if (is_mouse_on(option_tran)) {
				item->hover_index = i;
				return;
			}

			y += item->spacing;
		}
    }
}

void event_click_dropdown(PX_Dropdown* dd, bool close_main_panel_too) {
    int open_index = -1;
    for (int i = 0; i < dd->item_count; i++) {
        if ((&dd->items[i])->is_open) {
            open_index = i;
            break;
        }
    }

    if (open_index > -1) {
		PX_DropdownItem* item = &dd->items[open_index];
		int x = dd->stext_pos.x;
		for (int i = 0; i < open_index; i++) {
			x += dd->items[i].width + dd->spacing;
		}

		PX_Transform2 panel_tran = dropdown_panel_transform(dd, item, x);
		int y = item->stext_pos.y;

		for (int i = 0; i < item->option_count; i++) {
			PX_DropdownOption* option = &item->options[i];
			PX_Transform2 option_tran = {
				(PX_Vector2){panel_tran.pos.x + item->stext_pos.x, panel_tran.pos.y + y},
				(PX_Scale2){option->width, option->height},
				0.0f
			};

			if (is_mouse_on(option_tran)) {
				PX_Event_GSignal signal = {0};
                signal.type = EVENT_GSIGNAL_UI_DROPDOWN_CLICK;
                signal.ui_dropdown_click = (PX_Event_GSignal_UIDropdownClick){dd, open_index, i};
                event_send_gsignal(&signal);
                item->is_open = false;
				return;
			}

			y += item->spacing;
		}
    }

	PX_Transform2 dd_tran = dropdown_transform(dd);
    bool mouse_on_dropdown = is_mouse_on(dd_tran);
    if (!mouse_on_dropdown && open_index == -1) {
		if (close_main_panel_too) dd->visible = false;
		return;
	}

    if (mouse_on_dropdown) {
        // It is on dropdown
        int x = dd->stext_pos.x;
        for (int i = 0; i < dd->item_count; i++) {
            PX_DropdownItem* item = &dd->items[i];
            PX_Transform2 tran = dropdown_item_transform(dd, item, x);

            if (is_mouse_on(tran) && open_index != i) {
                item->is_open = true;
                (&dd->items[open_index])->is_open = false;
                open_index = -1;
            } else {
                item->is_open = false;
            }

            x += dd->spacing + item->width;
        }
    }

    if (open_index > -1) (&dd->items[open_index])->is_open = false;
}

void event_send_gsignal(PX_Event_GSignal* signal) {
    if (!signal || signal->type == EVENT_GSIGNAL_UNKNOWN || gsignals_count >= MAX_GLOBAL_SIGNALS) return;
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

static char* get_identifier(PX_Event_Identifier** identifiers, int size, void* ptr) {
    for (int i = 0; i < size; i++) {
        if (identifiers[i]->ptr == ptr) {
            return (char*)identifiers[i]->identifier;
        }
    }
    return NULL;
}

void event_handle_gsignals(PX_Event_Identifier** identifiers, int identifiers_len, PX_Event_GSignal* core_signal, bool* core_signal_active) {
    PX_Event_GSignal sig = {0};
    PX_Event_GSignal* s = &sig;
    event_pop_gsignal(s);
    if (s->type == EVENT_GSIGNAL_UNKNOWN) return;

    char* iden = NULL;
    switch (s->type) {
        case EVENT_GSIGNAL_UI_DROPDOWN_CLICK:
            iden = get_identifier(identifiers, identifiers_len, (void*)s->ui_dropdown_click.dropdown);
            if (!iden) break;

            if (strcmp(iden, "menubar") == 0) {
                menu_evs_handle_events(s);
            }
            break;
        case EVENT_GSIGNAL_CORE_QUIT:
            memcpy(core_signal, s, sizeof(PX_Event_GSignal));
            *core_signal_active = true;
            return;
        default: break;
    }

    *core_signal_active = false;
}
