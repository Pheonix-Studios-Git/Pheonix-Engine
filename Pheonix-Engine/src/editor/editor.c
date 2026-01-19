#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <rendering-sys.h>
#include <err-codes.h>
#include <window-sys.h>
#include <editor.h>

static PX_EditorState state_raw = {0};
static PX_EditorState* state = &state_raw;

static t_err_codes editor_init_state(char* proj_name) {
    state->editor_version = PX_EDITOR_CUR_VERSION;

    PX_EditorObject* root_obj = (PX_EditorObject*)calloc(1, sizeof(PX_EditorObject));
    if (!root_obj)
        return ERR_ALLOC_FAILED;

    root_obj->parent = NULL;
    root_obj->child = NULL;
    root_obj->next = NULL;
    root_obj->child_count = 0;
    root_obj->name = "root";
    root_obj->components = NULL;
    root_obj->component_count = 0;

    state->objects = root_obj;
    state->total_object_count = 1;
    state->saved = false;
    state->project_dir = NULL;
    state->project_name = proj_name;

    state->initialized = true;

    return ERR_SUCCESS;
}

t_err_codes editor_new_project(char* proj_name) {
    return editor_init_state(proj_name);
}

PX_EditorState* editor_get_state(void) {
    return state;
}

bool editor_add_object(PX_EditorObject* parent, PX_EditorObject* obj) {
    if (parent == NULL) {
        PX_EditorObject* nxt = state->objects;
        for (int i = 0; i < state->total_object_count; i++) {
            if (nxt->next == NULL) {
                nxt->next = obj;
                state->total_object_count++;
                return true;
            }
            nxt = nxt->next;
        }
    }

    if (parent->child_count < 1) {
        parent->child = obj;
        parent->child_count = 1;
        state->total_object_count++;
        return true;
    }

    PX_EditorObject* nxt = parent->child;
    for (int i = 0; i < parent->child_count; i++) {
        if (nxt->next == NULL) {
            nxt->next = obj;
            parent->child_count++;
            state->total_object_count++;
            return true;
        }
        nxt = nxt->next;
    }

    return false;
}

static int editor_render_object(PX_EditorObject* object, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 color, PX_Font* font, float font_size, int xspacing, int yspacing, bool render_name) {
    PX_EditorObject* obj = object;
    int x = transform.pos.x;
    int y = transform.pos.y;

    while (obj) {
        if (render_name)
            px_rs_render_text(obj->name, font_size, (PX_Vector2){x, y}, color, font);

        PX_Transform2 child_transform = (PX_Transform2){
            (PX_Vector2){x + xspacing, y + yspacing},
            transform.scale
        };

        if (obj->child) {
            child_transform.pos.y = editor_render_object(obj->child, child_transform, iline_color, color, font, font_size, xspacing, yspacing, true);
        }

        y = child_transform.pos.y;
        obj = obj->next;
    }

    return y;
    int iline_x = transform.pos.x - 2;
    int iline_y = transform.pos.y;
    if (iline_x < 0)
        iline_x = transform.pos.x;
    if (iline_x < 0)
        iline_x = 0;
    int iline_w = 2;
    int iline_h = y - iline_y;
    if (iline_h <= 0)
        iline_h = 1;
    px_rs_draw_line(
        (PX_Vector2){iline_x, iline_y},
        (PX_Vector2){iline_x, iline_y + iline_h},
        (float)iline_w,
        iline_color
    );

    return y;
}

void editor_draw_scene_panel(PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 text_color, PX_Color4 color, float noise, float cradius, PX_Font* font, float font_size, int xspacing, int yspacing) {
    px_rs_draw_panel(transform, color, noise, cradius);

    int x = transform.pos.x + 8;
    int y = transform.pos.y + 8;
    PX_Transform2 tran = (PX_Transform2){
        (PX_Vector2){x, y},
        transform.scale
    };

    (void)editor_render_object(state->objects, tran, iline_color, text_color, font, font_size, xspacing, yspacing, true);
}

