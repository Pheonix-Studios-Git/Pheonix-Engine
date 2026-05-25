#pragma once

#include <stdbool.h>

#include <err-codes.h>
#include <font.h>
#include <event-sys/keycodes.h>

#include <cglm/cglm.h>

#define PX_RS_MAX_DROPDOWN_ITEMS 16
#define PX_RS_MAX_DROPDOWN_OPTIONS 16

#define PX_RS_MAX_OBJECTS_PER_SCENE 4096

typedef struct {
    float m[16];
} PX_Mat4;

typedef struct {
    unsigned char r, g, b, a;
} PX_Color4;

typedef struct {
    unsigned char r, g, b;
} PX_Color3;

typedef struct {
    int x, y;
} PX_Vector2;

typedef struct {
    float x, y, z;
} PX_Vector3;

typedef struct {
    int w, h;
} PX_Scale2;

typedef struct {
    float w, h, l;
} PX_Scale3;

typedef struct {
    float x, y, z, w;
} PX_Orientation3;

typedef struct {
    PX_Vector3 pos;
    PX_Scale3 scale;
    PX_Orientation3 rot;
} PX_Transform3;

typedef struct {
    PX_Vector2 pos;
    PX_Scale2 scale;
} PX_Transform2;

typedef struct {
    char* label;
    int width;
    int height;
} PX_DropdownOption;

typedef struct {
    char* label;
    int width;
    int height;
    int spacing;

    PX_DropdownOption options[PX_RS_MAX_DROPDOWN_OPTIONS];
    int option_count;

    bool is_open;
    
    PX_Transform2 panel_tran;
    PX_Vector2 stext_pos;
    
    float font_size;

    PX_Color4 text_color;
    PX_Color4 panel_color;
    PX_Color4 hover_color;

    float panel_noise;
    float panel_cradius;

    int hover_index;
} PX_DropdownItem;

typedef struct {
    PX_Vector2 pos;
    PX_Vector2 stext_pos;
    int spacing;
    int width, height;
    
    PX_Color4 color;
    PX_Color4 hover_color;
    PX_Color4 text_color;
    
    PX_Font* font;
    float font_size;
    
    float noise;
    float cradius;

    PX_DropdownItem items[PX_RS_MAX_DROPDOWN_ITEMS];
    int item_count;

    int hover_index;
} PX_Dropdown;

typedef enum {
    OBJECT_3D_TYPE_MESH,
    OBJECT_3D_TYPE_LIGHT,
    OBJECT_3D_TYPE_CAMERA,
    OBJECT_3D_TYPE_EMPTY
} PX_3D_Object_Type;

/*
ExData field

1. Type - Mesh -> Batch_3D structure
*/
typedef struct PX_3D_Object {
    char* name;
    bool active;
    PX_3D_Object_Type type;

    bool static_object;

    PX_Transform3 world_transform;
    PX_Transform3 local_transform;

    struct PX_3D_Object* children[10]; // 10 MAX Children for now
    bool has_children;

    void* ex_data; // for meshes, its batch_3d struct
    PX_3D_Object_Type ex_data_type;
} PX_3D_Object;

typedef enum {
    OBJECT_3D_EDITOR_GRID,
    OBJECT_3D_EDITOR_GIZMO
} PX_3D_Editor_Object_Type;

/*
ExData field

1. Type - Grid -> PX_EditorGrid structure
2. Type - Gizmo -> bool pointer to define hover
*/
typedef struct PX_3D_Editor_Object {
    char* name;
    bool active;
    PX_3D_Editor_Object_Type type;
    uint16_t id;

    bool static_object;

    PX_Transform3 world_transform;
    PX_Transform3 local_transform;

    struct PX_3D_Editor_Object* children[10];
    bool has_children;

    void* ex_data;
    PX_3D_Editor_Object_Type ex_data_type;
} PX_3D_Editor_Object;

typedef struct PX_BVHNode {
    PX_Vector3 min;
    PX_Vector3 max;

    struct PX_BVHNode* left;
    struct PX_BVHNode* right;

    uint32_t first;
    uint32_t count;
} PX_BVHNode;

typedef struct {
    PX_3D_Object objects[PX_RS_MAX_OBJECTS_PER_SCENE];
    PX_3D_Object* active_object;
    size_t object_count;

    PX_BVHNode bvh_nodes[PX_RS_MAX_OBJECTS_PER_SCENE];
    size_t bvh_node_count;

    PX_3D_Editor_Object editor_objects[PX_RS_MAX_OBJECTS_PER_SCENE];
    size_t editor_object_count;
} PX_Scene;

typedef struct {
    bool visible;

    float half_size;
    float spacing;

    PX_Color4 color;
} PX_EditorGrid;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} PX_AnchorRect;

t_err_codes px_rs_init(void);
t_err_codes px_rs_init_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos);
t_err_codes px_rs_init_ui(PX_Scale2 screen_scale);
void px_rs_shutdown_ui(void);
void px_rs_shutdown_3d(void);
void px_rs_shutdown(void);
void px_rs_frame_start(void);
void px_rs_frame_end(void);
void px_rs_ui_frame_update(void);
void px_rs_3d_frame_update(void);
void px_rs_frame_update(void);
void px_rs_ui_resize(PX_Scale2 screen_scale);
void px_rs_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos);
void px_rs_update_scene_cam(PX_Vector2 mdelta, PX_EKeycodes key);
void px_rs_config_scene_cam(float mouse_sensitivity, float speed);
t_err_codes px_rs_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius);
int px_rs_text_width(PX_Font* font, const char* text, float pixel_height);
t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font);
t_err_codes px_rs_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color);
t_err_codes px_rs_draw_dropdown(PX_Dropdown* dd);
t_err_codes px_rs_draw_editor_objects(PX_Scene* scene);
t_err_codes px_rs_draw_scene(PX_Scene* scene);
void px_rs_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale);