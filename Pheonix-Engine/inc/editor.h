#pragma once

#include <stdbool.h>

#include <rendering-sys.h>
#include <err-codes.h>

#define PX_EDITOR_CUR_VERSION 1.0f

typedef struct {
    bool initialized;
    float opengl_version; // Default = 4.0, Fallback = 2.0
    float editor_version;

    bool saved;
    char* project_dir;
    char* project_name;

    PX_Scene* scene;
} PX_EditorState;

#pragma pack(push, 1)
typedef struct {
    long unsigned int name;
} PX_PXProj_Material;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    long unsigned int parent;
    long unsigned int child;
    long unsigned int next;
    int child_count;

    long unsigned int name;

    PX_Transform3 transform;
    PX_PXProj_Material material;
    //PX_PXProj_Components components;
    int component_count;
} PX_PXProj_Object;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    float opengl_version;
    float editor_version;
    float engine_version;
    
    long unsigned int project_dir;
    long unsigned int project_name;

    PX_PXProj_Object* objects;
    int total_object_count;
} PX_PXProj_Hdr;
#pragma pack(pop)

t_err_codes editor_new_project(char* proj_name);
PX_EditorState* editor_get_state(void);
void editor_draw_scene_panel(PX_Vector2 mpos, PX_Transform2 transform, PX_Color4 iline_color, PX_Color4 text_color, PX_Color4 color, PX_Color4 Hcolor, float noise, float cradius, PX_Font* font, float font_size, int xspacing, int yspacing);
void editor_click_scene_panel(PX_Vector2 mpos, PX_Transform2 transform, PX_Font* font, float font_size, int xspacing, int yspacing);
