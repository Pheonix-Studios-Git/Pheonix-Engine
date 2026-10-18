#include <pstdlib.h>

#include <pheonix-engine.h>

#include <err-codes.h>
#include <event-sys.h>
#include <window-sys.h>
#include <rendering-sys.h>
#include <font.h>

#include <ctype.h>

#include <event-sys/keycodes.h>

#define MAX_GLOBAL_SIGNALS 64
#define MAX_TEXT_FIELDS 64
#define EVENT_WIDGET_CAP_STEP 32
#define TEXT_INPUT_CAP 128

static PX_Scale2 mwindow_s = {0};
static PX_Vector2 mouse_pos = {0};

static PX_Event_GSignal gsignal_queue[MAX_GLOBAL_SIGNALS];
static int gsignals_count = 0;

static PX_Event_Widget* event_widgets = NULL;
static size_t event_widget_count = 0;
static size_t event_widget_cap = 0;

static PX_Event_TextField* active_text_field = NULL;

static bool is_caps = false;
static bool is_shift = false;

static PX_Transform2 combine_transform2_as_container(PX_Transform2 parent, PX_Transform2 local) {
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

void event_sys_init(PX_Scale2 main_window_scale, PX_Vector2 mouse_position) {
    mwindow_s = main_window_scale;
    mouse_pos = mouse_position;
}

void event_sys_deinit(void) {
    if (event_widgets) {
		size_t widgets_valid = 0;
		for (size_t i = 0; i < event_widget_cap; i++) {
			if (widgets_valid >= event_widget_count) break;

			if (event_widgets[i].valid) {
				event_unregister_widget(i+1);
				widgets_valid++;
			}
		}

		dealloc(event_widgets);
	}

	event_widgets = NULL;
	event_widget_cap = 0;
	event_widget_count = 0;
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
	PX_Transform2 tran = px_util_convert_anchor_to_transform(anchor);

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
	if (node->on_select) node->on_select(node, node->callback_data);

	event_dropdown_close_children(dd->root);
	if (close_main_panel_too) dd->visible = false;
}

void event_click_text_input(PX_AnchorRect local_viewport, PX_Event_TextField* field, PX_Transform2 transform) {
	if (!field) return;

	PX_Transform2 lvt = px_util_convert_anchor_to_transform(local_viewport);
	PX_Transform2 final = combine_transform2_as_container(lvt, transform);

	field->is_typing = is_mouse_on(final);
	if (field->is_typing) {
		field->interacted = true;
		field->text_cursor = field->text_size;
		active_text_field = field;
	}
}

void event_text_input(PX_AnchorRect local_viewport, PX_Event_TextField* field, PX_Transform2 transform) {
	if (!field) return;
	
	PX_Transform2 lvt = px_util_convert_anchor_to_transform(local_viewport);
	px_rs_draw_panel(local_viewport, transform, field->panel, field->fixed_on_screen);

	char* t = (char*)field->placeholder_text;
	PX_Color4 color = field->placeholder_color;

	if (field->text) {
		t = field->text;
		color = field->text_color;
	} else if (field->base_text && !field->interacted) {
		t = (char*)field->base_text;
		color = field->base_color;
	}

	if (t) px_rs_render_text((const char*)t, field->pixel_height, px_util_convert_transform_to_anchor(combine_transform2_as_container(lvt, transform), local_viewport.window), (PX_Vector2){.x=2, .y=2}, color, field->font);
}

void event_send_gsignal(PX_Event_GSignal* signal) {
    if (!signal || signal->type == EVENT_GSIGNAL_UNKNOWN || gsignals_count >= MAX_GLOBAL_SIGNALS) return;
    copybuf(&gsignal_queue[gsignals_count], signal, sizeof(PX_Event_GSignal));
    gsignals_count++;
}

void event_pop_gsignal(PX_Event_GSignal* out) {
    if (gsignals_count < 1 || gsignals_count > MAX_GLOBAL_SIGNALS) {
        PX_Event_GSignal zeroed = {0};
        copybuf(out, &zeroed, sizeof(PX_Event_GSignal));
        return;
    }
    gsignals_count--;
    copybuf(out, &gsignal_queue[gsignals_count], sizeof(PX_Event_GSignal));
}

void event_handle_gsignals(PX_Event_GSignal* core_signal, bool* core_signal_active) {
    PX_Event_GSignal sig = {0};
    PX_Event_GSignal* s = &sig;
    event_pop_gsignal(s);

    if (s->type == EVENT_GSIGNAL_UNKNOWN) return;

    switch (s->type) {
        case EVENT_GSIGNAL_CORE_QUIT: {
            copybuf(core_signal, s, sizeof(PX_Event_GSignal));
            *core_signal_active = true;
            return;
		}

        default: break;
    }

    *core_signal_active = false;
}

static int64_t event_find_free_widget_slot(void) {
	if (!event_widgets) return -1;
	if (event_widget_count < 1) return 0;

	for (size_t i = 0; i < event_widget_cap; i++) {
		if (!event_widgets[i].valid) return i;
	}

	return -1;
}

size_t event_register_widget(PX_Event_Widget* widget) { // Returns Identifier ID of Widget (The Widget's index + 1)
	if (!widget) return 0;

	if (!event_widgets) {
		// Allocate
		event_widgets = (PX_Event_Widget*)zalloc(EVENT_WIDGET_CAP_STEP * sizeof(PX_Event_Widget));
		if (!event_widgets) return 0;

		event_widget_cap = EVENT_WIDGET_CAP_STEP;
	} else if (event_widget_count + 1 > event_widget_cap) {
		// Realloc
		PX_Event_Widget* nptr = (PX_Event_Widget*)rzalloc(event_widgets, sizeof(PX_Event_Widget)*(event_widget_cap+EVENT_WIDGET_CAP_STEP));
		if (!nptr) return 0;

		event_widget_cap += EVENT_WIDGET_CAP_STEP;
		event_widgets = nptr;
	}

	widget->valid = true;

	int64_t true_iden = event_find_free_widget_slot();
	if (true_iden < 0) return 0; // No free slot

	size_t iden = (size_t)true_iden;
	event_widgets[iden] = *widget;
	
	event_widget_count++;

	return iden+1;
}

void event_unregister_widget(size_t widget_identifier) {
	if (widget_identifier < 1 || widget_identifier > event_widget_count) return;

	PX_Event_Widget* widget = &event_widgets[widget_identifier-1];
	if (!widget->valid) return;

	switch (widget->type) {
		case PX_EVENT_WIDGET_TYPE_TEXT_FIELD: {
			// Release all event side allocations
			if (!widget->text_field.tfield) goto remove;

			if (widget->text_field.tfield->text) dealloc(widget->text_field.tfield->text);

			widget->text_field.tfield->text = NULL;
			widget->text_field.tfield->text_cap = 0;
			widget->text_field.tfield->text_size = 0;

			break;
		}

		default: break;
	}

	remove: {
		widget->valid = false;
		event_widget_count--;
	}
}

static bool handle_active_text_input_key_press(PX_EKeycodes key, bool pressed) {
	if (!pressed || !active_text_field) return false;

	// IS Key Printable ASCII?
	char c = px_util_ekeycode_to_char(key, is_caps, is_shift);
	if (c != '\0') {
		switch (active_text_field->type) {
			case PX_EVENT_TEXTFIELD_TYPE_NORMAL: break;
			case PX_EVENT_TEXTFIELD_TYPE_NUMERIC: { if (!isdigit(c)) { return true; } break; }
			case PX_EVENT_TEXTFIELD_TYPE_ALPHA: { if (!isalpha(c)) { return true; } break; }
			case PX_EVENT_TEXTFIELD_TYPE_FLOATING: {
				if (!isdigit(c) && c == '.') {
					if (active_text_field->floating_point_set) return true;
					active_text_field->floating_point_set = true;
					break;
				} else if (!isdigit(c)) return true;

				break;
			}
			
			default: break; // Acts as normal type
		}

		if (!active_text_field->text) {
			active_text_field->text = (char*)zalloc(TEXT_INPUT_CAP);
			if (!active_text_field->text) goto text_field_alloc_failed;

			active_text_field->text_cap = TEXT_INPUT_CAP;
			active_text_field->text_size = 0;
			active_text_field->text_cursor = 0;
		} else if (active_text_field->text_size + 2 > active_text_field->text_cap) {
			char* nptr = (char*)rzalloc(active_text_field->text, active_text_field->text_cap + TEXT_INPUT_CAP);
			if (!nptr) goto text_field_alloc_failed;

			active_text_field->text = nptr;
			active_text_field->text_cap += TEXT_INPUT_CAP;
		}

		if (active_text_field->text_cursor > active_text_field->text_size)
			active_text_field->text_cursor = active_text_field->text_size;

		if (active_text_field->text_cursor != active_text_field->text_size) {
			movebuf(
				active_text_field->text + active_text_field->text_cursor + 1,
				active_text_field->text + active_text_field->text_cursor,
				active_text_field->text_size - active_text_field->text_cursor + 1
			);
		}
		
		active_text_field->text[active_text_field->text_cursor++] = c;
		active_text_field->text[++active_text_field->text_size] = '\0';

		text_field_alloc_failed: {
			return true; // Later cases (in event_key_update) don't require printable, so skip them entirely
		}
	} else {
		switch (key) {
			case EKeycode_Enter: {
				if (active_text_field->on_enter) active_text_field->on_enter(active_text_field, active_text_field->text, active_text_field->callback_data);
				active_text_field->is_typing = false;
				active_text_field = NULL;
				return false;
			}
			
			case EKeycode_Backspace: {
				if (!active_text_field->text || active_text_field->text_cursor < 1 || active_text_field->text_size < 1) break;

				if (active_text_field->text_cursor == active_text_field->text_size) {
					active_text_field->text[--active_text_field->text_cursor] = '\0';
				} else {
					movebuf(active_text_field->text + active_text_field->text_cursor - 1, active_text_field->text + active_text_field->text_cursor, active_text_field->text_size - active_text_field->text_cursor + 1);
					active_text_field->text_cursor--;
				}

				active_text_field->text_size--;
				break;
			}
			case EKeycode_Delete: {
				if (!active_text_field->text || active_text_field->text_cursor >= active_text_field->text_size || active_text_field->text_cursor >= active_text_field->text_cap) break;
				
				if (active_text_field->text_cursor == active_text_field->text_size-1) {
					active_text_field->text[active_text_field->text_cursor] = '\0';
				} else {
					movebuf(active_text_field->text + active_text_field->text_cursor, active_text_field->text + active_text_field->text_cursor + 1, active_text_field->text_size + 2 - active_text_field->text_cursor);
				}
				
				active_text_field->text_size--;
				break;
			}

			case EKeycode_LeftArrow: {
				if (!active_text_field->text || active_text_field->text_cursor == 0) break;

				active_text_field->text_cursor--;
				break;
			}
			case EKeycode_RightArrow: {
				if (!active_text_field->text || active_text_field->text_cursor == active_text_field->text_size) break;

				active_text_field->text_cursor++;
				break;
			}

			case EKeycode_Home: {
				if (!active_text_field->text || active_text_field->text_cursor == 0) break;
				active_text_field->text_cursor = 0;
				break;
			}
			case EKeycode_End: {
				if (!active_text_field->text || active_text_field->text_cursor == active_text_field->text_size) break;
				active_text_field->text_cursor = active_text_field->text_size;
				break;
			}

			default: break;
		}
	}

	return true;
}

bool event_key_update(PX_EKeycodes key, bool pressed) { // Returns a bool, which when true should block all later key processesing
	switch (key) {
		case EKeycode_RShift:
		case EKeycode_LShift: {
			is_shift = pressed;
			return false;
		}

		default: break;
	}

	if (!pressed) return false;

	switch (key) {
		case EKeycode_Capslock: {
			is_caps = !is_caps;
			return false;
		}
		default: break;
	}

	if (active_text_field) {
		return handle_active_text_input_key_press(key, pressed);
	}

	if (!event_widgets) return false;

	size_t widgets_valid = 0;
	for (size_t i = 0; i < event_widget_cap; i++) {
		if (widgets_valid >= event_widget_count) break;

		PX_Event_Widget* widget = &event_widgets[i];
		if (!widget->valid) continue;
		widgets_valid++;

		switch (widget->type) {
			case PX_EVENT_WIDGET_TYPE_DROPDOWN: {
				if (key == EKeycode_MouseLButton) event_click_dropdown(widget->dropdown.dd, widget->dropdown.close_main_panel_too);
				else event_hover_dropdown(widget->dropdown.dd);
				break;
			}

			case PX_EVENT_WIDGET_TYPE_TEXT_FIELD: {
				if (key == EKeycode_MouseLButton) event_click_text_input(widget->text_field.lvt, widget->text_field.tfield, widget->text_field.transform);
				break;
			}

			default: break;
		}
	}

	return false;
}
