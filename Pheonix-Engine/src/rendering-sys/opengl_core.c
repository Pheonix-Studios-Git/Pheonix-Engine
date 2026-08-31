#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cglm/cglm.h>

#include <rendering-sys.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>
#include <decoders/unicode.h>
#include <event-sys.h>

#include <rendering-sys/opengl.h>
#include <rendering-sys/internal.h>

#define STB_IMAGE_IMPLEMENTATION
#include <external/stb_image.h> // Doesnt use it but implements it here

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

static struct ui_renderer gr_ui_b = {0};
struct ui_renderer* gr_gl_ui = &gr_ui_b;

static struct renderer_3d gr_3d_b = {0};
struct renderer_3d* gr_gl_3d = &gr_3d_b;

static PX_Transform3 opengl_combine_transform3(PX_Transform3 parent, PX_Transform3 local) {
    PX_Transform3 out = {0};

    out.scale.w = parent.scale.w * local.scale.w;
    out.scale.h = parent.scale.h * local.scale.h;
    out.scale.l = parent.scale.l * local.scale.l;

    versor p = {
        parent.rot.x,
        parent.rot.y,
        parent.rot.z,
        parent.rot.w
    };

    versor l = {
        local.rot.x,
        local.rot.y,
        local.rot.z,
        local.rot.w
    };

    versor r;
    glm_quat_mul(p, l, r);

    out.rot.x = r[0];
    out.rot.y = r[1];
    out.rot.z = r[2];
    out.rot.w = r[3];

    vec3 lp = {
        local.pos.x,
        local.pos.y,
        local.pos.z
    };

    lp[0] *= parent.scale.w;
    lp[1] *= parent.scale.h;
    lp[2] *= parent.scale.l;

    glm_quat_rotatev(p, lp, lp);

    out.pos.x = parent.pos.x + lp[0];
    out.pos.y = parent.pos.y + lp[1];
    out.pos.z = parent.pos.z + lp[2];

    return out;
}

static char* read_shader(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "shaders/%s", name);

    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* src = (char*)malloc(size + 1);
    if (!src) {
        fclose(f);
        return NULL;
    }

    fread(src, 1, size, f);
    src[size] = '\0';

    fclose(f);
    return src;
}

static unsigned int pxgl_compile_shader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile failed error:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static unsigned int pxgl_create_program(const char* vert, const char* frag) {
    char* vert_src = read_shader(vert);
    char* frag_src = read_shader(frag);

    if (!vert_src || !frag_src) {
        fprintf(stderr, "Failed to load shader files\n");
        if (vert_src)
            free(vert_src);
        if (frag_src)
            free(frag_src);
        return 0;
    }

    unsigned int vs = pxgl_compile_shader(GL_VERTEX_SHADER, vert_src);
    unsigned int fs = pxgl_compile_shader(GL_FRAGMENT_SHADER, frag_src);

    free(vert_src);
    free(frag_src);

    if (!vs || !fs)
        return 0;

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program Link Errror:\n%s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static void pxgl_ui_ortho(float left, float right, float bottom, float top, float* out_mat4) {
    memset(out_mat4, 0, sizeof(float) * 16);

    out_mat4[0] = 2.0f / (right - left);
    out_mat4[5] = -2.0f / (top - bottom);
    out_mat4[10] = -1.0f;
    out_mat4[12] = -(right + left) / (right - left);
    out_mat4[13] = (top + bottom) / (top - bottom);
    out_mat4[15] = 1.0f;
}

static void pxgl_ui_push_quad(PX_Vector2 pos, PX_Scale2 scale, PX_Color4 c) {
    if (gr_gl_ui->vertex_count + 6 > gr_gl_ui->vertex_capacity)
        return;

    struct ui_vertex* v = gr_gl_ui->vertices + gr_gl_ui->vertex_count;
    float x2 = (float)pos.x + (float)scale.w;
    float y2 = (float)pos.y + (float)scale.h;

    v[0] = (struct ui_vertex){(float)pos.x, (float)pos.y, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x2, (float)pos.y, 1, 0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x2, y2, 1, 1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){(float)pos.x, y2, 0, 1, c.r, c.g, c.b, c.a};
 
    gr_gl_ui->vertex_count += 4;
}

static void pxgl_ui_push_glyph(float x0, float y0, float x1, float y1, struct px_sdf_glyph* g, PX_Color4 c) {
    if (gr_gl_ui->vertex_count + 6 > gr_gl_ui->vertex_capacity)
        return;

    struct ui_vertex* v = gr_gl_ui->vertices + gr_gl_ui->vertex_count;

    v[0] = (struct ui_vertex){x0, y0, g->u0, g->v0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x1, y0, g->u1, g->v0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x1, y1, g->u1, g->v1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){x0, y1, g->u0, g->v1, c.r, c.g, c.b, c.a};

    gr_gl_ui->vertex_count += 4;
}

static void pxgl_ui_push_line(float x0, float y0, float x1, float y1, float thickness, PX_Color4 c) {
    if (gr_gl_ui->vertex_count + 6 > gr_gl_ui->vertex_capacity)
        return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx*dx + dy*dy);
    if (len == 0.0f) return;

    dx /= len; dy /= len;
    float px = -dy * thickness * 0.5f;
    float py =  dx * thickness * 0.5f;

    struct ui_vertex* v = gr_gl_ui->vertices + gr_gl_ui->vertex_count;
    v[0] = (struct ui_vertex){x0 + px, y0 + py, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x1 + px, y1 + py, 1, 0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x1 - px, y1 - py, 1, 1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){x0 - px, y0 - py, 0, 1, c.r, c.g, c.b, c.a};

    gr_gl_ui->vertex_count += 4;
}

