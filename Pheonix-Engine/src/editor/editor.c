#include <stdlib.h>
#include <stdbool.h>

#include <pheonix-engine.h>
#include <rendering-sys.h>
#include <font.h>
#include <event-sys.h>
#include <err-codes.h>
#include <window-sys.h>
#include <editor.h>

static PX_EditorState state_raw = {0};
static PX_EditorState* state = &state_raw;

static t_err_codes editor_init_state(char* proj_name, PX_EditorMode base_mode) {
    state->editor_version = PX_EDITOR_CUR_VERSION;

	PX_2D_Object* root_obj2d = &engine_2drenderer_main_scene.objects[engine_2drenderer_main_scene.object_count++];
    if (!root_obj2d) return ERR_ALLOC_FAILED;

    PX_3D_Object* root_obj3d = &engine_3drenderer_main_scene.objects[engine_3drenderer_main_scene.object_count++];
    if (!root_obj3d) return ERR_ALLOC_FAILED;

    root_obj3d->active = true;
    root_obj3d->ex_data = NULL;
    root_obj3d->ex_data_type = PX_RS_OBJECT_3D_TYPE_EMPTY;
    root_obj3d->has_children = false;
    root_obj3d->name = "root";
    root_obj3d->static_object = true;
    root_obj3d->type = PX_RS_OBJECT_3D_TYPE_EMPTY;
    root_obj3d->local_transform = (PX_Transform3){.rot.w=1,.scale=(PX_Scale3){1,1,1}};
    root_obj3d->world_transform = (PX_Transform3){.rot.w=1,.scale=(PX_Scale3){1,1,1}};

	root_obj2d->active = true;
    root_obj2d->ex_data = NULL;
    root_obj2d->ex_data_type = PX_RS_OBJECT_2D_TYPE_EMPTY;
    root_obj2d->has_children = false;
    root_obj2d->name = "root";
    root_obj2d->static_object = true;
    root_obj2d->type = PX_RS_OBJECT_2D_TYPE_EMPTY;
    root_obj2d->local_transform = (PX_Transform2){.scale=(PX_Scale2){1,1}};
    root_obj2d->world_transform = (PX_Transform2){.scale=(PX_Scale2){1,1}};

    state->scene_3d = &engine_3drenderer_main_scene;
	state->scene_2d = &engine_2drenderer_main_scene;
    state->saved = false;
    state->project_dir = NULL;
    state->project_name = proj_name;
	state->current_mode = base_mode;

    state->initialized = true;

    return ERR_SUCCESS;
}

t_err_codes editor_new_project(PX_Scene_3D* cScene3D, PX_Scene_2D* cScene2D, char* proj_name, PX_EditorMode base_mode) {
	if (!cScene3D && base_mode == PX_EDITOR_MODE_3D) return editor_init_state(proj_name, base_mode);
	if (cScene3D) {
		cScene3D->active_object = NULL;
		cScene3D->bvh_node_count = 0;
		cScene3D->editor_object_count = 0;
		cScene3D->object_count = 0;
	}

	if (!cScene2D && base_mode == PX_EDITOR_MODE_2D) return editor_init_state(proj_name, base_mode);
	if (cScene2D) {
		cScene2D->active_object = NULL;
		cScene2D->bvh_node_count = 0;
		cScene2D->editor_object_count = 0;
		cScene2D->object_count = 0;
	}

	return editor_init_state(proj_name, base_mode);
}

PX_EditorState* editor_get_state(void) {
    return state;
}

static int editor_render_object_3d(PX_AnchorRect lv, PX_Vector2 mpos, PX_3D_Object* object, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 color, PX_Color4 Hcolor, PX_Font* font, float font_size, int xspacing, int yspacing, bool render_name, bool draw_vertical) {
    if (!object) return transform.pos.y;
    PX_3D_Object* obj = object;

    int x = transform.pos.x;
    int y = transform.pos.y;

    const int line_offset_x = 8;
    const int text_offset_x = 12;

    int branch_x = x - line_offset_x;

    if (draw_vertical) {
        px_rs_draw_line(
			lv,
            (PX_Vector2){branch_x, y - (yspacing / 2)},
            (PX_Vector2){branch_x, y + (yspacing / 2)},
            1.0f,
            iline_color
        );
    }

    px_rs_draw_line(
		lv,
        (PX_Vector2){branch_x, y},
        (PX_Vector2){x + 4, y},
        1.0f,
        iline_color
    );

    if (render_name) {
        PX_Vector2 box = {x + text_offset_x, y - (font_size * 0.35f)};
        int w = px_rs_text_width(font, object->name, font_size);

        PX_Color4 Xcolor = color;
        if (
            mpos.x >= box.x &&
            mpos.y >= box.y &&
            mpos.x <= box.x + w &&
            mpos.y <= (int)(box.y + font_size)
        ) {
            Xcolor = Hcolor;
        }

        px_rs_render_text(
            obj->name,
            font_size,
			lv,
            box,
            Xcolor,
            font
        );
    }

    int current_y = y;

    if (obj->has_children) {
        int child_start_y = current_y + yspacing;
        int child_end_y = child_start_y;

        for (size_t i = 0; i < sizeof(obj->children) / sizeof(uintptr_t); i++) {
            PX_3D_Object* child = obj->children[i];

            if (!child) continue;

            PX_Transform2 child_transform = {
                .pos = {
                    x + xspacing,
                    child_end_y
                },
                .scale = transform.scale
            };

            child_end_y = editor_render_object_3d(
				lv,
                mpos,
                child,
                child_transform,
                iline_color,
                color,
                Hcolor,
                font,
                font_size,
                xspacing,
                yspacing,
                true,
                true
            );

            child_end_y += yspacing;
        }

        px_rs_draw_line(
			lv,
            (PX_Vector2){branch_x, child_start_y},
            (PX_Vector2){branch_x, child_end_y - yspacing},
            2.0f,
            iline_color
        );

        current_y = child_end_y - yspacing;
    }

    return current_y;
}

