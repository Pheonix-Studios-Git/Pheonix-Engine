#include <stdbool.h>
#include <string.h>

#include <pheonix-engine.h>
#include <window-sys.h>
#include <event-sys.h>
#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/loader.h>
#include <editor.h>
#include <event-sys/scene_context_panel_events.h>

#include <editor.h>

static const PX_Color4 white_color = (const PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
static PX_Dropdown* scene_context_panel_dropdown = NULL;

static void append_to_scene_2d(PX_2D_Object* obj) {
	if (!obj) return;
	if (engine_2drenderer_main_scene.object_count >= PX_RS_MAX_OBJECTS_PER_SCENE) return;

	engine_2drenderer_main_scene.objects[engine_2drenderer_main_scene.object_count++] = *obj;
}

// FILE Options
static void handle_create_2d_panel(void) {
	if (engine_2drenderer_main_scene.object_count >= PX_RS_MAX_OBJECTS_PER_SCENE) return;

	PX_Panel* panel = (PX_Panel*)malloc(sizeof(PX_Panel));
	if (!panel) return;
	
	*panel = (PX_Panel){
		.color = white_color,
		.blur = 0.0f,
		.cradius = 0.0f,
		.noise = 0.0f
	};

	// Connect Properties
	PX_Property* p_color = px_util_property_add("Color", PX_RS_PROPERTY_COLOR4, NULL);
	if (!p_color) {free(panel); return;}
	p_color->data = &panel->color; p_color->size = sizeof(PX_Color4);

	PX_Property* p_blur = px_util_property_add("Blur", PX_RS_PROPERTY_FLOAT, p_color);
	if (!p_blur) {px_util_destroy_properties(p_color, NULL); free(panel); return;}
	p_blur->data = &panel->blur; p_blur->size = sizeof(float);
	p_blur->min = 0.0f; p_blur->max = 1.0f; p_blur->step = 0.05f;

	PX_Property* p_cradius = px_util_property_add("Corner Radius", PX_RS_PROPERTY_FLOAT, p_blur);
	if (!p_cradius) {px_util_destroy_properties(p_color, NULL); free(panel); return;}
	p_cradius->data = &panel->cradius; p_cradius->size = sizeof(float);
	p_cradius->min = 0.0f; p_cradius->max = 100.0f; p_cradius->step = 0.1f;

	PX_Property* p_noise = px_util_property_add("Noise", PX_RS_PROPERTY_FLOAT, p_cradius);
	if (!p_noise) {px_util_destroy_properties(p_color, NULL); free(panel); return;}
	p_noise->data = &panel->noise; p_noise->size = sizeof(float);
	p_noise->min = 0.0f; p_noise->max = 1.0f; p_noise->step = 0.05f;

	PX_2D_Object new_panel = (PX_2D_Object){
		.name = "Untitled Panel",
		.type = PX_RS_OBJECT_2D_TYPE_PANEL,
		.active = true,
		.properties = p_color, // Start property
		.static_object = false,
		.world_transform = (PX_Transform2){.pos=(PX_Vector2){0}, .scale=(PX_Scale2){1,1}, .rot=0},
		.local_transform = (PX_Transform2){.pos=(PX_Vector2){0}, .scale=(PX_Scale2){100,100}, .rot=0},
		.has_children = false,
		.screen_pos_fixed = false,
		.ex_data = (void*)panel,
		.ex_data_type = PX_RS_OBJECT_2D_TYPE_PANEL
	};

	append_to_scene_2d(&new_panel);
}

void scene_context_panel_evs_init(PX_Dropdown* scene_context_panel_dd) {
    scene_context_panel_dropdown = scene_context_panel_dd;
}

void scene_context_panel_evs_handle_events(PX_Event_GSignal* signal) {
    switch (signal->type) {
        case EVENT_GSIGNAL_UI_DROPDOWN_CLICK:
			if (!signal->ui_dropdown_click.clicked_node || !signal->ui_dropdown_click.dropdown) return;
            if (signal->ui_dropdown_click.dropdown != scene_context_panel_dropdown) return;
            switch (signal->ui_dropdown_click.clicked_node->identifier) {
				// Create (Identifier 1, Options 1000-1005)
				// Create -> 2D (Identifier 1000, Options 10001-10003)
				case 10001: handle_create_2d_panel(); break;

				default: return;
            }
            break;
        default: return;
    }
}