static void pxgl_rs_internal_push_batch_ui(struct ui_batch* b) {
    if (gr_gl_ui->batch_count > 0) {
        struct ui_batch* last_b = &gr_gl_ui->batches[gr_gl_ui->batch_count - 1];

        if (last_b->type == b->type && last_b->texture == b->texture) {
            if (b->type == UI_BATCH_PANEL) {
                if (b->corner_radius == last_b->corner_radius && b->noise == last_b->noise) {
                    last_b->vertex_count += b->vertex_count;
                    return;
                }
            } else if (b->type == UI_BATCH_TEXT) {
                if (b->text_sdf_width == last_b->text_sdf_width && b->text_outline_width == last_b->text_outline_width &&
                    (b->text_outline_color.r == last_b->text_outline_color.r &&
                    b->text_outline_color.g == last_b->text_outline_color.g &&
                    b->text_outline_color.b == last_b->text_outline_color.b &&
                    b->text_outline_color.a == last_b->text_outline_color.a)
                ){
                    last_b->vertex_count += b->vertex_count;
                    return;
                }
            }
        }
    }

    if (gr_gl_ui->batch_count < MAX_BATCHES) {
        memcpy(&gr_gl_ui->batches[gr_gl_ui->batch_count++], b, sizeof(struct ui_batch));
    }
}

static void push_3d_grid(PX_EditorGrid* grid, PX_Transform3 transform, struct batch_3d* b) {
    memset(b, 0, sizeof(struct batch_3d));
    memcpy(&b->transform, &transform, sizeof(PX_Transform3));
    
    b->color = grid->color;
    b->line_width = 1.0f;

    b->type = BATCH_3D_LINES;
    b->vertex_offset = gr_gl_3d->vertex_count;
    b->index_offset = gr_gl_3d->index_count;

    const float spacing = grid->spacing;
    const float half = grid->half_size;

    float cam_x = floorf(gscene_cam.position[0] / spacing) * spacing;
    float cam_z = floorf(gscene_cam.position[2] / spacing) * spacing;

    float extent = half * spacing;

    for (float i = -half; i <= half; i++) {
        if (gr_gl_3d->vertex_count + 4 >= gr_gl_3d->vertex_capacity)
            break;

        if (gr_gl_3d->index_count + 4 >= gr_gl_3d->index_capacity)
            break;

        uint32_t base = (uint32_t)gr_gl_3d->vertex_count;

        float p = i * spacing;

        float x = cam_x + p;
        float z = cam_z + p;

        struct vertex_3d v0 = {
            .x = x,
            .y = -0.1f,
            .z = cam_z - extent
        };

        struct vertex_3d v1 = {
            .x = x,
            .y = 0.0f,
            .z = cam_z + extent
        };

        struct vertex_3d v2 = {
            .x = cam_x - extent,
            .y = 0.0f,
            .z = z
        };

        struct vertex_3d v3 = {
            .x = cam_x + extent,
            .y = 0.0f,
            .z = z
        };

        gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v0;
        gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v1;
        gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v2;
        gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v3;

        gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 0;
        gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 1;

        gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 2;
        gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 3;
    }

    b->vertex_count = gr_gl_3d->vertex_count - b->vertex_offset;
    b->index_count = gr_gl_3d->index_count - b->index_offset;
}

static void push_3d_line(PX_Color4 color, PX_Transform3 transform, PX_Vector3 endpos, float width, struct batch_3d* b) {
    memset(b, 0, sizeof(struct batch_3d));
   
    if (gr_gl_3d->vertex_count + 2 >= gr_gl_3d->vertex_capacity)
        return;

    if (gr_gl_3d->index_count + 2 >= gr_gl_3d->index_capacity)
        return;
   
    memcpy(&b->transform, &transform, sizeof(PX_Transform3));
    
    b->color = color;

    b->type = BATCH_3D_LINES;
    b->line_width = width;
    b->vertex_offset = gr_gl_3d->vertex_count;
    b->index_offset = gr_gl_3d->index_count;

    uint32_t base = (uint32_t)gr_gl_3d->vertex_count;

    struct vertex_3d v0 = {
        .x = transform.pos.x,
        .y = transform.pos.y,
        .z = transform.pos.z
    };

    struct vertex_3d v1 = {
        .x = endpos.x,
        .y = endpos.y,
        .z = endpos.z
    };

    gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v0;
    gr_gl_3d->vertices[gr_gl_3d->vertex_count++] = v1;

    gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 0;
    gr_gl_3d->indices[gr_gl_3d->index_count++] = base + 1;

    b->vertex_count = gr_gl_3d->vertex_count - b->vertex_offset;
    b->index_count = gr_gl_3d->index_count - b->index_offset;
}

static void pxgl_rs_internal_push_batch_3d(struct batch_3d* b) {
    if (gr_gl_3d->batch_count >= MAX_BATCHES)
        return;

    memcpy(&gr_gl_3d->batches[gr_gl_3d->batch_count], b, sizeof(struct batch_3d));
    gr_gl_3d->batch_count++;
}

