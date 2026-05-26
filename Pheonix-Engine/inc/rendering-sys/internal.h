#pragma once

#include <rendering-sys.h>
#include <rendering-sys/opengl.h>

#define MAX_BATCHES 256
#define MAX_VERTEX_COUNT 8192
#define MAX_3D_INDICES 131072

struct sdf_font {
    GLuint texture;
    struct px_sdf_glyph* glyphs;
    uint16_t glyph_count;

    float ascent;
    float descent;
    float line_gap;
    float sdf_range;
};

struct ui_vertex {
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

enum ui_batch_type {
    UI_BATCH_PANEL,
    UI_BATCH_LINE,
    UI_BATCH_TEXT
};

enum batch_3d_type {
    BATCH_3D_SIMPLE,
    BATCH_3D_LINES
};

struct ui_batch {
    enum ui_batch_type type;
    int vertex_offset;
    int vertex_count;

    struct ui_vertex* vertices;

    PX_Scale2 size;
    PX_Scale2 texel_size;
    float noise;
    float corner_radius;
    GLuint texture;
    float text_sdf_width;
    float text_pixel_height;
    float text_outline_width;
    PX_Color4 text_outline_color;
};

struct batch_3d {
    enum batch_3d_type type;
    GLuint fbo;
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

struct ui_renderer {
    int initialized;

    unsigned int program;
    unsigned int text_program;

    unsigned int vbo;
    unsigned int vao;
    unsigned int ebo;
    
    // Core UI Programs
    int attr_pos;
    int attr_uv;
    int attr_color;
    int uni_projection;
    int uni_size;
    int uni_noise;
    int uni_corner_radius;
    int uni_texel_size;
    int uni_texture;
    // Text Programs
    int text_uni_projection;
    int text_uni_texture;
    int text_uni_sdf_width;
    int text_uni_pixel_height;
    int text_uni_outline_width;
    int text_uni_outline_color;
    int text_attr_pos;
    int text_attr_uv;
    int text_attr_color;

    GLuint blank_tex;

    struct ui_vertex* vertices;
    size_t vertex_count;
    size_t vertex_capacity;

    struct ui_batch batches[MAX_BATCHES];
    size_t batch_count;

    int screen_w;
    int screen_h;
};

struct renderer_3d {
    int initialized;

    unsigned int program;

    unsigned int vbo;
    unsigned int vao;
    unsigned int ebo;

    // Core 3D Programs
    int attr_pos;
    int attr_uv;
    int attr_normal;

    int uni_model;
    int uni_view;
    int uni_projection;
    int uni_color;
    int uni_texture;

    GLuint blank_tex;
    struct vertex_3d* vertices;
    size_t vertex_count;
    size_t vertex_capacity;

    uint32_t* indices;
    size_t index_count;
    size_t index_capacity;

    struct batch_3d batches[MAX_BATCHES];
    size_t batch_count;

    int screen_w;
    int screen_h;
    int screen_x;
    int screen_y;

    GLuint flatFBO;
    GLuint flatColorTex;
    GLuint flatDepthRBO;
};

struct scene_cam {
    vec3 position;
    vec3 target;
    vec3 up;

    float yaw;
    float pitch;

    float move_speed;
    float mouse_sens;
};

extern struct ui_renderer* gr_ui;
extern struct renderer_3d* gr_3d;

void px_rs_internal_push_batch_3d(struct batch_3d* b);
void px_rs_internal_push_batch_ui(struct ui_batch* b);