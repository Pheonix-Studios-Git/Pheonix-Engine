#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <rendering-sys.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>
#include <decoders/unicode.h>

#include <rendering-sys/opengl.h>

#define MAX_BATCHES 256
#define MAX_VERTEX_COUNT 8192

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

enum ui_batch_type {
    UI_BATCH_PANEL,
    UI_BATCH_LINE,
    UI_BATCH_TEXT
};

struct ui_batch {
    enum ui_batch_type type;
    int vertex_offset;
    int vertex_count;
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
    int vertex_count;
    int vertex_capacity;

    struct ui_batch batches[MAX_BATCHES];
    int batch_count;

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
    v[3] = (struct ui_vertex){(float)pos.x, y2, 0, 1, c.r, c.g, c.b, c.a};
 
    gr_ui->vertex_count += 4;
}

static void pxgl_ui_push_glyph(float x0, float y0, float x1, float y1, struct px_sdf_glyph* g, PX_Color4 c) {
    if (gr_ui->vertex_count + 6 > gr_ui->vertex_capacity)
        return;

    struct ui_vertex* v = gr_ui->vertices + gr_ui->vertex_count;

    v[0] = (struct ui_vertex){x0, y0, g->u0, g->v0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x1, y0, g->u1, g->v0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x1, y1, g->u1, g->v1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){x0, y1, g->u0, g->v1, c.r, c.g, c.b, c.a};

    gr_ui->vertex_count += 4;
}

static void pxgl_ui_push_line(float x0, float y0, float x1, float y1, float thickness, PX_Color4 c) {
    if (gr_ui->vertex_count + 6 > gr_ui->vertex_capacity)
        return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx*dx + dy*dy);
    if (len == 0.0f) return;

    dx /= len; dy /= len;
    float px = -dy * thickness * 0.5f;
    float py =  dx * thickness * 0.5f;

    struct ui_vertex* v = gr_ui->vertices + gr_ui->vertex_count;
    v[0] = (struct ui_vertex){x0 + px, y0 + py, 0, 0, c.r, c.g, c.b, c.a};
    v[1] = (struct ui_vertex){x1 + px, y1 + py, 1, 0, c.r, c.g, c.b, c.a};
    v[2] = (struct ui_vertex){x1 - px, y1 - py, 1, 1, c.r, c.g, c.b, c.a};
    v[3] = (struct ui_vertex){x0 - px, y0 - py, 0, 1, c.r, c.g, c.b, c.a};

    gr_ui->vertex_count += 4;
}