t_err_codes px_rs_init_gl(void) {
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return ERR_GL_GLEW_INIT_FAILED;
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_init_gl_ui(PX_Scale2 screen_scale) {
    memset(gr_gl_ui, 0, sizeof(*gr_gl_ui));

    gr_gl_ui->program = pxgl_create_program("ui_vertex.glsl", "ui_fragment.glsl");
    if (gr_gl_ui->program == 0)
        return ERR_GL_PROGRAM_CREATION_FAILED;
    gr_gl_ui->text_program = pxgl_create_program("ui_vertex.glsl", "ui_textfrag.glsl");
    if (gr_gl_ui->text_program == 0) {
        glDeleteProgram(gr_gl_ui->program);
        return ERR_GL_PROGRAM_CREATION_FAILED;
    } 

    // Core UI Programs
    gr_gl_ui->uni_projection = glGetUniformLocation(gr_gl_ui->program, "u_projection");
    gr_gl_ui->uni_size = glGetUniformLocation(gr_gl_ui->program, "u_size");
    gr_gl_ui->uni_corner_radius = glGetUniformLocation(gr_gl_ui->program, "u_corner_radius");
    gr_gl_ui->uni_noise = glGetUniformLocation(gr_gl_ui->program, "u_noise");
    gr_gl_ui->uni_texel_size = glGetUniformLocation(gr_gl_ui->program, "u_texel_size");
    gr_gl_ui->uni_texture = glGetUniformLocation(gr_gl_ui->program, "u_texture");
    gr_gl_ui->attr_pos = glGetAttribLocation(gr_gl_ui->program, "a_pos");
    gr_gl_ui->attr_uv = glGetAttribLocation(gr_gl_ui->program, "a_uv");
    gr_gl_ui->attr_color = glGetAttribLocation(gr_gl_ui->program, "a_color");
    // Text Programs
    gr_gl_ui->text_uni_projection = glGetUniformLocation(gr_gl_ui->text_program, "u_projection");
    gr_gl_ui->text_uni_texture = glGetUniformLocation(gr_gl_ui->text_program, "u_font_text");
    gr_gl_ui->text_uni_sdf_width = glGetUniformLocation(gr_gl_ui->text_program, "u_sdf_width");
    gr_gl_ui->text_uni_pixel_height = glGetUniformLocation(gr_gl_ui->text_program, "u_pixel_height");
    gr_gl_ui->text_uni_outline_width = glGetUniformLocation(gr_gl_ui->text_program, "u_outline_width");
    gr_gl_ui->text_uni_outline_color = glGetUniformLocation(gr_gl_ui->text_program, "u_outline_color");
    gr_gl_ui->text_attr_pos = glGetAttribLocation(gr_gl_ui->text_program, "a_pos");
    gr_gl_ui->text_attr_uv = glGetAttribLocation(gr_gl_ui->text_program, "a_uv");
    gr_gl_ui->text_attr_color = glGetAttribLocation(gr_gl_ui->text_program, "a_color");

    // Textures Pre-made
    uint8_t white_pixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    glGenTextures(1, &gr_gl_ui->blank_tex);
    glBindTexture(GL_TEXTURE_2D, gr_gl_ui->blank_tex);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        1, 1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        white_pixel
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &gr_gl_ui->vao);
    glGenBuffers(1, &gr_gl_ui->vbo);
    glGenBuffers(1, &gr_gl_ui->ebo);

    glBindVertexArray(gr_gl_ui->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_ui->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_ui->ebo);

    unsigned short indices[MAX_VERTEX_COUNT / 4 * 6];
    for (size_t i = 0, v = 0; i < (MAX_VERTEX_COUNT / 4 * 6); i += 6, v += 4) {
        indices[i + 0] = v + 0; indices[i + 1] = v + 1; indices[i + 2] = v + 2;
        indices[i + 3] = v + 2; indices[i + 4] = v + 3; indices[i + 5] = v + 0;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    gr_gl_ui->vertex_capacity = MAX_VERTEX_COUNT;
    gr_gl_ui->vertices = (struct ui_vertex*)malloc(sizeof(struct ui_vertex) * gr_gl_ui->vertex_capacity);
    if (!gr_gl_ui->vertices) {
        return ERR_ALLOC_FAILED;
    }

    gr_gl_ui->screen_w = screen_scale.w;
    gr_gl_ui->screen_h = screen_scale.h;
    gr_gl_ui->initialized = true;

    glViewport(0, 0, screen_scale.w, screen_scale.h);

    return ERR_SUCCESS;
}

t_err_codes px_rs_init_gl_3d(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
    gscene_cam.position[0] = 8.0f;
    gscene_cam.position[1] = 8.0f;
    gscene_cam.position[2] = 8.0f;

    gscene_cam.target[0] = 0.0f;
    gscene_cam.target[1] = 0.0f;
    gscene_cam.target[2] = 0.0f;

    gscene_cam.up[0] = 0.0f;
    gscene_cam.up[1] = 1.0f;
    gscene_cam.up[2] = 0.0f;

    gscene_cam.yaw = -90.0f;
    gscene_cam.pitch = -25.0f;

    gscene_cam.move_speed = 0.1f;
    gscene_cam.mouse_sens = 0.1f;

    memset(gr_gl_3d, 0, sizeof(*gr_gl_3d));

    gr_gl_3d->program = pxgl_create_program("3d_vertex.glsl", "3d_fragment.glsl");
    if (gr_gl_3d->program == 0)
        return ERR_GL_PROGRAM_CREATION_FAILED;

    // Core 3D Programs
    gr_gl_3d->attr_pos = glGetAttribLocation(gr_gl_3d->program, "a_pos");
    gr_gl_3d->attr_uv = glGetAttribLocation(gr_gl_3d->program, "a_uv");
    gr_gl_3d->attr_normal = glGetAttribLocation(gr_gl_3d->program, "a_normal");

    gr_gl_3d->uni_model = glGetUniformLocation(gr_gl_3d->program, "u_model");
    gr_gl_3d->uni_view = glGetUniformLocation(gr_gl_3d->program, "u_view");
    gr_gl_3d->uni_projection = glGetUniformLocation(gr_gl_3d->program, "u_projection");
    gr_gl_3d->uni_color = glGetUniformLocation(gr_gl_3d->program, "u_color");
    gr_gl_3d->uni_texture = glGetUniformLocation(gr_gl_3d->program, "u_texture");

    // Textures Pre-made
    uint8_t white_pixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    glGenTextures(1, &gr_gl_3d->blank_tex);
    glBindTexture(GL_TEXTURE_2D, gr_gl_3d->blank_tex);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        1, 1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        white_pixel
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &gr_gl_3d->flatFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gr_gl_3d->flatFBO);

    glGenTextures(1, &gr_gl_3d->flatColorTex);
    glBindTexture(GL_TEXTURE_2D, gr_gl_3d->flatColorTex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        screen_scale.w,
        screen_scale.h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        gr_gl_3d->flatColorTex,
        0
    );

    glGenRenderbuffers(1, &gr_gl_3d->flatDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gr_gl_3d->flatDepthRBO);

    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        screen_scale.w,
        screen_scale.h
    );

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        gr_gl_3d->flatDepthRBO
    );

    GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, buffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Error: FBO creation failed: 0x%x\n", status);
        px_rs_gl_shutdown();
        return ERR_INTERNAL;
    }

    glViewport(screen_pos.x, screen_pos.y, screen_scale.w, screen_scale.h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &gr_gl_3d->vao);
    glGenBuffers(1, &gr_gl_3d->vbo);
    glGenBuffers(1, &gr_gl_3d->ebo);

    glBindVertexArray(gr_gl_3d->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);

    gr_gl_3d->vertex_capacity = MAX_VERTEX_COUNT;
    gr_gl_3d->vertices = (struct vertex_3d*)malloc(sizeof(struct vertex_3d) * gr_gl_3d->vertex_capacity);
    if (!gr_gl_3d->vertices) {
        return ERR_ALLOC_FAILED;
    }

    gr_gl_3d->index_capacity = MAX_3D_INDICES;
    gr_gl_3d->indices = (uint32_t*)malloc(sizeof(uint32_t) * gr_gl_3d->index_capacity);
    if (!gr_gl_3d->indices) {
        free(gr_gl_3d->vertices);
        return ERR_ALLOC_FAILED;
    }

    gr_gl_3d->screen_w = screen_scale.w;
    gr_gl_3d->screen_h = screen_scale.h;
    gr_gl_3d->screen_x = screen_pos.x;
    gr_gl_3d->screen_y = screen_pos.y;
    gr_gl_3d->initialized = true;

    glViewport(screen_pos.x, screen_pos.y, screen_scale.w, screen_scale.h);

    return ERR_SUCCESS;
}

