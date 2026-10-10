#pragma once

#include <event-sys/keycodes.h>
#include <rendering-sys.h>
#include <font.h>

typedef struct PX_Event_TextField {
	PX_Panel panel;

	PX_Font* font;
	float pixel_height;

	const char* placeholder_text; // Placeholder Text is text that shows when no value is present
	PX_Color4 placeholder_color;

	const char* base_text; // Base Text is text that shows in the start
	PX_Color4 base_color;

	char* text;
	PX_Color4 text_color;
	size_t text_size;
	size_t text_cap;

	bool is_typing;
	bool interacted;
	bool fixed_on_screen;

	void* callback_data;
	void (*on_enter)(struct PX_Event_TextField* self, char* text, void* callback_data);
} PX_Event_TextField;

typedef enum {
    EVENT_GSIGNAL_UNKNOWN = 0,
    EVENT_GSIGNAL_CORE_QUIT
} PX_Event_GSignals;

typedef struct {
    PX_Event_GSignals type;
    union {
        bool core_quit;
    };
} PX_Event_GSignal;

typedef enum {
	PX_EVENT_WIDGET_TYPE_UNKNOWN = 0,
	PX_EVENT_WIDGET_TYPE_DROPDOWN,
	PX_EVENT_WIDGET_TYPE_TEXT_FIELD
} PX_Event_WidgetType;

typedef struct {
	PX_Event_WidgetType type;
	bool valid;

	union {
		struct {
			PX_Dropdown* dd;
			bool close_main_panel_too;
		} dropdown;

		struct {
			PX_Event_TextField* tfield;

			PX_AnchorRect lvt;
			PX_Transform2 transform;
		} text_field;
	};
} PX_Event_Widget;


// Include subsystems
#include <event-sys/menu-events.h>
#include <event-sys/scene_context_panel_events.h>

void event_sys_init(PX_Scale2 main_window_scale, PX_Vector2 mouse_position);
void event_sys_deinit(void);
void event_resize(PX_Scale2 main_window_scale);
void event_mouse_move(PX_Vector2 mouse_position);
void event_hover_dropdown(PX_Dropdown* dd);
void event_click_dropdown(PX_Dropdown* dd, bool close_main_panel_too);
void event_text_input(PX_AnchorRect local_viewport, PX_Event_TextField* field, PX_Transform2 transform);
void event_send_gsignal(PX_Event_GSignal* signal);
void event_pop_gsignal(PX_Event_GSignal* out);
void event_handle_gsignals(PX_Event_GSignal* core_signal, bool* core_signal_active);
size_t event_register_widget(PX_Event_Widget* widget); // Returns widget identifier
void event_unregister_widget(size_t widget_identifier);
bool event_key_update(PX_EKeycodes key, bool pressed);
bool is_mouse_on(PX_Transform2 tran);
bool is_mouse_on_anchor(PX_AnchorRect anchor);