static void push_batch(struct ui_batch* b) {
    if (gr_ui->batch_count > 0) {
        struct ui_batch* last_b = &gr_ui->batches[gr_ui->batch_count - 1];

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

    if (gr_ui->batch_count < MAX_BATCHES) {
        memcpy(&gr_ui->batches[gr_ui->batch_count++], b, sizeof(struct ui_batch));
    }
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
    gr_ui->uni_size = glGetUniformLocation(gr_ui->program, "u_size");
    gr_ui->uni_corner_radius = glGetUniformLocation(gr_ui->program, "u_corner_radius");
    gr_ui->uni_noise = glGetUniformLocation(gr_ui->program, "u_noise");
    gr_ui->uni_texel_size = glGetUniformLocation(gr_ui->program, "u_texel_size");
    gr_ui->uni_texture = glGetUniformLocation(gr_ui->program, "u_texture");
    gr_ui->attr_pos = glGetAttribLocation(gr_ui->program, "a_pos");
    gr_ui->attr_uv = glGetAttribLocation(gr_ui->program, "a_uv");
    gr_ui->attr_color = glGetAttribLocation(gr_ui->program, "a_color");
    // Text Programs
    gr_ui->text_uni_projection = glGetUniformLocation(gr_ui->text_program, "u_projection");
    gr_ui->text_uni_texture = glGetUniformLocation(gr_ui->text_program, "u_font_text");
    gr_ui->text_uni_sdf_width = glGetUniformLocation(gr_ui->text_program, "u_sdf_width");
    gr_ui->text_uni_pixel_height = glGetUniformLocation(gr_ui->text_program, "u_pixel_height");
    gr_ui->text_uni_outline_width = glGetUniformLocation(gr_ui->text_program, "u_outline_width");
    gr_ui->text_uni_outline_color = glGetUniformLocation(gr_ui->text_program, "u_outline_color");
    gr_ui->text_attr_pos = glGetAttribLocation(gr_ui->text_program, "a_pos");
    gr_ui->text_attr_uv = glGetAttribLocation(gr_ui->text_program, "a_uv");
    gr_ui->text_attr_color = glGetAttribLocation(gr_ui->text_program, "a_color");

    // Textures Pre-made
    unsigned int white_pixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    glGenTextures(1, &gr_ui->blank_tex);
    glBindTexture(GL_TEXTURE_2D, gr_ui->blank_tex);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, &gr_ui->vao);
    glGenBuffers(1, &gr_ui->vbo);
    glGenBuffers(1, &gr_ui->ebo);

    glBindVertexArray(gr_ui->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_ui->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_ui->ebo);

    unsigned short indices[MAX_VERTEX_COUNT / 4 * 6];
    for (int i = 0, v = 0; i < (MAX_VERTEX_COUNT / 4 * 6); i += 6, v += 4) {
        indices[i + 0] = v + 0; indices[i + 1] = v + 1; indices[i + 2] = v + 2;
        indices[i + 3] = v + 2; indices[i + 4] = v + 3; indices[i + 5] = v + 0;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    gr_ui->vertex_capacity = MAX_VERTEX_COUNT;
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

void px_rs_frame_start(void) {
    gr_ui->vertex_count = 0;
    gr_ui->batch_count = 0;
}

void px_rs_frame_end(void) {
    if (gr_ui->vertex_count <= 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, gr_ui->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct ui_vertex) * gr_ui->vertex_count,
        gr_ui->vertices,
        GL_DYNAMIC_DRAW
    );

    float proj[16];
    pxgl_ui_ortho(0.0f, (float)gr_ui->screen_w, 0.0f, (float)gr_ui->screen_h, proj);

    glBindVertexArray(gr_ui->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_ui->ebo);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < gr_ui->batch_count; i++) {
        struct ui_batch* b = &gr_ui->batches[i];
        if (b->vertex_count <= 0) continue;

        unsigned int program = 0;
        int attr_pos = 0;
        int attr_uv = 0;
        int attr_color = 0;

        if (b->type == UI_BATCH_TEXT)
            program = gr_ui->text_program;
        else
            program = gr_ui->program;
        glUseProgram(program);

        if (b->type == UI_BATCH_PANEL) {
            glUniformMatrix4fv(gr_ui->uni_projection, 1, GL_FALSE, proj);
            glUniform2f(gr_ui->uni_size, b->size.w, b->size.h);
            glUniform2f(gr_ui->uni_texel_size, b->texel_size.w, b->texel_size.h);
            glUniform1f(gr_ui->uni_corner_radius, b->corner_radius);
            glUniform1f(gr_ui->uni_noise, b->noise);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, b->texture);
            glUniform1i(gr_ui->uni_texture, 0);

            attr_pos = gr_ui->attr_pos;
            attr_uv = gr_ui->attr_uv;
            attr_color = gr_ui->attr_color;
        } else if (b->type == UI_BATCH_TEXT) {
            glUniformMatrix4fv(gr_ui->text_uni_projection, 1, GL_FALSE, proj);
            glUniform1f(gr_ui->text_uni_sdf_width, b->text_sdf_width);
            glUniform1f(gr_ui->text_uni_pixel_height, b->text_pixel_height);
            glUniform1f(gr_ui->text_uni_outline_width, b->text_outline_width);
            glUniform4f(gr_ui->text_uni_outline_color, (float)b->text_outline_color.r, (float)b->text_outline_color.g, (float)b->text_outline_color.b, (float)b->text_outline_color.a);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, b->texture);
            glUniform1i(gr_ui->text_uni_texture, 0);

            attr_pos = gr_ui->text_attr_pos;
            attr_uv = gr_ui->text_attr_uv;
            attr_color = gr_ui->text_attr_color;
        } else if (b->type == UI_BATCH_LINE) {
            glUniformMatrix4fv(gr_ui->uni_projection, 1, GL_FALSE, proj);
            glUniform2f(gr_ui->uni_size, 0, 0);
            glUniform2f(gr_ui->uni_texel_size, 1, 1);
            glUniform1f(gr_ui->uni_corner_radius, 0.0f);
            glUniform1f(gr_ui->uni_noise, 0.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, b->texture);
            glUniform1i(gr_ui->uni_texture, 0);

            attr_pos = gr_ui->attr_pos;
            attr_uv = gr_ui->attr_uv;
            attr_color = gr_ui->attr_color;
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
    }

    glBindVertexArray(0);
}

