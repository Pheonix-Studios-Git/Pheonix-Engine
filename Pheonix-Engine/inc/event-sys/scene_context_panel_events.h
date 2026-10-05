#pragma once

#include <rendering-sys.h>
#include <font.h>
#include <event-sys.h>

void scene_context_panel_evs_init(void);
void scene_context_panel_evs_handle_events(PX_DropdownNode* node, void* identifier);