static int editor_render_object_2d(PX_AnchorRect lv, PX_Vector2 mpos, PX_2D_Object* object, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 color, PX_Color4 Hcolor, PX_Font* font, float font_size, int xspacing, int yspacing, bool render_name, bool draw_vertical) {
    if (!object) return transform.pos.y;
    PX_2D_Object* obj = object;

    int x = transform.pos.x;
    int y = transform.pos.y;

    const int line_offset_x = 8;
    const int text_offset_x = 12;

    int branch_x = x - line_offset_x;

    if (draw_vertical) {
        px_rs_draw_line(
			lv,
            (PX_Vector2){branch_x, y - (yspacing / 2)},
            (PX_Vector2){branch_x, y + (yspacing / 2)},
            1.0f,
            iline_color
        );
    }

    px_rs_draw_line(
		lv,
        (PX_Vector2){branch_x, y},
        (PX_Vector2){x + 4, y},
        1.0f,
        iline_color
    );

    if (render_name) {
        PX_Vector2 box = {x + text_offset_x, y - (font_size * 0.35f)};
        int w = px_rs_text_width(font, object->name, font_size);

        PX_Color4 Xcolor = color;
        if (
            mpos.x >= box.x &&
            mpos.y >= box.y &&
            mpos.x <= box.x + w &&
            mpos.y <= (int)(box.y + font_size)
        ) {
            Xcolor = Hcolor;
        }

        px_rs_render_text(
            obj->name,
            font_size,
			lv,
            box,
            Xcolor,
            font
        );
    }

    int current_y = y;

    if (obj->has_children) {
        int child_start_y = current_y + yspacing;
        int child_end_y = child_start_y;

        for (size_t i = 0; i < sizeof(obj->children) / sizeof(uintptr_t); i++) {
            PX_2D_Object* child = obj->children[i];

            if (!child) continue;

            PX_Transform2 child_transform = {
                .pos = {
                    x + xspacing,
                    child_end_y
                },
                .scale = transform.scale
            };

            child_end_y = editor_render_object_2d(
				lv,
                mpos,
                child,
                child_transform,
                iline_color,
                color,
                Hcolor,
                font,
                font_size,
                xspacing,
                yspacing,
                true,
                true
            );

            child_end_y += yspacing;
        }

        px_rs_draw_line(
			lv,
            (PX_Vector2){branch_x, child_start_y},
            (PX_Vector2){branch_x, child_end_y - yspacing},
            2.0f,
            iline_color
        );

        current_y = child_end_y - yspacing;
    }

    return current_y;
}

static int editor_click_object_3d(PX_Scene_3D* scene, PX_Vector2 mpos, PX_3D_Object* object, PX_Transform2 transform, PX_Font* font, float font_size, int xspacing, int yspacing, bool render_name) {
    if (!object) return transform.pos.y;
    PX_3D_Object* obj = object;

    int x = transform.pos.x;
    int y = transform.pos.y;

    const int text_offset_x = 12;

    if (render_name) {
        PX_Vector2 box = {x + text_offset_x, y - (font_size * 0.35f)};

        int w = px_rs_text_width(font, object->name, font_size);
        if (
            mpos.x >= box.x &&
            mpos.y >= box.y &&
            mpos.x <= box.x + w &&
            mpos.y <= (int)(box.y + font_size)
        ) {
            PX_Event_GSignal s = {
                .core_quit = false,
                .type = EVENT_GSIGNAL_UI_SCENE_PANEL_CLICK,
                .ui_scenepanel_click = (PX_Event_GSignal_UIScenePanelClick){
                    .clicked_name = object->name,
                    .obj3d = object
                }
            };
            event_send_gsignal(&s);
            scene->active_object = obj;
            return y;
        }
    }

    int current_y = y;

    if (obj->has_children) {
        int child_start_y = current_y + yspacing;
        int child_end_y = child_start_y;

        for (size_t i = 0; i < sizeof(obj->children) / sizeof(uintptr_t); i++) {
            PX_3D_Object* child = obj->children[i];

            if (!child) continue;

            PX_Transform2 child_transform = {
                .pos = {
                    x + xspacing,
                    child_end_y
                },
                .scale = transform.scale
            };

            child_end_y = editor_click_object_3d(
                scene,
                mpos,
                child,
                child_transform,
                font,
                font_size,
                xspacing,
                yspacing,
                true
            );

            child_end_y += yspacing;
        }

        current_y = child_end_y - yspacing;
    }

    return current_y;
}

