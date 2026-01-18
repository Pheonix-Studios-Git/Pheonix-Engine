#pragma once

#include <stdbool.h>

#include <rendering-sys.h>
#include <err-codes.h>

#define PX_EDITOR_CUR_VERSION 1.0f

typedef enum {
    PX_EditorComponentType_NONE,
    PX_EditorComponentType_UNKNOWN,
    PX_EditorComponentType_SCRIPT
} PX_EditorComponentType;

typedef struct {
    char* name;
    PX_EditorComponentType type;
} PX_EditorComponent;

typedef struct {
    bool used;
    char* name;
} PX_EditorMaterial;

typedef struct PX_EditorObject {
    struct PX_EditorObject* parent;
    struct PX_EditorObject* child;
    struct PX_EditorObject* next;

    int child_count;

    char* name;
    
    PX_Transform3 transform;
    PX_EditorMaterial material;
    PX_EditorComponent* components;
    int component_count;
} PX_EditorObject;

typedef struct {
    bool initialized;
    float opengl_version; // Default = 4.0, Fallback = 2.0
    float editor_version;

    bool saved;
    char* project_dir;
    char* project_name;

    PX_EditorObject* objects;
    int total_object_count;
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
bool editor_add_object(PX_EditorObject* parent, PX_EditorObject* obj);
void editor_draw_scene_panel(PX_Transform2 transform, PX_Color4 color, float noise, float cradius, PX_Font* font, float font_size, int xspacing, int yspacing);
