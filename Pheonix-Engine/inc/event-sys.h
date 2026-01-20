#pragma once

#include <rendering-sys.h>

typedef enum {
    EKeycode_Unknown = 0,
    EKeycode_MouseLButton,
    EKeycode_MouseRButton,
    EKeycode_MouseMButton,
    EKeycode_MouseScrollUp,
    EKeycode_MouseScrollDown,
    EKeycode_MouseScrollLeft,
    EKeycode_MouseScrollRight,
    EKeycode_MouseX1,
    EKeycode_MouseX2,
    EKeycode_0,
    EKeycode_1,
    EKeycode_2,
    EKeycode_3,
    EKeycode_4,
    EKeycode_5,
    EKeycode_6,
    EKeycode_7,
    EKeycode_8,
    EKeycode_9,
    EKeycode_Q,
    EKeycode_W,
    EKeycode_E,
    EKeycode_R,
    EKeycode_T,
    EKeycode_Y,
    EKeycode_U,
    EKeycode_I,
    EKeycode_O,
    EKeycode_P,
    EKeycode_A,
    EKeycode_S,
    EKeycode_D,
    EKeycode_F,
    EKeycode_G,
    EKeycode_H,
    EKeycode_J,
    EKeycode_K,
    EKeycode_L,
    EKeycode_Z,
    EKeycode_X,
    EKeycode_C,
    EKeycode_V,
    EKeycode_B,
    EKeycode_N,
    EKeycode_M,
    EKeycode_Tab,
    EKeycode_Capslock,
    EKeycode_LShift,
    EKeycode_RShift,
    EKeycode_LControl,
    EKeycode_RControl,
    EKeycode_Super,
    EKeycode_LAlternate,
    EKeycode_RAlternate,
    EKeycode_Space,
    EKeycode_Function,
    EKeycode_Enter,
    EKeycode_Backspace,
    EKeycode_Insert,
    EKeycode_Home,
    EKeycode_PageUp,
    EKeycode_PageDown,
    EKeycode_Delete,
    EKeycode_End,
    EKeycode_Numlock,
    EKeycode_PrintScreen,
    EKeycode_ScrollLock,
    EKeycode_PauseBreak,
    EKeycode_Escape,
    EKeycode_UpArrow,
    EKeycode_DownArrow,
    EKeycode_RightArrow,
    EKeycode_LeftArrow,
    EKeycode_NumSlash,
    EKeycode_NumAsterik,
    EKeycode_NumDash,
    EKeycode_NumPlus,
    EKeycode_NumEnter,
    EKeycode_Num7,
    EKeycode_Num8,
    EKeycode_Num9,
    EKeycode_Num4,
    EKeycode_Num5,
    EKeycode_Num6,
    EKeycode_Num1,
    EKeycode_Num2,
    EKeycode_Num3,
    EKeycode_Num0,
    EKeycode_NumPoint,
    EKeycode_F1,
    EKeycode_F2,
    EKeycode_F3,
    EKeycode_F4,
    EKeycode_F5,
    EKeycode_F6,
    EKeycode_F7,
    EKeycode_F8,
    EKeycode_F9,
    EKeycode_F10,
    EKeycode_F11,
    EKeycode_F12,
    EKeycode_Backtick,
    EKeycode_Dash,
    EKeycode_Equals,
    EKeycode_SqBracketOpen,
    EKeycode_SqBracketClose,
    EKeycode_Backslash,
    EKeycode_Semicolon,
    EKeycode_SingleQuotes,
    EKeycode_Comma,
    EKeycode_Fullstop,
    EKeycode_Slash
} PX_EKeycodes;

typedef enum {
    EVENT_GSIGNAL_UNKNOWN = 0,
    EVENT_GSIGNAL_UI_DROPDOWN_CLICK
} PX_Event_GSignals;

typedef struct {
    int opened_index;
    int clicked_option;
} PX_Event_GSignal_UIDropdownClick;

typedef struct {
    union {
        PX_Event_GSignal_UIDropdownClick ui_dropdown_click;
    }
} PX_Event_GSignal;

void event_sys_init(PX_Scale2 main_window_scale, PX_Vector2 mouse_position);
void event_resize(PX_Scale2 main_window_scale);
void event_mouse_move(PX_Vector2 mouse_position);
void event_hover_dropdown(PX_Dropdown* dd);
void event_click_dropdown(PX_Dropdown* dd);
void event_send_gsignal(PX_Event_GSignals type, PX_Event_GSignal* signal);
PX_Event_GSignal* event_pop_gsignal(void);