void px_rs_gl_shutdown_ui(void) {
    if (!gr_gl_ui->initialized)
        return;

    if (gr_gl_ui->blank_tex) glDeleteTextures(1, &gr_gl_ui->blank_tex);

    if (gr_gl_ui->vbo) glDeleteBuffers(1, &gr_gl_ui->vbo);
    if (gr_gl_ui->vao) glDeleteVertexArrays(1, &gr_gl_ui->vao);
    if (gr_gl_ui->ebo) glDeleteBuffers(1, &gr_gl_ui->ebo);
    if (gr_gl_ui->program) glDeleteProgram(gr_gl_ui->program);
    if (gr_gl_ui->text_program) glDeleteProgram(gr_gl_ui->text_program);

    if (gr_gl_ui->vertices) free(gr_gl_ui->vertices);
    memset(gr_gl_ui, 0, sizeof(*gr_gl_ui));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void px_rs_gl_shutdown_3d(void) {
    if (!gr_gl_3d->initialized)
        return;

    if (gr_gl_3d->blank_tex) glDeleteTextures(1, &gr_gl_3d->blank_tex);

    if (gr_gl_3d->vbo) glDeleteBuffers(1, &gr_gl_3d->vbo);
    if (gr_gl_3d->vao) glDeleteVertexArrays(1, &gr_gl_3d->vao);
    if (gr_gl_3d->ebo) glDeleteBuffers(1, &gr_gl_3d->ebo);
    if (gr_gl_3d->program) glDeleteProgram(gr_gl_3d->program);

    if (gr_gl_3d->flatFBO) glDeleteFramebuffers(1, &gr_gl_3d->flatFBO);
    if (gr_gl_3d->flatColorTex) glDeleteTextures(1, &gr_gl_3d->flatColorTex);
    if (gr_gl_3d->flatDepthRBO) glDeleteRenderbuffers(1, &gr_gl_3d->flatDepthRBO);

    if (gr_gl_3d->vertices) free(gr_gl_3d->vertices);
    if (gr_gl_3d->indices) free(gr_gl_3d->indices);
    memset(gr_gl_3d, 0, sizeof(*gr_gl_3d));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void px_rs_gl_shutdown(void) {
    px_rs_gl_shutdown_ui();
    px_rs_gl_shutdown_3d();
}

void px_rs_gl_frame_start(void) {
    gr_gl_ui->vertex_count = 0;
    gr_gl_ui->batch_count = 0;
    
    gr_gl_3d->vertex_count = 0;
    gr_gl_3d->index_count = 0;
    gr_gl_3d->batch_count = 0;
}

void px_rs_gl_ui_frame_end(void) {
    if (gr_gl_ui->vertex_count <= 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_ui->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct ui_vertex) * gr_gl_ui->vertex_count,
        gr_gl_ui->vertices,
        GL_DYNAMIC_DRAW
    );

    float proj[16];
    pxgl_ui_ortho(0.0f, (float)gr_gl_ui->screen_w, 0.0f, (float)gr_gl_ui->screen_h, proj);

    glBindVertexArray(gr_gl_ui->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_ui->ebo);

    unsigned int old_program = 0;
    for (size_t i = 0; i < gr_gl_ui->batch_count; i++) {
        struct ui_batch* b = &gr_gl_ui->batches[i];
        if (b->vertex_count <= 0) continue;

        unsigned int program = 0;
        int attr_pos = 0;
        int attr_uv = 0;
        int attr_color = 0;

        if (b->type == UI_BATCH_TEXT)
            program = gr_gl_ui->text_program;
        else
            program = gr_gl_ui->program;
        
        if (program != old_program) glUseProgram(program);

        switch (b->type) {
            case UI_BATCH_PANEL: {
                glUniformMatrix4fv(gr_gl_ui->uni_projection, 1, GL_FALSE, proj);
                glUniform2f(gr_gl_ui->uni_size, b->size.w, b->size.h);
                glUniform2f(gr_gl_ui->uni_texel_size, b->texel_size.w, b->texel_size.h);
                glUniform1f(gr_gl_ui->uni_corner_radius, b->corner_radius);
                glUniform1f(gr_gl_ui->uni_noise, b->noise);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
                glUniform1i(gr_gl_ui->uni_texture, 0);

                attr_pos = gr_gl_ui->attr_pos;
                attr_uv = gr_gl_ui->attr_uv;
                attr_color = gr_gl_ui->attr_color;
                break;
            }
            case UI_BATCH_TEXT: {
                glUniformMatrix4fv(gr_gl_ui->text_uni_projection, 1, GL_FALSE, proj);
                glUniform1f(gr_gl_ui->text_uni_sdf_width, b->text_sdf_width);
                glUniform1f(gr_gl_ui->text_uni_pixel_height, b->text_pixel_height);
                glUniform1f(gr_gl_ui->text_uni_outline_width, b->text_outline_width);
                glUniform4f(gr_gl_ui->text_uni_outline_color, (float)b->text_outline_color.r, (float)b->text_outline_color.g, (float)b->text_outline_color.b, (float)b->text_outline_color.a);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
                glUniform1i(gr_gl_ui->text_uni_texture, 0);

                attr_pos = gr_gl_ui->text_attr_pos;
                attr_uv = gr_gl_ui->text_attr_uv;
                attr_color = gr_gl_ui->text_attr_color;
                break;
            }
            case UI_BATCH_LINE: {
                glUniformMatrix4fv(gr_gl_ui->uni_projection, 1, GL_FALSE, proj);
                glUniform2f(gr_gl_ui->uni_size, 0, 0);
                glUniform2f(gr_gl_ui->uni_texel_size, 1, 1);
                glUniform1f(gr_gl_ui->uni_corner_radius, 0.0f);
                glUniform1f(gr_gl_ui->uni_noise, 0.0f);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
                glUniform1i(gr_gl_ui->uni_texture, 0);

                attr_pos = gr_gl_ui->attr_pos;
                attr_uv = gr_gl_ui->attr_uv;
                attr_color = gr_gl_ui->attr_color;
                break;
            }
            default: continue;
        }

        uintptr_t base_offset = (uintptr_t)b->vertex_offset * sizeof(struct ui_vertex);
        int stride = sizeof(struct ui_vertex);

        glEnableVertexAttribArray(attr_pos);
        glEnableVertexAttribArray(attr_uv);
        glEnableVertexAttribArray(attr_color);

        glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, stride, (void*)(base_offset + offsetof(struct ui_vertex, x)));
        glVertexAttribPointer(attr_uv, 2, GL_FLOAT, GL_FALSE, stride, (void*)(base_offset + offsetof(struct ui_vertex, u)));
        glVertexAttribPointer(attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)(base_offset + offsetof(struct ui_vertex, r)));

        int index_count = (b->vertex_count / 4) * 6;
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, (void*)0);

        glDisableVertexAttribArray(attr_pos);
        glDisableVertexAttribArray(attr_uv);
        glDisableVertexAttribArray(attr_color);

        old_program = program;
    }

    glBindVertexArray(0);
}

