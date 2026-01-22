#pragma once

#include <rendering-sys.h>
#include <event-sys.h>

void menu_evs_init(PX_Dropdown* menu_dd, char* path_to_save);
void menu_evs_handle_events(PX_Event_GSignal* signal);