static int editor_click_object_2d(PX_Scene_2D* scene, PX_Vector2 mpos, PX_2D_Object* object, PX_Transform2 transform, PX_Font* font, float font_size, int xspacing, int yspacing, bool render_name) {
    if (!object) return transform.pos.y;
    PX_2D_Object* obj = object;

    int x = transform.pos.x;
    int y = transform.pos.y;

    const int text_offset_x = 12;

    if (render_name) {
        PX_Vector2 box = {x + text_offset_x, y - (font_size * 0.35f)};

        int w = px_rs_text_width(font, object->name, font_size);
        if (
            mpos.x >= box.x &&
            mpos.y >= box.y &&
            mpos.x <= box.x + w &&
            mpos.y <= (int)(box.y + font_size)
        ) {
            PX_Event_GSignal s = {
                .core_quit = false,
                .type = EVENT_GSIGNAL_UI_SCENE_PANEL_CLICK,
                .ui_scenepanel_click = (PX_Event_GSignal_UIScenePanelClick){
                    .clicked_name = object->name,
                    .obj2d = object
                }
            };
            event_send_gsignal(&s);
            scene->active_object = obj;
            return y;
        }
    }

    int current_y = y;

    if (obj->has_children) {
        int child_start_y = current_y + yspacing;
        int child_end_y = child_start_y;

        for (size_t i = 0; i < sizeof(obj->children) / sizeof(uintptr_t); i++) {
            PX_2D_Object* child = obj->children[i];

            if (!child) continue;

            PX_Transform2 child_transform = {
                .pos = {
                    x + xspacing,
                    child_end_y
                },
                .scale = transform.scale
            };

            child_end_y = editor_click_object_2d(
                scene,
                mpos,
                child,
                child_transform,
                font,
                font_size,
                xspacing,
                yspacing,
                true
            );

            child_end_y += yspacing;
        }

        current_y = child_end_y - yspacing;
    }

    return current_y;
}

void editor_draw_scene_panel(PX_AnchorRect local_viewport, PX_Vector2 mpos, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 text_color, PX_Color4 color, PX_Color4 Hcolor, float noise, float cradius, PX_Font* font, float font_size, int xspacing, int yspacing) {
	px_rs_draw_panel(local_viewport, (PX_Transform2){.pos=(PX_Vector2){transform.pos.x,transform.pos.y+4}, .scale=transform.scale, .rot=transform.rot}, color, noise, cradius, true);

    int x = transform.pos.x + 16;
    int y = transform.pos.y + 24;

    PX_Transform2 tran = {
        .pos = {x, y},
        .scale = transform.scale
    };

	switch (state->current_mode) {
		case PX_EDITOR_MODE_3D: {
			for (size_t i = 0; i < state->scene_3d->object_count; i++) {
				y = editor_render_object_3d(
					local_viewport,
					mpos,
					&state->scene_3d->objects[i],
					tran,
					iline_color,
					text_color,
					Hcolor,
					font,
					font_size,
					xspacing,
					yspacing,
					true,
					true
				);

				y += yspacing;

				tran.pos.y = y;
			}
			break;
		}

		case PX_EDITOR_MODE_2D: {
			for (size_t i = 0; i < state->scene_2d->object_count; i++) {
				y = editor_render_object_2d(
					local_viewport,
					mpos,
					&state->scene_2d->objects[i],
					tran,
					iline_color,
					text_color,
					Hcolor,
					font,
					font_size,
					xspacing,
					yspacing,
					true,
					true
				);

				y += yspacing;

				tran.pos.y = y;
			}
			break;
		}
	
		default: break;
	}
}

void editor_click_scene_panel(PX_Vector2 mpos, PX_Transform2 transform, PX_Font* font, float font_size, int xspacing, int yspacing) {
    int x = transform.pos.x + 16;
    int y = transform.pos.y + 24;

    PX_Transform2 tran = {
        .pos = {x, y},
        .scale = transform.scale
    };

	switch (state->current_mode) {
		case PX_EDITOR_MODE_3D: {
			for (size_t i = 0; i < state->scene_3d->object_count; i++) {
				y = editor_click_object_3d(
					state->scene_3d,
					mpos,
					&state->scene_3d->objects[i],
					tran,
					font,
					font_size,
					xspacing,
					yspacing,
					true
				);

				y += yspacing;

				tran.pos.y = y;
			}
			break;
		}
		case PX_EDITOR_MODE_2D: {
			for (size_t i = 0; i < state->scene_2d->object_count; i++) {
				y = editor_click_object_2d(
					state->scene_2d,
					mpos,
					&state->scene_2d->objects[i],
					tran,
					font,
					font_size,
					xspacing,
					yspacing,
					true
				);

				y += yspacing;

				tran.pos.y = y;
			}
			break;
		}

		default: break;
	}
}