void px_rs_gl_3d_frame_end(void) {
    if (gr_gl_3d->vertex_count <= 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct vertex_3d) * gr_gl_3d->vertex_count,
        gr_gl_3d->vertices,
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(uint32_t) * gr_gl_3d->index_count,
        gr_gl_3d->indices,
        GL_DYNAMIC_DRAW
    );

    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    glm_perspective(glm_rad(70.0f), (float)gr_gl_3d->screen_w / (float)gr_gl_3d->screen_h, 0.1f, 1000.0f, proj);
    
    vec3 forward;
    forward[0] = cos(glm_rad(gscene_cam.yaw)) * cos(glm_rad(gscene_cam.pitch));
    forward[1] = sin(glm_rad(gscene_cam.pitch));
    forward[2] = sin(glm_rad(gscene_cam.yaw)) * cos(glm_rad(gscene_cam.pitch));
    glm_normalize(forward);

    vec3 target;
    glm_vec3_add(gscene_cam.position, forward, target);
    glm_lookat(gscene_cam.position, target, gscene_cam.up, view);

    glBindVertexArray(gr_gl_3d->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);

    unsigned int old_program = 0;
    
    float oldWidth = 1.0f;
    glLineWidth(oldWidth);
    
    bool depth_enabled = true;
    GLuint curFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&curFBO);
    bool pure_color_enabled = false;

    for (size_t i = 0; i < gr_gl_3d->batch_count; i++) {
        struct batch_3d* b = &gr_gl_3d->batches[i];
        if (b->vertex_count <= 0) continue;

        unsigned int program = 0;
        int attr_pos = 0;
        int attr_uv = 0;
        int attr_normal = 0;

        program = gr_gl_3d->program;
        if (old_program != program) glUseProgram(program);

        mat4 model = GLM_MAT4_IDENTITY_INIT;
        mat4 temp = GLM_MAT4_IDENTITY_INIT;
        mat4 translation = GLM_MAT4_IDENTITY_INIT;
        mat4 rotation = GLM_MAT4_IDENTITY_INIT;
        mat4 scaling = GLM_MAT4_IDENTITY_INIT;

        vec3 pos = {
            b->transform.pos.x,
            b->transform.pos.y,
            b->transform.pos.z
        };
        vec3 scl = {
            b->transform.scale.w,
            b->transform.scale.h,
            b->transform.scale.l
        };
        versor rot = {
            b->transform.rot.x,
            b->transform.rot.y,
            b->transform.rot.z,
            b->transform.rot.w
        };
        glm_translate(translation, pos);
        glm_quat_mat4(rot, rotation);
        glm_scale(scaling, scl);
        glm_mat4_mul(translation, rotation, temp);
        glm_mat4_mul(temp, scaling, model);

        if (b->depth_override) {
            depth_enabled = false;
            glDisable(GL_DEPTH_TEST);
        } else if (!depth_enabled) {
            depth_enabled = true;
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
        }

        if (b->switch_fbo && curFBO != (GLuint)b->fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)b->fbo);
            glViewport(b->fbo_x, b->fbo_y, b->fbo_w, b->fbo_h);
			GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    		glDrawBuffers(1, &drawBuf);
            curFBO = (GLuint)b->fbo;
        } else if (!b->switch_fbo && curFBO != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(gr_gl_3d->screen_x, gr_gl_3d->screen_y, gr_gl_3d->screen_w, gr_gl_3d->screen_h);
            curFBO = 0;
        }

        if (b->pure_color) {
            glDisable(GL_BLEND);
            glDisable(GL_DITHER);
            glDisable(GL_MULTISAMPLE);
            glDisable(GL_LINE_SMOOTH);
            pure_color_enabled = true;
        } else if (pure_color_enabled) {
            glEnable(GL_BLEND);
            glEnable(GL_DITHER);
            glEnable(GL_MULTISAMPLE);
            glEnable(GL_LINE_SMOOTH);
            pure_color_enabled = false;
        }

        switch (b->type) {
            case BATCH_3D_SIMPLE: {
                glUniformMatrix4fv(gr_gl_3d->uni_projection, 1, GL_FALSE, (float*)proj);
                glUniformMatrix4fv(gr_gl_3d->uni_view, 1, GL_FALSE, (float*)view);
                glUniformMatrix4fv(gr_gl_3d->uni_model, 1, GL_FALSE, (float*)model);
                
                glUniform4f(gr_gl_3d->uni_color, b->color.r / 255.0f, b->color.g / 255.0f, b->color.b / 255.0f, b->color.a / 255.0f);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gr_gl_3d->blank_tex);
                glUniform1i(gr_gl_3d->uni_texture, 0);

                attr_pos = gr_gl_3d->attr_pos;
                attr_uv = gr_gl_3d->attr_uv;
                attr_normal = gr_gl_3d->attr_normal;
                break;
            }
            case BATCH_3D_LINES: {
                if (b->line_width != oldWidth) {
                    glLineWidth(b->line_width);
                    oldWidth = b->line_width;
                }

                glUniformMatrix4fv(gr_gl_3d->uni_projection, 1, GL_FALSE, (float*)proj);
                glUniformMatrix4fv(gr_gl_3d->uni_view, 1, GL_FALSE, (float*)view);
                glUniformMatrix4fv(gr_gl_3d->uni_model, 1, GL_FALSE, (float*)model);

                glUniform4f(gr_gl_3d->uni_color, b->color.r / 255.0f, b->color.g / 255.0f, b->color.b / 255.0f, b->color.a / 255.0f);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, gr_gl_3d->blank_tex);
                glUniform1i(gr_gl_3d->uni_texture, 0);

                attr_pos = gr_gl_3d->attr_pos;
                attr_uv = gr_gl_3d->attr_uv;
                attr_normal = gr_gl_3d->attr_normal;

                break;
            }
            default: continue;
        }

        int stride = sizeof(struct vertex_3d);

        glEnableVertexAttribArray(attr_pos);
        glEnableVertexAttribArray(attr_uv);
        glEnableVertexAttribArray(attr_normal);

        glVertexAttribPointer(attr_pos, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(struct vertex_3d, x)));
        glVertexAttribPointer(attr_uv, 2, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(struct vertex_3d, u)));
        glVertexAttribPointer(attr_normal, 3, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(struct vertex_3d, nx)));

        GLenum mode = (b->type == BATCH_3D_LINES) ? GL_LINES : GL_TRIANGLES;

        glDrawElements(
            mode,
            b->index_count,
            GL_UNSIGNED_INT,
            (void*)(b->index_offset * sizeof(uint32_t))
        );

        glDisableVertexAttribArray(attr_pos);
        glDisableVertexAttribArray(attr_uv);
        glDisableVertexAttribArray(attr_normal);

        old_program = program;
    }

    glBindVertexArray(0);
}

