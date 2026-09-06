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

static void event_dropdown_close_children(PX_DropdownNode* node) {
	if (!node || !node->children) return;

	for (size_t i = 0; i < node->children_count; i++) {
		PX_DropdownNode* child = node->children[i];
		if (!child) continue;

		child->open = false;
		child->hovered = false;

		event_dropdown_close_children(child);
	}
}

static void event_dropdown_close_siblings(PX_DropdownNode* node) {
	if (!node || !node->parent) return;
	PX_DropdownNode* parent = node->parent;
	if (!parent->children) return;

	for (size_t i = 0; i < parent->children_count; i++) {
		PX_DropdownNode* sibling = parent->children[i];
		if (!sibling || sibling == node) continue;

		sibling->open = false;
		sibling->hovered = false;

		event_dropdown_close_children(sibling);
	}
}

static PX_DropdownNode* event_dropdown_find_hovered(PX_DropdownNode* node) {
	if (!node || !node->children) return NULL;

	for (size_t i = 0; i < node->children_count; i++) {
		PX_DropdownNode* child = node->children[i];
		if (!child) continue;

		if (is_mouse_on(child->rendered_transform)) {
			if (child->open && child->children && child->children_count > 0) {
				PX_DropdownNode* hovered = event_dropdown_find_hovered(child);
				if (hovered) return hovered;
			}
			return child;
		}

		if (child->open && child->children && child->children_count > 0) {
			PX_DropdownNode* hovered = event_dropdown_find_hovered(child);
			if (hovered) return hovered;
		}
	}

	return NULL;
}

static void event_dropdown_clear_hover_node(PX_DropdownNode* node) {
	if (!node) return;
	node->hovered = false;

	if (!node->children) return;
	for (size_t i = 0; i < node->children_count; i++) event_dropdown_clear_hover_node(node->children[i]);
}

void event_hover_dropdown(PX_Dropdown* dd) {
	if (!dd) return;
	if (!dd->visible || !dd->root) return;
	if (!dd->root->children || dd->root->children_count == 0) return;

	event_dropdown_clear_hover_node(dd->root);
	PX_DropdownNode* hovered = event_dropdown_find_hovered(dd->root);
	if (!hovered) return;
	hovered->hovered = true;
}

void event_click_dropdown(PX_Dropdown* dd, bool close_main_panel_too) {
	if (!dd) return;
	if (!dd->visible || !dd->root) return;
	if (!dd->root->children || dd->root->children_count == 0) return;

	PX_DropdownNode* node = event_dropdown_find_hovered(dd->root);
	if (!node) {
		if (close_main_panel_too) dd->visible = false;
		return;
	}

	if (node->children && node->children_count > 0) {
		if (node->open) {
			node->open = false;
			event_dropdown_close_children(node);
		} else {
			event_dropdown_close_siblings(node);
			node->open = true;
		}
		return;
	}
	if (node->on_select) node->on_select(node, node->user_data);
	else {
		// Incase silent event is not used, send global wide event
		PX_Event_GSignal gsignal = {
			.type = EVENT_GSIGNAL_UI_DROPDOWN_CLICK,
			.ui_dropdown_click = (PX_Event_GSignal_UIDropdownClick){
				.dropdown = dd,
				.clicked_node = node
			}
		};
		event_send_gsignal(&gsignal);
	}

	event_dropdown_close_children(dd->root);
	if (close_main_panel_too) dd->visible = false;
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
            } else if (strcmp(iden, "scene-panel-context-menu") == 0) {
                scene_context_panel_evs_handle_events(s);
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
