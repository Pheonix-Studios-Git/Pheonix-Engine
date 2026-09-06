#pragma once

#include <rendering-sys.h>
#include <font.h>
#include <event-sys.h>

void scene_context_panel_evs_init(PX_Dropdown* scene_context_panel_dd);
void scene_context_panel_evs_handle_events(PX_Event_GSignal* signal);