void px_rs_gl_frame_end(void) {
    px_rs_gl_3d_frame_update();
    px_rs_gl_3d_frame_end();

    px_rs_gl_ui_frame_update();
    px_rs_gl_ui_frame_end();
}

t_err_codes px_rs_gl_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius) {
    int start_vertex = gr_gl_ui->vertex_count;
    pxgl_ui_push_quad(tran.pos, tran.scale, color);
    int vertex_count = gr_gl_ui->vertex_count - start_vertex;

    struct ui_batch b = {0};
    b.type = UI_BATCH_PANEL;
    b.size = tran.scale;
    b.texel_size = (PX_Scale2){1, 1};
    b.corner_radius = cradius;
    b.noise = noise;
    b.texture = (PheonixEngine_GPU_Handle)gr_gl_ui->blank_tex;
    b.vertex_count = vertex_count;
    b.vertex_offset = start_vertex;

    pxgl_rs_internal_push_batch_ui(&b);

    return ERR_SUCCESS;
}

int px_rs_gl_text_width(PX_Font* font, const char* text, float pixel_height) {
    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));
    float pen_x = 0.0f;

    for (const char* p = text; *p; ) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        pen_x += g->advance * scale;
    }

    return (int)(pen_x + 0.5f);
}

t_err_codes px_rs_gl_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) {
    int start_vertex = gr_gl_ui->vertex_count;
    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));

    float pen_x = pos.x;
    float pen_y = pos.y + px_sdf_ascent(font) * scale;

    for (const char* p = text; *p;) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        float y1 = pen_y - g->bearing_y * scale;
        float y0 = y1 + g->height * scale;
        float x0 = pen_x + g->bearing_x * scale;
        float x1 = x0 + g->width * scale;

        pxgl_ui_push_glyph(
            x0,
            y0,
            x1,
            y1,
            (struct px_sdf_glyph*)g,
            color
        );

        pen_x += g->advance * scale;
    }
    int vertex_count = gr_gl_ui->vertex_count - start_vertex;

    float sdf_width = px_sdf_range(font) / pixel_height;
    sdf_width = fmaxf(0.015f, fminf(sdf_width, 0.03));

    struct ui_batch b = {0};
    b.type = UI_BATCH_TEXT;
    b.text_sdf_width = sdf_width;
    b.text_pixel_height = pixel_height;
    b.text_outline_width = sdf_width * 2.0f;
    b.text_outline_color = (PX_Color4){0x00, 0x00, 0x00, 0xFF};
    b.texture = px_sdf_get_texture(font);
    b.vertex_count = vertex_count;
    b.vertex_offset = start_vertex;

    pxgl_rs_internal_push_batch_ui(&b);

    return ERR_SUCCESS;
}

