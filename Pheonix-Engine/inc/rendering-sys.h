#pragma once

#include <stdbool.h>

#include <err-codes.h>
#include <event-sys/keycodes.h>

#include <cglm/cglm.h>

#define PX_RS_MAX_DROPDOWN_ITEMS 16
#define PX_RS_MAX_DROPDOWN_OPTIONS 16

#define PX_RS_MAX_OBJECTS_PER_SCENE 4096

typedef uint64_t PX_GPU_Handle;
#define PX_RS_GPU_INVALID_HANDLE ((PX_GPU_Handle)0)

typedef enum {
	PX_RS_GPU_BACKEND_OPENGL,
	PX_RS_GPU_BACKEND_VULKAN
} PX_GPU_Backend;
#define __PHEONIX_ENGINE__RENDERING_SYS__DEFAULT_BACKEND__ PX_RS_GPU_BACKEND_OPENGL

struct PX_Font;

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
    float x, y;
} PX_Vector2;

typedef struct {
    float x, y, z;
} PX_Vector3;

typedef struct {
    float w, h;
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
	float rot;
} PX_Transform2;

typedef struct PX_DropdownNode {
	const char* label;
	size_t identifier;

	void (*on_select)(struct PX_DropdownNode* node, void* user_data);
	void* user_data;

	struct PX_DropdownNode* parent;
	struct PX_DropdownNode* next;
	struct PX_DropdownNode** children;
	size_t children_count;

	bool open;
	bool hovered;
	bool vertical;

	PX_Scale2 scale;
	float rot;

	PX_Transform2 rendered_transform;
} PX_DropdownNode;

typedef struct {
	PX_Transform2 transform;
	PX_Vector2 text_start_offset;
	float node_spacing;

	struct PX_Font* font;
	float font_size;

	PX_Color4 panel_color;
	PX_Color4 subpanel_color;
	PX_Color4 text_hover_color;
	PX_Color4 text_color;

	float noise;
	float cradius;

	PX_DropdownNode* root;

	int64_t hover_index;
	bool visible;
	bool screen_pos_fixed;
} PX_Dropdown;

typedef enum {
    PX_RS_OBJECT_3D_TYPE_MESH,
    PX_RS_OBJECT_3D_TYPE_LIGHT,
    PX_RS_OBJECT_3D_TYPE_CAMERA,
    PX_RS_OBJECT_3D_TYPE_EMPTY
} PX_3D_Object_Type;

typedef enum {
    PX_RS_OBJECT_2D_TYPE_PANEL,
	PX_RS_OBJECT_2D_TYPE_LIGHT,
	PX_RS_OBJECT_2D_TYPE_CAMERA,
	PX_RS_OBJECT_2D_TYPE_EMPTY,
	PX_RS_OBJECT_2D_TYPE_SPRITE
} PX_2D_Object_Type;

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

/*
ExData field

1. Type - Sprite -> Batch_UI structure
2. Type - Panel -> Color4 structure
*/
typedef struct PX_2D_Object {
    char* name;
    bool active;
    PX_2D_Object_Type type;

    bool static_object;

    PX_Transform2 world_transform;
    PX_Transform2 local_transform;

    struct PX_2D_Object* children[10]; // 10 MAX Children for now
    bool has_children;

	bool screen_pos_fixed;

    void* ex_data;
    PX_2D_Object_Type ex_data_type;
} PX_2D_Object;

typedef enum {
    PX_RS_OBJECT_3D_EDITOR_GRID,
    PX_RS_OBJECT_3D_EDITOR_GIZMO
} PX_3D_Editor_Object_Type;

typedef enum {
    PX_RS_OBJECT_2D_EDITOR_GRID,
    PX_RS_OBJECT_2D_EDITOR_GIZMO
} PX_2D_Editor_Object_Type;

