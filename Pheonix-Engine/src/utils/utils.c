#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <rendering-sys.h>
#include <font.h>
#include <window-sys.h>
#include <utils.h>

char* px_util_strdup(const char* s) {
	if (!s) return NULL;

	size_t len = strlen(s);

    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;

    memcpy(out, s, len);
	out[len] = '\0';
    return out;
}

PX_Transform2 px_util_convert_anchor_to_transform(PX_AnchorRect r) {
	if (!r.window) return (PX_Transform2){0}; // If Scale is 0, it can't be used!

	PX_Window* win = (PX_Window*)r.window;
    return (PX_Transform2){
        .pos = {
            .x = r.x * win->width,
            .y = r.y * win->height
        },
        .scale = {
            .w = r.w * win->width,
            .h = r.h * win->height
        },
		.rot = 0
    };
}

PX_DropdownNode* px_util_dropdown_node_create(const char* label, PX_DropdownNode* parent, size_t iden, PX_Font* font, float font_size) {
    PX_DropdownNode* node = malloc(sizeof(*node));
    if (!node) return NULL;

    *node = (PX_DropdownNode){
        .label = label ? px_util_strdup(label) : NULL,
		.identifier = iden,
        .on_select = NULL,
        .user_data = NULL,
        .parent = parent,
        .next = NULL,
        .children = NULL,
        .children_count = 0,
        .open = false,
        .hovered = false,
		.vertical = true,
        .scale = label ? (PX_Scale2){px_rs_text_width(font, label, font_size) + 5, font_size + 5} : (PX_Scale2){0},
        .rot = 0.0f
    };

    if (!node->label && label) {
        free(node);
        return NULL;
    }

	if (!parent) return node;

	PX_DropdownNode** children = realloc(parent->children, sizeof(PX_DropdownNode*)*(parent->children_count + 1));
    if (!children) {
		free(node);
		return NULL;
	}

    parent->children = children;
    parent->children[parent->children_count] = node;
    parent->children_count++;

    if (parent->children_count > 1) {
        PX_DropdownNode* previous = parent->children[parent->children_count - 2];
        previous->next = node;
    }

    return node;
}

void px_util_dropdown_node_destroy(PX_DropdownNode* node) {
    if (!node) return;

    for (size_t i = 0; i < node->children_count; i++) px_util_dropdown_node_destroy(node->children[i]);
	if (node->children) free(node->children);
    if (node->label) free((void*)node->label);

	free(node);
}

void px_util_dropdown_destroy(PX_Dropdown* dd) {
	if (!dd) return;
	px_util_dropdown_node_destroy(dd->root);
	dd->root = NULL;
}

PX_Property* px_util_property_add(const char* label, PX_PropertyType type, PX_Property* parent) {
	if (!label) return NULL; // Properties must have labels

	PX_Property* pnode = (PX_Property*)malloc(sizeof(PX_Property));
	if (!pnode) return NULL;
	*pnode = (PX_Property){0};

	pnode->label = px_util_strdup(label);
	pnode->type = type;

	if (!parent) return pnode;
	parent->next = pnode;
	pnode->parent = parent;

	return pnode;
}

void px_util_destroy_properties(PX_Property* start, PX_Property** field) {
	PX_Property* cur = start;
	while (cur) {
		if (cur->label) free(cur->label);

		PX_Property* to_free = cur;
		cur = cur->next;

		free(to_free);
	}

	if (field) *field = NULL;
}