t_err_codes px_rs_gl_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) {
    int start_vertex = gr_gl_ui->vertex_count;
    pxgl_ui_push_line(start.x, start.y, end.x, end.y, thickness, color);
    int vertex_count = gr_gl_ui->vertex_count - start_vertex;

    struct ui_batch b = {0};
    b.type = UI_BATCH_LINE;
    b.size = (PX_Scale2){0,0};
    b.texel_size = (PX_Scale2){1,1};
    b.texture = (PheonixEngine_GPU_Handle)gr_gl_ui->blank_tex;
    b.noise = 0.0f;
    b.corner_radius = 0.0f;
    b.vertex_offset = start_vertex;
    b.vertex_count = vertex_count;

    pxgl_rs_internal_push_batch_ui(&b);
    return ERR_SUCCESS;
}

t_err_codes px_rs_gl_draw_dropdown(PX_Dropdown* dd) {
    PX_Color4 color = dd->color; 
    px_rs_gl_draw_panel((PX_Transform2){dd->pos, (PX_Scale2){dd->width, dd->height}}, color, dd->noise, dd->cradius);

    int x = dd->stext_pos.x;
    for (int i = 0; i < dd->item_count; i++) {
        PX_DropdownItem* item = &dd->items[i];

        PX_Color4 tcolor = dd->hover_index == i ? dd->hover_color : dd->text_color;
        px_rs_gl_render_text(item->label, dd->font_size, (PX_Vector2){x, dd->stext_pos.y}, tcolor, dd->font);

        x += dd->spacing + px_rs_gl_text_width(dd->font, item->label, dd->font_size);

        if (item->is_open) {
            px_rs_gl_draw_panel(item->panel_tran, item->panel_color, item->panel_noise, item->panel_cradius);

            int y = item->stext_pos.y;
            for (int j = 0; j < item->option_count; j++) {
                PX_DropdownOption* option = &item->options[j];

                PX_Color4 ptcolor = item->hover_index == j ? item->hover_color : item->text_color;
                px_rs_gl_render_text(option->label, item->font_size, (PX_Vector2){item->stext_pos.x, y}, ptcolor, dd->font);

                y += item->spacing;
            }
        }
    }

    return ERR_SUCCESS;
}

