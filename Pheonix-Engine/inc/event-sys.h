#pragma once

#include <event-sys/keycodes.h>
#include <rendering-sys.h>
#include <font.h>

typedef enum {
    EVENT_GSIGNAL_UNKNOWN = 0,
    EVENT_GSIGNAL_UI_DROPDOWN_CLICK,
    EVENT_GSIGNAL_UI_SCENE_PANEL_CLICK,
    EVENT_GSIGNAL_CORE_QUIT,
    EVENT_GSIGNAL_3D_HOVER,
	EVENT_GSIGNAL_2D_HOVER
} PX_Event_GSignals;

typedef struct {
    PX_Dropdown* dropdown;
    int opened_index;
    int clicked_option;
} PX_Event_GSignal_UIDropdownClick;

typedef struct {
    PX_3D_Object* obj3d;
	PX_2D_Object* obj2d;
    char* clicked_name;
} PX_Event_GSignal_UIScenePanelClick;

typedef struct {
    uint16_t id;
    uint8_t subId;
    uint8_t objType;
} PX_Event_GSignal_3dHover;
typedef PX_Event_GSignal_3dHover PX_Event_GSignal_2dHover;

typedef struct {
    PX_Event_GSignals type;
    union {
        PX_Event_GSignal_UIDropdownClick ui_dropdown_click;
        PX_Event_GSignal_UIScenePanelClick ui_scenepanel_click;
        bool core_quit;
        PX_Event_GSignal_3dHover mouse_hover_on_3d;
		PX_Event_GSignal_2dHover mouse_hover_on_2d;
    };
} PX_Event_GSignal;

typedef struct {
    void* ptr;
    const char* identifier;
} PX_Event_Identifier;

// Include subsystems
#include <event-sys/menu-events.h>

void event_sys_init(PX_Scale2 main_window_scale, PX_Vector2 mouse_position);
void event_resize(PX_Scale2 main_window_scale);
void event_mouse_move(PX_Vector2 mouse_position);
void event_hover_dropdown(PX_Dropdown* dd);
void event_click_dropdown(PX_Dropdown* dd, bool close_main_panel_too);
void event_send_gsignal(PX_Event_GSignal* signal);
void event_pop_gsignal(PX_Event_GSignal* out);
void event_handle_gsignals(PX_Event_Identifier** identifiers, int identifiers_len, PX_Event_GSignal* core_signal, bool* core_signal_active);
bool is_mouse_on(PX_Transform2 tran);
bool is_mouse_on_anchor(PX_AnchorRect anchor);