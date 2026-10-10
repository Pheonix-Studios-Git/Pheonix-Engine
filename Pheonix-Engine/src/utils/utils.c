#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <rendering-sys.h>
#include <font.h>
#include <window-sys.h>

#include <event-sys/keycodes.h>

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

PX_AnchorRect px_util_convert_transform_to_anchor(PX_Transform2 t, PX_Window* win) {
	if (!win) return (PX_AnchorRect){0}; // If Scale is 0, it can't be used!

    return (PX_AnchorRect){
        .x = (float)t.pos.x / (float)win->width,
		.y = (float)t.pos.y / (float)win->height,
		.w = (float)t.scale.w / (float)win->width,
		.h = (float)t.scale.h / (float)win->height,
		.window = win
    };
}

// Auto sets callback data as identifier
PX_DropdownNode* px_util_dropdown_node_create(const char* label, PX_DropdownNode* parent, size_t iden, PX_Font* font, float font_size, void (*on_select)(PX_DropdownNode* node, void* identifier)) {
    PX_DropdownNode* node = malloc(sizeof(*node));
    if (!node) return NULL;

    *node = (PX_DropdownNode){
        .label = label ? px_util_strdup(label) : NULL,
		.identifier = iden,
        .on_select = on_select,
        .callback_data = NULL,
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
	node->callback_data = (void*)&node->identifier;

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
		if (cur->label) free((void*)cur->label);

		switch (cur->type) {
			case PX_RS_PROPERTY_STRING: {
				if (cur->size != sizeof(char**) || !cur->data) break;
				
				free(*(char**)cur->data);
				*(char**)cur->data = NULL;
			}

			default: break;
		}

		PX_Property* to_free = cur;
		cur = cur->next;

		free(to_free);
	}

	if (field) *field = NULL;
}

char px_util_ekeycode_to_char(PX_EKeycodes key, bool caps, bool shift) {
	switch (key) {
		case EKeycode_0: return shift ? ')' : '0';
		case EKeycode_1: return shift ? '!' : '1';
		case EKeycode_2: return shift ? '@' : '2';
		case EKeycode_3: return shift ? '#' : '3';
		case EKeycode_4: return shift ? '$' : '4';
		case EKeycode_5: return shift ? '%' : '5';
		case EKeycode_6: return shift ? '^' : '6';
		case EKeycode_7: return shift ? '&' : '7';
		case EKeycode_8: return shift ? '*' : '8';
		case EKeycode_9: return shift ? '(' : '9';

		case EKeycode_Q: return caps ^ shift ? 'Q' : 'q';
		case EKeycode_W: return caps ^ shift ? 'W' : 'w';
		case EKeycode_E: return caps ^ shift ? 'E' : 'e';
		case EKeycode_R: return caps ^ shift ? 'R' : 'r';
		case EKeycode_T: return caps ^ shift ? 'T' : 't';
		case EKeycode_Y: return caps ^ shift ? 'Y' : 'y';
		case EKeycode_U: return caps ^ shift ? 'U' : 'u';
		case EKeycode_I: return caps ^ shift ? 'I' : 'i';
		case EKeycode_O: return caps ^ shift ? 'O' : 'o';
		case EKeycode_P: return caps ^ shift ? 'P' : 'p';
		case EKeycode_A: return caps ^ shift ? 'A' : 'a';
		case EKeycode_S: return caps ^ shift ? 'S' : 's';
		case EKeycode_D: return caps ^ shift ? 'D' : 'd';
		case EKeycode_F: return caps ^ shift ? 'F' : 'f';
		case EKeycode_G: return caps ^ shift ? 'G' : 'g';
		case EKeycode_H: return caps ^ shift ? 'H' : 'h';
		case EKeycode_J: return caps ^ shift ? 'J' : 'j';
		case EKeycode_K: return caps ^ shift ? 'K' : 'k';
		case EKeycode_L: return caps ^ shift ? 'L' : 'l';
		case EKeycode_Z: return caps ^ shift ? 'Z' : 'z';
		case EKeycode_X: return caps ^ shift ? 'X' : 'x';
		case EKeycode_C: return caps ^ shift ? 'C' : 'c';
		case EKeycode_V: return caps ^ shift ? 'V' : 'v';
		case EKeycode_B: return caps ^ shift ? 'B' : 'b';
		case EKeycode_N: return caps ^ shift ? 'N' : 'n';
		case EKeycode_M: return caps ^ shift ? 'M' : 'm';

		case EKeycode_Space: return ' ';

		case EKeycode_Backtick: return shift ? '~' : '`';
		case EKeycode_Dash: return shift ? '_' : '-';
		case EKeycode_Equals: return shift ? '+' : '=';
		case EKeycode_SqBracketOpen: return shift ? '{' : '[';
		case EKeycode_SqBracketClose: return shift ? '}' : ']';
		case EKeycode_Backslash: return shift ? '|' : '\\';
		case EKeycode_Semicolon: return shift ? ':' : ';';
		case EKeycode_SingleQuotes: return shift ? '"' : '\'';
		case EKeycode_Comma: return shift ? '<' : ',';
		case EKeycode_Fullstop: return shift ? '>' : '.';
		case EKeycode_Slash: return shift ? '?' : '/';

		case EKeycode_Num0: return shift ? ')' : '0';
		case EKeycode_Num1: return shift ? '!' : '1';
		case EKeycode_Num2: return shift ? '@' : '2';
		case EKeycode_Num3: return shift ? '#' : '3';
		case EKeycode_Num4: return shift ? '$' : '4';
		case EKeycode_Num5: return shift ? '%' : '5';
		case EKeycode_Num6: return shift ? '^' : '6';
		case EKeycode_Num7: return shift ? '&' : '7';
		case EKeycode_Num8: return shift ? '*' : '8';
		case EKeycode_Num9: return shift ? '(' : '9';

		case EKeycode_NumPoint: return '.';
		case EKeycode_NumSlash: return '/';
		case EKeycode_NumAsterik: return '*';
		case EKeycode_NumDash: return '-';
		case EKeycode_NumPlus: return '+';

		default: return '\0';
	}
}