/*
ExData field

1. Type - Grid -> PX_EditorGrid_3D structure
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

/*
ExData field

1. Type - Grid -> PX_EditorGrid_2D structure
2. Type - Gizmo -> bool pointer to define hover
*/
typedef struct PX_2D_Editor_Object {
    char* name;
    bool active;
    PX_2D_Editor_Object_Type type;
    uint16_t id;

    bool static_object;

    PX_Transform2 world_transform;
    PX_Transform2 local_transform;

    struct PX_2D_Editor_Object* children[10];
    bool has_children;

    void* ex_data;
    PX_2D_Editor_Object_Type ex_data_type;
} PX_2D_Editor_Object;

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
} PX_Scene_3D;

typedef struct {
    PX_2D_Object objects[PX_RS_MAX_OBJECTS_PER_SCENE];
    PX_2D_Object* active_object;
    size_t object_count;

    PX_BVHNode bvh_nodes[PX_RS_MAX_OBJECTS_PER_SCENE];
    size_t bvh_node_count;

    PX_2D_Editor_Object editor_objects[PX_RS_MAX_OBJECTS_PER_SCENE];
    size_t editor_object_count;
} PX_Scene_2D;

typedef struct {
    bool visible;

    float half_size;
    float spacing;

    PX_Color4 color;
} PX_EditorGrid_3D;

typedef struct {
    bool visible;

    float half_size;
    float spacing;

    PX_Color4 color;
} PX_EditorGrid_2D;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} PX_AnchorRect;

typedef enum {
	// Red Channel
	PX_RS_TEXTURE_FORMAT_R8UNORM, // Red Channel only Normalized Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_R8SNORM, // Red Channel only Normalized Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_R8U, // Red Channel only Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_R8I, // Red Channel only Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_R16UNORM, // Red Channel only Normalized Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_R16SNORM, // Red Channel only Normalized Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_R16U, // Red Channel only Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_R16I, // Red Channel only Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_R16F, // Red Channel only Floating Point (16-bit)
	PX_RS_TEXTURE_FORMAT_R32U, // Red Channel only Unsigned Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_R32I, // Red Channel only Signed Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_R32F, // Red Channel only Floating Point (32-bit)

	// Red + Green Channel
	PX_RS_TEXTURE_FORMAT_RG8UNORM, // Red and Green Channel only Normalized Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RG8SNORM, // Red and Green Channel only Normalized Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RG8U, // Red and Green Channel only Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RG8I, // Red and Green Channel only Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RG16UNORM, // Red and Green Channel only Normalized Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RG16SNORM, // Red and Green Channel only Normalized Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RG16U, // Red and Green Channel only Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RG16I, // Red and Green Channel only Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RG16F, // Red and Green Channel only Floating Point (16-bit)
	PX_RS_TEXTURE_FORMAT_RG32U, // Red and Green Channel only Unsigned Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RG32I, // Red and Green Channel only Signed Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RG32F, // Red and Green Channel only Floating Point (32-bit)
	
	// Red + Green + Blue Channel
	PX_RS_TEXTURE_FORMAT_RGB8UNORM, // Red, Green and Blue Channel only Normalized Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGB8SNORM, // Red, Green and Blue Channel only Normalized Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGB8U, // Red, Green and Blue Channel only Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGB8I, // Red, Green and Blue Channel only Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGB8sRGB, // Red, Green and Blue Channel only sRGB Encoded (8-bit)
	PX_RS_TEXTURE_FORMAT_RGB16UNORM, // Red, Green and Blue Channel only Normalized Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGB16SNORM, // Red, Green and Blue Channel only Normalized Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGB16U, // Red, Green and Blue Channel only Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGB16I, // Red, Green and Blue Channel only Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGB16F, // Red, Green and Blue Channel only Floating Point (16-bit)
	PX_RS_TEXTURE_FORMAT_RGB32U, // Red, Green and Blue Channel only Unsigned Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RGB32I, // Red, Green and Blue Channel only Signed Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RGB32F, // Red, Green and Blue Channel only Floating Point (32-bit)

	// Red + Green + Blue + Alpha Channel
	PX_RS_TEXTURE_FORMAT_RGBA8UNORM, // Red, Green, Blue and Alpha Channel only Normalized Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGBA8SNORM, // Red, Green, Blue and Alpha Channel only Normalized Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGBA8U, // Red, Green, Blue and Alpha Channel only Unsigned Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGBA8I, // Red, Green, Blue and Alpha Channel only Signed Integer (8-bit)
	PX_RS_TEXTURE_FORMAT_RGBA8sRGB, // Red, Green, Blue and Alpha Channel only sRGB Encoded (8-bit)
	PX_RS_TEXTURE_FORMAT_RGBA16UNORM, // Red, Green, Blue and Alpha Channel only Normalized Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGBA16SNORM, // Red, Green, Blue and Alpha Channel only Normalized Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGBA16U, // Red, Green, Blue and Alpha Channel only Unsigned Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGBA16I, // Red, Green, Blue and Alpha Channel only Signed Integer (16-bit)
	PX_RS_TEXTURE_FORMAT_RGBA16F, // Red, Green, Blue and Alpha Channel only Floating Point (16-bit)
	PX_RS_TEXTURE_FORMAT_RGBA32U, // Red, Green, Blue and Alpha Channel only Unsigned Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RGBA32I, // Red, Green, Blue and Alpha Channel only Signed Integer (32-bit)
	PX_RS_TEXTURE_FORMAT_RGBA32F, // Red, Green, Blue and Alpha Channel only Floating Point (32-bit)
} PX_TextureFormat;