void px_rs_gl_ui_frame_update(void) {
    if (!gr_gl_ui->initialized)
        return;

    glViewport(0, 0, gr_gl_ui->screen_w, gr_gl_ui->screen_h);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void px_rs_gl_3d_frame_update(void) {
    if (!gr_gl_3d->initialized)
        return;

    glViewport(gr_gl_3d->screen_x, gr_gl_3d->screen_y, gr_gl_3d->screen_w, gr_gl_3d->screen_h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, gr_gl_3d->flatFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void px_rs_gl_frame_update(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("Got GL Error (Caught on Frame Update): %02X\n", err);
	}
}

void px_rs_gl_ui_resize(PX_Scale2 screen_scale) {
    gr_gl_ui->screen_w = screen_scale.w;
    gr_gl_ui->screen_h = screen_scale.h;
    glViewport(0, 0, screen_scale.w, screen_scale.h);
}

void px_rs_gl_3d_resize(PX_Scale2 screen_scale, PX_Vector2 screen_pos) {
    gr_gl_3d->screen_w = screen_scale.w;
    gr_gl_3d->screen_h = screen_scale.h;
    gr_gl_3d->screen_x = screen_pos.x;
    gr_gl_3d->screen_y = screen_pos.y;
    glViewport(screen_pos.x, screen_pos.y, screen_scale.w, screen_scale.h);
}

t_err_codes px_rs_gl_draw_editor_objects(PX_Scene* scene) {
    for (size_t i = 0; i < scene->editor_object_count; i++) {
        struct batch_3d batch = {0};
        batch.depth_override = false;
        batch.switch_fbo = false;
        batch.pure_color = false;

        PX_3D_Editor_Object* obj = &scene->editor_objects[i];
        if (!obj->active) continue;
        PX_Transform3 localT = obj->local_transform;
        PX_Transform3 worldT = obj->world_transform;
        PX_Transform3 finalT = opengl_combine_transform3(worldT, localT);

        switch (obj->type){
            case OBJECT_3D_EDITOR_GRID: {
                if (!obj->ex_data) continue;
                push_3d_grid(obj->ex_data, worldT, &batch);
				pxgl_rs_internal_push_batch_3d(&batch);
                break;
            }
            case OBJECT_3D_EDITOR_GIZMO: {
                PX_Vector3 GXep = (PX_Vector3){finalT.pos.x + 5, finalT.pos.y, finalT.pos.z};
                PX_Vector3 GYep = (PX_Vector3){finalT.pos.x, finalT.pos.y + 5, finalT.pos.z};
                PX_Vector3 GZep = (PX_Vector3){finalT.pos.x, finalT.pos.y, finalT.pos.z + 5};
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x75,0x0,0x0,0xFF}, finalT, GXep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0xFF,0x0,0x0,0xFF}, finalT, GXep, 4.0f, &batch);
                batch.depth_override = true;
                pxgl_rs_internal_push_batch_3d(&batch);
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x0,0x75,0x0,0xFF}, finalT, GYep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0x0,0xFF,0x0,0xFF}, finalT, GYep, 4.0f, &batch);
                batch.depth_override = true;
                pxgl_rs_internal_push_batch_3d(&batch);
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x0,0x0,0x75,0xFF}, finalT, GZep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0x0,0x0,0xFF,0xFF}, finalT, GZep, 4.0f, &batch);
                batch.depth_override = true;
                pxgl_rs_internal_push_batch_3d(&batch);
                
                // Picker
                PX_Color3 main_color = {
                    .r = 0xFF, //obj->id & 0xFF,
                    .g = (obj->id >> 8) & 0xFF,
                    .b = obj->type
                };
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GXep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PheonixEngine_GPU_Handle)gr_gl_3d->flatFBO;
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = gr_gl_3d->screen_w;
                batch.fbo_h = gr_gl_3d->screen_h;
                pxgl_rs_internal_push_batch_3d(&batch);
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GYep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PheonixEngine_GPU_Handle)gr_gl_3d->flatFBO;
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = gr_gl_3d->screen_w;
                batch.fbo_h = gr_gl_3d->screen_h;
                pxgl_rs_internal_push_batch_3d(&batch);
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GZep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PheonixEngine_GPU_Handle)gr_gl_3d->flatFBO;
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = gr_gl_3d->screen_w;
                batch.fbo_h = gr_gl_3d->screen_h;
                // Outer loop will push this
                break;
            }
            default: continue;
        }
        pxgl_rs_internal_push_batch_3d(&batch);
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_gl_draw_scene(PX_Scene* scene) {
    for (size_t i = 0; i < scene->object_count; i++) {
        PX_3D_Object* obj = &scene->objects[i];
        if (!obj->active) continue;
        PX_Transform3 localT = obj->local_transform;
        PX_Transform3 worldT = obj->world_transform;
        PX_Transform3 finalT = opengl_combine_transform3(worldT, localT);

        struct batch_3d batch = {0};
        batch.depth_override = false;
        batch.switch_fbo = false;
        batch.pure_color = false;

        switch (obj->type){
            case OBJECT_3D_TYPE_MESH: {
                if (!obj->ex_data) continue;
                struct batch_3d* b = (struct batch_3d*)obj->ex_data;
                if (gr_gl_3d->vertex_count + b->vertex_count > gr_gl_3d->vertex_capacity) continue; // Skip, too large
                if (gr_gl_3d->index_count + b->index_count > gr_gl_3d->index_capacity) continue; // Skip, too large

                memcpy(&batch, b, sizeof(struct batch_3d));

                batch.vertices = NULL;
                batch.indices = NULL;
                batch.transform = finalT;

                batch.vertex_offset = gr_gl_3d->vertex_count;
                memcpy(&gr_gl_3d->vertices[gr_gl_3d->vertex_count], b->vertices, sizeof(struct vertex_3d) * b->vertex_count);
                gr_gl_3d->vertex_count += b->vertex_count;

                batch.index_offset = gr_gl_3d->index_count;
                for (size_t j = 0; j < b->index_count; j++) {
                    gr_gl_3d->indices[gr_gl_3d->index_count + j] =  b->indices[j] + batch.vertex_offset;
                }
                gr_gl_3d->index_count += b->index_count;

                break;
            }
            default: continue;
        }
        pxgl_rs_internal_push_batch_3d(&batch);
    }
    return ERR_SUCCESS;
}

void px_rs_gl_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {
    int local_x = mpos.x - gr_gl_3d->screen_x;
	int local_y = mpos.y - gr_gl_3d->screen_y;

	if (
		local_x < 0 ||
		local_y < 0 ||
		local_x >= gr_gl_3d->screen_w ||
		local_y >= gr_gl_3d->screen_h
	) {
		return;
	}

	local_y = gr_gl_3d->screen_h - local_y - 1;

    uint8_t pixel[4];

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gr_gl_3d->flatFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(
        local_x,
        local_y,
        1,
        1,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixel
    );

    uint16_t id = (pixel[1] << 8) | pixel[0];
    uint8_t objType = pixel[2];
    uint8_t subId = pixel[3];

    if (subId <= 0) return; // Nothing there

    PX_Event_GSignal event = {
        .type=EVENT_GSIGNAL_3D_HOVER,
        .mouse_hover_on_3d = (PX_Event_GSignal_3dHover){
            .id = id,
            .subId = subId,
            .objType = objType
        }
    };

    event_send_gsignal(&event);
}