t_err_codes px_rs_draw_panel(PX_Transform2 tran, PX_Color4 color, float noise, float cradius) {
    int start_vertex = gr_ui->vertex_count;
    pxgl_ui_push_quad(tran.pos, tran.scale, color);
    int vertex_count = gr_ui->vertex_count - start_vertex;

    struct ui_batch b = {0};
    b.type = UI_BATCH_PANEL;
    b.size = tran.scale;
    b.texel_size = (PX_Scale2){1, 1};
    b.corner_radius = cradius;
    b.noise = noise;
    b.texture = gr_ui->blank_tex;
    b.vertex_count = vertex_count;
    b.vertex_offset = start_vertex;

    push_batch(&b);

    return ERR_SUCCESS;
}

int px_rs_text_width(PX_Font* font, const char* text, float pixel_height) {
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

t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_Vector2 pos, PX_Color4 color, PX_Font* font) {
    int start_vertex = gr_ui->vertex_count;
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
    int vertex_count = gr_ui->vertex_count - start_vertex;

    float sdf_width = px_sdf_range(font) / pixel_height;
    sdf_width = fmaxf(0.015f, fminf(sdf_width, 0.03));

    struct ui_batch b = {0};
    b.type = UI_BATCH_TEXT;
    b.text_sdf_width = sdf_width;
    b.text_pixel_height = pixel_height;
    b.text_outline_width = sdf_width * 2.0f;
    b.text_outline_color = (PX_Color4){0x00, 0x00, 0x00, 0xFF};
    b.texture = px_sdf_gl_texture(font);
    b.vertex_count = vertex_count;
    b.vertex_offset = start_vertex;

    push_batch(&b);

    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_line(PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) {
    int start_vertex = gr_ui->vertex_count;
    pxgl_ui_push_line(start.x, start.y, end.x, end.y, thickness, color);
    int vertex_count = gr_ui->vertex_count - start_vertex;

    struct ui_batch b = {0};
    b.type = UI_BATCH_LINE;
    b.size = (PX_Scale2){0,0};
    b.texel_size = (PX_Scale2){1,1};
    b.texture = gr_ui->blank_tex;
    b.noise = 0.0f;
    b.corner_radius = 0.0f;
    b.vertex_offset = start_vertex;
    b.vertex_count = vertex_count;

    push_batch(&b);
    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_dropdown(PX_Dropdown* dd) {
    PX_Color4 color = dd->color; 
    px_rs_draw_panel((PX_Transform2){dd->pos, (PX_Scale2){dd->width, dd->height}}, color, dd->noise, dd->cradius);

    int x = dd->stext_pos.x;
    for (int i = 0; i < dd->item_count; i++) {
        PX_DropdownItem* item = &dd->items[i];

        PX_Color4 tcolor = dd->hover_index == i ? dd->hover_color : dd->text_color;
        px_rs_render_text(item->label, dd->font_size, (PX_Vector2){x, dd->stext_pos.y}, tcolor, dd->font);

        x += dd->spacing + px_rs_text_width(dd->font, item->label, dd->font_size);

        if (item->is_open) {
            px_rs_draw_panel(item->panel_tran, item->panel_color, item->panel_noise, item->panel_cradius);

            int y = item->stext_pos.y;
            for (int j = 0; j < item->option_count; j++) {
                PX_DropdownOption* option = &item->options[j];

                PX_Color4 ptcolor = item->hover_index == j ? item->hover_color : item->text_color;
                px_rs_render_text(option->label, item->font_size, (PX_Vector2){item->stext_pos.x, y}, ptcolor, dd->font);

                y += item->spacing;
            }
        }
    }

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