typedef enum {
	PX_RS_TEXTURE_TYPE_1D = 0x0, // 1 Dimensional (W)
	PX_RS_TEXTURE_TYPE_1DA, // 1 Dimensional Array (W * Layers)

	PX_RS_TEXTURE_TYPE_2D, // 2 Dimensional (W*H)
	PX_RS_TEXTURE_TYPE_2DA, // 2 Dimensional Array (W*H * Layers)
	PX_RS_TEXTURE_TYPE_2DMS, // 2 Dimensional Multisample (W*H * Samples)
	PX_RS_TEXTURE_TYPE_2DMSA, // 2 Dimensional Multisamle Array (W*H * Samples*Layers)

	PX_RS_TEXTURE_TYPE_3D, // 3 Dimensional (W*H*D)
	PX_RS_TEXTURE_TYPE_3DCUBE, // 3 Dimensional Cube (W*H * 6)
	PX_RS_TEXTURE_TYPE_3DACUBE, // 3 Dimensional Cube Array (W*H * 6*Layers)
} PX_TextureType;

typedef enum {
	PX_RS_TEXTURE_FILTER_NEAREST, // Nearest-neighbor filtering
	PX_RS_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, // Nearest filtering with nearest mipmap level
	PX_RS_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, // Nearest filtering with linear interpolation between mipmap levels

	PX_RS_TEXTURE_FILTER_LINEAR, // Linear filtering
	PX_RS_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST, // Linear filtering with nearest mipmap level
	PX_RS_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR, // Linear filtering with linear interpolation between mipmap levels
} PX_TextureFilter;

typedef enum {
	PX_RS_TEXTURE_ADDRESS_REPEAT, // Repeat texture when coordinates are outside [0,1]
	PX_RS_TEXTURE_ADDRESS_MIRRORED_REPEAT, // Mirror texture when coordinates are outside [0,1]
	PX_RS_TEXTURE_ADDRESS_CLAMP_TO_EDGE, // Clamp coordinates to the edge of the texture
	PX_RS_TEXTURE_ADDRESS_CLAMP_TO_BORDER, // Use border color when coordinates are outside [0,1]
} PX_TextureAddressMode;

typedef struct {
	PX_GPU_Handle handle;
	PX_GPU_Handle sampler_handle;

	PX_TextureFormat format;
	PX_TextureType type;

	uint32_t width;
    uint32_t height;
    uint32_t depth;

    uint32_t layers;
    uint32_t mip_levels;
    uint32_t samples;
} PX_Texture;

typedef struct {
	PX_GPU_Handle handle;

	PX_TextureFilter min_filter;
	PX_TextureFilter mag_filter;

	PX_TextureAddressMode address_u;
	PX_TextureAddressMode address_v;
	PX_TextureAddressMode address_w;
} PX_Sampler;

