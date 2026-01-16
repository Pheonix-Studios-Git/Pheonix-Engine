#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <rendering-sys.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>
#include <decoders/unicode.h>

#include <rendering-sys/opengl.h>

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

struct ui_renderer {
    int initialized;

    unsigned int program;
    unsigned int text_program;

    unsigned int vbo;
    unsigned int vao;
    
    // Core UI Programs
    int attr_pos;
    int attr_uv;
    int attr_color;
    int uni_projection;
    // Text Programs
    int text_uni_projection;
    int text_uni_texture;
    int text_uni_sdf_width;
    int text_uni_outline_width;
    int text_uni_outline_color;
    int text_attr_pos;
    int text_attr_uv;
    int text_attr_color;

    struct ui_vertex* vertices;
    int vertex_count;
    int vertex_capacity;

    int screen_w;
    int screen_h;
};

static struct ui_renderer gr_ui_b = {0};
static struct ui_renderer* gr_ui = &gr_ui_b;

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
    if (gr_ui->vertex_count + 6 > gr_ui->vertex_capacity)
        return;

    struct ui_vertex* v = gr_ui->vertices + gr_ui->vertex_count;
    float x2 = (float)pos.x + (float)scale.w;
    float y2 = (float)pos.y + (float)scale.h;

    v[0] = (struct ui_vertex){(float)pos.x, (float)pos.y, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x2, (float)pos.y, 1, 0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x2, y2, 1, 1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){(float)pos.x, (float)pos.y, 0, 0, c.r, c.g, c.b, c.a};
    v[4] = (struct ui_vertex){x2, y2, 1, 1, c.r, c.g, c.b, c.a};
    v[5] = (struct ui_vertex){(float)pos.x, y2, 0, 1, c.r, c.g, c.b, c.a};
 
    gr_ui->vertex_count += 6;
}

static void pxgl_ui_push_glyph(float x0, float y0, float x1, float y1, struct px_sdf_glyph* g, PX_Color4 c) {
    if (gr_ui->vertex_count + 6 > gr_ui->vertex_capacity)
        return;

    struct ui_vertex* v = gr_ui->vertices + gr_ui->vertex_count;

    v[0] = (struct ui_vertex){x0, y0, g->u0, 1.0f - g->v0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x1, y0, g->u1, 1.0f - g->v0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x1, y1, g->u1, 1.0f - g->v1, c.r, c.g, c.b, c.a};
    v[3] = v[0];
    v[4] = v[2];
    v[5] = (struct ui_vertex){x0, y1, g->u0, 1.0f - g->v1, c.r, c.g, c.b, c.a};

    gr_ui->vertex_count += 6;
}

t_err_codes px_rs_init_ui(PX_Scale2 screen_scale) {
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return ERR_GL_GLEW_INIT_FAILED;
    }

    memset(gr_ui, 0, sizeof(*gr_ui));

    gr_ui->program = pxgl_create_program("ui_vertex.glsl", "ui_fragment.glsl");
    if (gr_ui->program == 0)
        return ERR_GL_PROGRAM_CREATION_FAILED;
    gr_ui->text_program = pxgl_create_program("ui_vertex.glsl", "ui_textfrag.glsl");
    if (gr_ui->text_program == 0) {
        glDeleteProgram(gr_ui->program);
        return ERR_GL_PROGRAM_CREATION_FAILED;
    } 

    // Core UI Programs
    gr_ui->uni_projection = glGetUniformLocation(gr_ui->program, "u_projection");
    gr_ui->attr_pos = glGetAttribLocation(gr_ui->program, "a_pos");
    gr_ui->attr_uv = glGetAttribLocation(gr_ui->program, "a_uv");
    gr_ui->attr_color = glGetAttribLocation(gr_ui->program, "a_color");
    // Text Programs
    gr_ui->text_uni_projection = glGetUniformLocation(gr_ui->text_program, "u_projection");
    gr_ui->text_uni_texture = glGetUniformLocation(gr_ui->text_program, "u_font_text");
    gr_ui->text_uni_sdf_width = glGetUniformLocation(gr_ui->text_program, "u_sdf_width");
    gr_ui->text_uni_outline_width = glGetUniformLocation(gr_ui->text_program, "u_outline_width");
    gr_ui->text_uni_outline_color = glGetUniformLocation(gr_ui->text_program, "u_outline_color");
    gr_ui->text_attr_pos = glGetAttribLocation(gr_ui->text_program, "a_pos");
    gr_ui->text_attr_uv = glGetAttribLocation(gr_ui->text_program, "a_uv");
    gr_ui->text_attr_color = glGetAttribLocation(gr_ui->text_program, "a_color");

    glGenBuffers(1, &gr_ui->vbo);

    gr_ui->vertex_capacity = 8192;
    gr_ui->vertices = (struct ui_vertex*)malloc(sizeof(struct ui_vertex) * gr_ui->vertex_capacity);
    if (!gr_ui->vertices) {
        return ERR_ALLOC_FAILED;
    }

    gr_ui->screen_w = screen_scale.w;
    gr_ui->screen_h = screen_scale.h;
    gr_ui->initialized = true;

    glViewport(0, 0, screen_scale.w, screen_scale.h);

    return ERR_SUCCESS;
}

