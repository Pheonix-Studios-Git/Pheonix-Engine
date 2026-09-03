#pragma once

#include <rendering-sys.h>
#include <font.h>

#define MAX_BATCHES 256
#define MAX_VERTEX_COUNT 8192
#define MAX_3D_INDICES 131072

struct sdf_font {
    PX_GPU_Handle texture;
    struct px_sdf_glyph* glyphs;
    uint16_t glyph_count;

    float ascent;
    float descent;
    float line_gap;
    float sdf_range;
};

struct vertex_2d {
    float x;
    float y;
    float u;
    float v;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct vertex_3d {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

enum batch_2d_type {
    BATCH_2D_PANEL,
    BATCH_2D_LINE,
    BATCH_2D_TEXT
};

enum batch_3d_type {
    BATCH_3D_SIMPLE,
    BATCH_3D_LINES
};

struct batch_2d {
    enum batch_2d_type type;
	PX_GPU_Handle fbo;
    int fbo_x;
    int fbo_y;
    int fbo_w;
    int fbo_h;
    bool switch_fbo;
    bool pure_color;

	float line_width;
	
    int vertex_offset;
    int vertex_count;

    struct vertex_2d* vertices;

    PX_Scale2 size;
    PX_Scale2 texel_size;
    float noise;
    float corner_radius;
    PX_GPU_Handle texture;
	PX_GPU_Handle sampler;
    float text_sdf_width;
    float text_pixel_height;
    float text_outline_width;
    PX_Color4 text_outline_color;
};

struct batch_3d {
    enum batch_3d_type type;
    PX_GPU_Handle fbo;
    int fbo_x;
    int fbo_y;
    int fbo_w;
    int fbo_h;
    bool switch_fbo;

    bool pure_color;

    float line_width;

    int vertex_offset;
    size_t vertex_count;
    struct vertex_3d* vertices;

    int index_offset;
    size_t index_count;
    uint32_t* indices;

    bool depth_override;

    PX_Transform3 transform;
    PX_Color4 color;
};

struct scene_cam_3d {
    vec3 position;
    vec3 target;
    vec3 up;

    float yaw;
    float pitch;

    float move_speed;
    float mouse_sens;
};

struct scene_cam_2d {
    vec2 position;
	
    float zoom;
	float move_speed;
};

extern struct scene_cam_3d gscene_cam_3d;
extern struct scene_cam_2d gscene_cam_2d;

void px_rs_internal_push_batch_3d(struct batch_3d* b);
void px_rs_internal_push_batch_2d(struct batch_2d* b);