// Window Structures related to Rendering
typedef struct {
	void* ictx;
} PX_WOpenGLContext;

typedef struct {
	const char** required_extensions;
	size_t required_extension_count;

	PX_GPU_Handle surfaceKHR;
} PX_WVulkanContext;

typedef struct {
	PX_GPU_Backend backend;
	void* iwin;

	union {
		PX_WOpenGLContext opengl;
		PX_WVulkanContext vulkan;
	};
} PX_WContext;

// Core
t_err_codes px_rs_init(PX_WContext* ctx); // Initialize Base Rendering Engine
t_err_codes px_rs_init_3d(PX_AnchorRect viewport); // Initialize 3D Rendering Engine
t_err_codes px_rs_init_2d(PX_AnchorRect viewport); // Initialize UI Rendering Engine
void px_rs_shutdown_2d(void); // Shutdown UI Rendering Engine
void px_rs_shutdown_3d(void); // Shutdown 3D Rendering Engine
void px_rs_shutdown(void); // Shutdown All Rendering Engines
void px_rs_frame_start(void);// Start Frame
void px_rs_frame_end(void); // End Frame
void px_rs_2d_frame_update(void); // Update Frame for UI Rendering Engine
void px_rs_3d_frame_update(void); // Update Frame for 3D Rendering Engine
void px_rs_frame_update(void); // Update Frame for All Rendering Engines
void px_rs_2d_resize(PX_AnchorRect viewport); // Resize Viewport for UI Rendering Engine
void px_rs_3d_resize(PX_AnchorRect viewport); // Resize Viewport for 3D Rendering Engine
void px_rs_update_scene_cam_3d(PX_Vector2 mdelta, PX_EKeycodes key); // Updates the 3D Scene Camera
void px_rs_update_scene_cam_2d(PX_Vector2 mdelta, PX_EKeycodes key); // Update the 2D Scene Camera
void px_rs_config_scene_cam_3d(float mouse_sensitivity, float speed); // Configurate 3D Scene Camera
void px_rs_config_scene_cam_2d(float speed); // Configurate 2D Scene Camera
t_err_codes px_rs_draw_panel(PX_AnchorRect local_viewport, PX_Transform2 tran, PX_Color4 color, float noise, float cradius, bool fixed_on_screen); // Draw a UI Panel
int px_rs_text_width(struct PX_Font* font, const char* text, float pixel_height); // Get Final Width of UI Text on Screen without Drawing it
t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_AnchorRect local_viewport, PX_Vector2 pos, PX_Color4 color, struct PX_Font* font); // Render UI Text on Screen
t_err_codes px_rs_draw_line(PX_AnchorRect local_viewport, PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color); // Draw a UI Line on Screen
t_err_codes px_rs_draw_dropdown(PX_AnchorRect local_viewport, PX_Dropdown* dd); // Draw a UI Dropdown on Screen
t_err_codes px_rs_draw_editor_objects_3d(PX_Scene_3D* scene); // Draw 3D Editor Objects in Viewport
t_err_codes px_rs_draw_scene_3d(PX_Scene_3D* scene); // Draw all 3D Objects in Viewport
t_err_codes px_rs_draw_editor_objects_2d(PX_AnchorRect local_viewport, PX_Scene_2D* scene); // Draw 2D Editor Objects on Screen
t_err_codes px_rs_draw_scene_2d(PX_AnchorRect local_viewport, PX_Scene_2D* scene); // Draw all 2D Objects on Screen
void px_rs_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale); // Handle Mouse Movement for Events and more
t_err_codes px_rs_change_backend(PX_GPU_Backend new_backend); // Change GPU API Backend

// Extra
t_err_codes px_rs_create_texture(PX_Texture* texture); // Create the desired texture
t_err_codes px_rs_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data); // Upload data to a created texture
t_err_codes px_rs_set_sampler(PX_Texture* texture, PX_Sampler* sampler); // Sets the desired sampler state for a texture
void px_rs_destroy_texture(PX_Texture* texture); // Destroys a created texture
void px_rs_destroy_sampler(PX_Sampler* sampler); // Destroys a created sampler