void px_rs_shutdown_ui(void) {
    if (!gr_ui->initialized)
        return;

    glDeleteBuffers(1, &gr_ui->vbo);
    glDeleteProgram(gr_ui->program);
    glDeleteProgram(gr_ui->text_program);

    free(gr_ui->vertices);
    memset(gr_ui, 0, sizeof(*gr_ui));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

t_err_codes px_rs_draw_panel(PX_Scale2 scale, PX_Vector2 pos, PX_Color4 color) {
    gr_ui->vertex_count = 0;
    pxgl_ui_push_quad(pos, scale, color);

    glUseProgram(gr_ui->program);
    float proj[16];
    pxgl_ui_ortho(0.0f, (float)gr_ui->screen_w, 0.0f, (float)gr_ui->screen_h, proj);
    glUniformMatrix4fv(gr_ui->uni_projection, 1, GL_FALSE, proj);

    glBindBuffer(GL_ARRAY_BUFFER, gr_ui->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct ui_vertex) * gr_ui->vertex_count,
        gr_ui->vertices,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(gr_ui->attr_pos);
    glEnableVertexAttribArray(gr_ui->attr_uv);
    glEnableVertexAttribArray(gr_ui->attr_color);

    glVertexAttribPointer(
        gr_ui->attr_pos, 2, GL_FLOAT, GL_FALSE,
        sizeof(struct ui_vertex), (void*)0
    );

    glVertexAttribPointer(
        gr_ui->attr_uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(struct ui_vertex), (void*)(sizeof(float) * 2)
    );
    
    glVertexAttribPointer(
        gr_ui->attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
        sizeof(struct ui_vertex), (void*)(sizeof(float) * 4)
    );

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, gr_ui->vertex_count);
    glDisableVertexAttribArray(gr_ui->attr_pos);
    glDisableVertexAttribArray(gr_ui->attr_uv);
    glDisableVertexAttribArray(gr_ui->attr_color);

    return ERR_SUCCESS;
}

t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) {
    gr_ui->vertex_count = 0;

    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));

    float pen_x = pos.x;
    float pen_y = pos.y + px_sdf_ascent(font) * scale;

    for (const char* p = text; *p; p++) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        pxgl_ui_push_glyph(
            pen_x + g->bearing_x * scale,
            pen_y - g->bearing_y * scale,
            pen_x + (g->bearing_x + g->width) * scale,
            pen_y + (g->height - g->bearing_y) * scale,
            (struct px_sdf_glyph*)g,
            color
        );

        pen_x += g->advance * scale;
    }

    glUseProgram(gr_ui->text_program);

    float proj[16];
    pxgl_ui_ortho(0.0f, gr_ui->screen_w, 0.0f, gr_ui->screen_h, proj);
    glUniformMatrix4fv(
        gr_ui->text_uni_projection,
        1, GL_FALSE, proj
    );

    float sdf_width = px_sdf_range(font) / pixel_height;
    sdf_width = fmaxf(0.02f, fminf(sdf_width, 0.05));
    glUniform1f(
        gr_ui->text_uni_sdf_width,
        sdf_width
    );

    glUniform1f(gr_ui->text_uni_outline_width, sdf_width * 2.0f);

    glUniform4f(
        gr_ui->text_uni_outline_color,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, px_sdf_gl_texture(font));
    glUniform1i(gr_ui->text_uni_texture, 0);

    glBindBuffer(GL_ARRAY_BUFFER, gr_ui->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct ui_vertex) * gr_ui->vertex_count,
        gr_ui->vertices,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(gr_ui->text_attr_pos);
    glEnableVertexAttribArray(gr_ui->text_attr_uv);
    glEnableVertexAttribArray(gr_ui->text_attr_color);

    glVertexAttribPointer(
        gr_ui->text_attr_pos, 2, GL_FLOAT, GL_FALSE,
        sizeof(struct ui_vertex), (void*)0
    );
    glVertexAttribPointer(
        gr_ui->text_attr_uv, 2, GL_FLOAT, GL_FALSE,
        sizeof(struct ui_vertex), (void*)(sizeof(float) * 2)
    );
    glVertexAttribPointer(
        gr_ui->text_attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
        sizeof(struct ui_vertex), (void*)(sizeof(float) * 4)
    );

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, gr_ui->vertex_count);

    return ERR_SUCCESS;
}

void px_rs_ui_frame_update(void) {
    if (!gr_ui->initialized)
        return;

    glViewport(0, 0, gr_ui->screen_w, gr_ui->screen_h);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gr_ui->program);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void px_rs_ui_resize(PX_Scale2 screen_scale) {
    gr_ui->screen_w = screen_scale.w;
    gr_ui->screen_h = screen_scale.h;
    glViewport(0, 0, screen_scale.w, screen_scale.h);
}
