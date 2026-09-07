#pragma once

#include <rendering-sys.h>
#include <font.h>

char* px_util_strdup(const char* s);
PX_Transform2 px_util_convert_anchor_to_transform(PX_AnchorRect r);
PX_DropdownNode* px_util_dropdown_node_create(const char* label, PX_DropdownNode* parent, size_t iden, PX_Font* font, float font_size);
void px_util_dropdown_node_destroy(PX_DropdownNode* node);
void px_util_dropdown_destroy(PX_Dropdown* dd);
PX_Property* px_util_property_add(const char* label, PX_PropertyType type, PX_Property* parent);
void px_util_destroy_properties(PX_Property* start, PX_Property** field);