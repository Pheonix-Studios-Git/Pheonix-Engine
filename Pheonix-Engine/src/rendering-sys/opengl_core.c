#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <cglm/cglm.h>

#include <rendering-sys.h>
#include <font.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>
#include <decoders/unicode.h>
#include <event-sys.h>
#include <pheonix-engine.h>

#include <rendering-sys/opengl.h>
#include <rendering-sys/internal.h>

#define STB_IMAGE_IMPLEMENTATION
#include <external/stb_image.h> // Doesnt use it but implements it here

#define GL_CHECK(label) { \
    GLenum e = glGetError(); \
    if (e != GL_NO_ERROR) printf("[OpenGL] GL Error Caught during " label ": 0x%X\n", e); \
}

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

    

	uint32_t screen_x;
	uint32_t screen_y;
    uint32_t screen_w;
    uint32_t screen_h;
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

    uint32_t screen_w;
    uint32_t screen_h;
    uint32_t screen_x;
    uint32_t screen_y;

    GLuint flatFBO;
    GLuint flatColorTex;
    GLuint flatDepthRBO;
};

static struct ui_renderer gr_2d_b = {0};
struct ui_renderer* gr_gl_2d = &gr_2d_b;

static struct renderer_3d gr_3d_b = {0};
struct renderer_3d* gr_gl_3d = &gr_3d_b;

static char* read_shader(const char* name) {
	if (!name) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "shaders/%s", name);

    FILE* f = fopen(path, "rb");
    if (!f) {
		fprintf(stderr, "Failed to read shader file [%s : Vertex]\n", path);
		perror("\tReason ");
		return NULL;
	}

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* src = (char*)malloc(size + 1);
    if (!src) {
        fclose(f);

		fprintf(stderr, "Failed to allocate memory for reading shader file [%s : Vertex]\n", path);
		perror("\tReason ");
        return NULL;
    }

    fread(src, 1, size, f);
    src[size] = '\0';

    fclose(f);
    return src;
}

static unsigned int pxgl_compile_shader(unsigned int type, const char* source) {
	if (!source) return 0;

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
	if (!vert || !frag) return 0;

    char* vert_src = read_shader(vert);
    char* frag_src = read_shader(frag);

    if (!vert_src || !frag_src) {
        if (vert_src) free(vert_src);
        if (frag_src) free(frag_src);
        return 0;
    }

    unsigned int vs = pxgl_compile_shader(GL_VERTEX_SHADER, vert_src);
    unsigned int fs = pxgl_compile_shader(GL_FRAGMENT_SHADER, frag_src);

    free(vert_src);
    free(frag_src);

    if (!vs || !fs) return 0;

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

static GLenum pxgl_get_texture_type(PX_TextureType type) {
	switch (type) {
		case PX_RS_TEXTURE_TYPE_1D: return GL_TEXTURE_1D;
		case PX_RS_TEXTURE_TYPE_1DA: return GL_TEXTURE_1D_ARRAY;

		case PX_RS_TEXTURE_TYPE_2D: return GL_TEXTURE_2D;
		case PX_RS_TEXTURE_TYPE_2DA: return GL_TEXTURE_2D_ARRAY;
		case PX_RS_TEXTURE_TYPE_2DMS: return GL_TEXTURE_2D_MULTISAMPLE;
		case PX_RS_TEXTURE_TYPE_2DMSA: return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;

		case PX_RS_TEXTURE_TYPE_3D: return GL_TEXTURE_3D;
		case PX_RS_TEXTURE_TYPE_3DCUBE: return GL_TEXTURE_CUBE_MAP;
		case PX_RS_TEXTURE_TYPE_3DACUBE: return GL_TEXTURE_CUBE_MAP_ARRAY;

		default: return GL_INVALID_ENUM;
	}
}

static GLenum pxgl_get_texture_format(PX_TextureFormat format) {
	switch (format) {
        // Red Channel
        case PX_RS_TEXTURE_FORMAT_R8UNORM: return GL_R8;
        case PX_RS_TEXTURE_FORMAT_R8SNORM: return GL_R8_SNORM;
        case PX_RS_TEXTURE_FORMAT_R8U: return GL_R8UI;
        case PX_RS_TEXTURE_FORMAT_R8I: return GL_R8I;

        case PX_RS_TEXTURE_FORMAT_R16UNORM: return GL_R16;
        case PX_RS_TEXTURE_FORMAT_R16SNORM: return GL_R16_SNORM;
        case PX_RS_TEXTURE_FORMAT_R16U: return GL_R16UI;
        case PX_RS_TEXTURE_FORMAT_R16I: return GL_R16I;
        case PX_RS_TEXTURE_FORMAT_R16F: return GL_R16F;

        case PX_RS_TEXTURE_FORMAT_R32U: return GL_R32UI;
        case PX_RS_TEXTURE_FORMAT_R32I: return GL_R32I;
        case PX_RS_TEXTURE_FORMAT_R32F: return GL_R32F;

        // Red + Green Channel
        case PX_RS_TEXTURE_FORMAT_RG8UNORM: return GL_RG8;
        case PX_RS_TEXTURE_FORMAT_RG8SNORM: return GL_RG8_SNORM;
        case PX_RS_TEXTURE_FORMAT_RG8U: return GL_RG8UI;
        case PX_RS_TEXTURE_FORMAT_RG8I: return GL_RG8I;

        case PX_RS_TEXTURE_FORMAT_RG16UNORM: return GL_RG16;
        case PX_RS_TEXTURE_FORMAT_RG16SNORM: return GL_RG16_SNORM;
        case PX_RS_TEXTURE_FORMAT_RG16U: return GL_RG16UI;
        case PX_RS_TEXTURE_FORMAT_RG16I: return GL_RG16I;
        case PX_RS_TEXTURE_FORMAT_RG16F: return GL_RG16F;

        case PX_RS_TEXTURE_FORMAT_RG32U: return GL_RG32UI;
        case PX_RS_TEXTURE_FORMAT_RG32I: return GL_RG32I;
        case PX_RS_TEXTURE_FORMAT_RG32F: return GL_RG32F;

        // Red + Green + Blue Channel
        case PX_RS_TEXTURE_FORMAT_RGB8UNORM: return GL_RGB8;
        case PX_RS_TEXTURE_FORMAT_RGB8SNORM: return GL_RGB8_SNORM;
        case PX_RS_TEXTURE_FORMAT_RGB8U: return GL_RGB8UI;
        case PX_RS_TEXTURE_FORMAT_RGB8I: return GL_RGB8I;
        case PX_RS_TEXTURE_FORMAT_RGB8sRGB: return GL_SRGB8;

        case PX_RS_TEXTURE_FORMAT_RGB16UNORM: return GL_RGB16;
        case PX_RS_TEXTURE_FORMAT_RGB16SNORM: return GL_RGB16_SNORM;
        case PX_RS_TEXTURE_FORMAT_RGB16U: return GL_RGB16UI;
        case PX_RS_TEXTURE_FORMAT_RGB16I: return GL_RGB16I;
        case PX_RS_TEXTURE_FORMAT_RGB16F: return GL_RGB16F;

        case PX_RS_TEXTURE_FORMAT_RGB32U: return GL_RGB32UI;
        case PX_RS_TEXTURE_FORMAT_RGB32I: return GL_RGB32I;
        case PX_RS_TEXTURE_FORMAT_RGB32F: return GL_RGB32F;

        // Red + Green + Blue + Alpha Channel
        case PX_RS_TEXTURE_FORMAT_RGBA8UNORM: return GL_RGBA8;
        case PX_RS_TEXTURE_FORMAT_RGBA8SNORM: return GL_RGBA8_SNORM;
        case PX_RS_TEXTURE_FORMAT_RGBA8U: return GL_RGBA8UI;
        case PX_RS_TEXTURE_FORMAT_RGBA8I: return GL_RGBA8I;
        case PX_RS_TEXTURE_FORMAT_RGBA8sRGB: return GL_SRGB8_ALPHA8;

        case PX_RS_TEXTURE_FORMAT_RGBA16UNORM: return GL_RGBA16;
        case PX_RS_TEXTURE_FORMAT_RGBA16SNORM: return GL_RGBA16_SNORM;
        case PX_RS_TEXTURE_FORMAT_RGBA16U: return GL_RGBA16UI;
        case PX_RS_TEXTURE_FORMAT_RGBA16I: return GL_RGBA16I;
        case PX_RS_TEXTURE_FORMAT_RGBA16F: return GL_RGBA16F;

        case PX_RS_TEXTURE_FORMAT_RGBA32U: return GL_RGBA32UI;
        case PX_RS_TEXTURE_FORMAT_RGBA32I: return GL_RGBA32I;
        case PX_RS_TEXTURE_FORMAT_RGBA32F: return GL_RGBA32F;

        default: return GL_INVALID_ENUM;
    }
}

static GLenum pxgl_get_texture_upload_format(PX_TextureFormat format) {
	switch (format) {
		case PX_RS_TEXTURE_FORMAT_R8UNORM:
		case PX_RS_TEXTURE_FORMAT_R8SNORM:
		case PX_RS_TEXTURE_FORMAT_R8U:
		case PX_RS_TEXTURE_FORMAT_R8I:
		case PX_RS_TEXTURE_FORMAT_R16UNORM:
		case PX_RS_TEXTURE_FORMAT_R16SNORM:
		case PX_RS_TEXTURE_FORMAT_R16U:
		case PX_RS_TEXTURE_FORMAT_R16I:
		case PX_RS_TEXTURE_FORMAT_R16F:
		case PX_RS_TEXTURE_FORMAT_R32U:
		case PX_RS_TEXTURE_FORMAT_R32I:
		case PX_RS_TEXTURE_FORMAT_R32F:
			return GL_RED;

		case PX_RS_TEXTURE_FORMAT_RG8UNORM:
		case PX_RS_TEXTURE_FORMAT_RG8SNORM:
		case PX_RS_TEXTURE_FORMAT_RG8U:
		case PX_RS_TEXTURE_FORMAT_RG8I:
		case PX_RS_TEXTURE_FORMAT_RG16UNORM:
		case PX_RS_TEXTURE_FORMAT_RG16SNORM:
		case PX_RS_TEXTURE_FORMAT_RG16U:
		case PX_RS_TEXTURE_FORMAT_RG16I:
		case PX_RS_TEXTURE_FORMAT_RG16F:
		case PX_RS_TEXTURE_FORMAT_RG32U:
		case PX_RS_TEXTURE_FORMAT_RG32I:
		case PX_RS_TEXTURE_FORMAT_RG32F:
			return GL_RG;

		case PX_RS_TEXTURE_FORMAT_RGB8UNORM:
		case PX_RS_TEXTURE_FORMAT_RGB8SNORM:
		case PX_RS_TEXTURE_FORMAT_RGB8U:
		case PX_RS_TEXTURE_FORMAT_RGB8I:
		case PX_RS_TEXTURE_FORMAT_RGB8sRGB:
		case PX_RS_TEXTURE_FORMAT_RGB16UNORM:
		case PX_RS_TEXTURE_FORMAT_RGB16SNORM:
		case PX_RS_TEXTURE_FORMAT_RGB16U:
		case PX_RS_TEXTURE_FORMAT_RGB16I:
		case PX_RS_TEXTURE_FORMAT_RGB16F:
		case PX_RS_TEXTURE_FORMAT_RGB32U:
		case PX_RS_TEXTURE_FORMAT_RGB32I:
		case PX_RS_TEXTURE_FORMAT_RGB32F:
			return GL_RGB;

		case PX_RS_TEXTURE_FORMAT_RGBA8UNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA8SNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA8U:
		case PX_RS_TEXTURE_FORMAT_RGBA8I:
		case PX_RS_TEXTURE_FORMAT_RGBA8sRGB:
		case PX_RS_TEXTURE_FORMAT_RGBA16UNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA16SNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA16U:
		case PX_RS_TEXTURE_FORMAT_RGBA16I:
		case PX_RS_TEXTURE_FORMAT_RGBA16F:
		case PX_RS_TEXTURE_FORMAT_RGBA32U:
		case PX_RS_TEXTURE_FORMAT_RGBA32I:
		case PX_RS_TEXTURE_FORMAT_RGBA32F:
			return GL_RGBA;

		default: return GL_INVALID_ENUM;
	}
}

static GLenum pxgl_get_texture_upload_type(PX_TextureFormat format) {
	switch (format) {
		case PX_RS_TEXTURE_FORMAT_R8UNORM:
		case PX_RS_TEXTURE_FORMAT_R8U:
		case PX_RS_TEXTURE_FORMAT_RG8UNORM:
		case PX_RS_TEXTURE_FORMAT_RG8U:
		case PX_RS_TEXTURE_FORMAT_RGB8UNORM:
		case PX_RS_TEXTURE_FORMAT_RGB8U:
		case PX_RS_TEXTURE_FORMAT_RGB8sRGB:
		case PX_RS_TEXTURE_FORMAT_RGBA8UNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA8U:
		case PX_RS_TEXTURE_FORMAT_RGBA8sRGB:
			return GL_UNSIGNED_BYTE;

		case PX_RS_TEXTURE_FORMAT_R8SNORM:
		case PX_RS_TEXTURE_FORMAT_R8I:
		case PX_RS_TEXTURE_FORMAT_RG8SNORM:
		case PX_RS_TEXTURE_FORMAT_RG8I:
		case PX_RS_TEXTURE_FORMAT_RGB8SNORM:
		case PX_RS_TEXTURE_FORMAT_RGB8I:
		case PX_RS_TEXTURE_FORMAT_RGBA8SNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA8I:
			return GL_BYTE;

		case PX_RS_TEXTURE_FORMAT_R16UNORM:
		case PX_RS_TEXTURE_FORMAT_R16U:
		case PX_RS_TEXTURE_FORMAT_RG16UNORM:
		case PX_RS_TEXTURE_FORMAT_RG16U:
		case PX_RS_TEXTURE_FORMAT_RGB16UNORM:
		case PX_RS_TEXTURE_FORMAT_RGB16U:
		case PX_RS_TEXTURE_FORMAT_RGBA16UNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA16U:
			return GL_UNSIGNED_SHORT;

		case PX_RS_TEXTURE_FORMAT_R16SNORM:
		case PX_RS_TEXTURE_FORMAT_R16I:
		case PX_RS_TEXTURE_FORMAT_RG16SNORM:
		case PX_RS_TEXTURE_FORMAT_RG16I:
		case PX_RS_TEXTURE_FORMAT_RGB16SNORM:
		case PX_RS_TEXTURE_FORMAT_RGB16I:
		case PX_RS_TEXTURE_FORMAT_RGBA16SNORM:
		case PX_RS_TEXTURE_FORMAT_RGBA16I:
			return GL_SHORT;

		case PX_RS_TEXTURE_FORMAT_R16F:
		case PX_RS_TEXTURE_FORMAT_RG16F:
		case PX_RS_TEXTURE_FORMAT_RGB16F:
		case PX_RS_TEXTURE_FORMAT_RGBA16F:
			return GL_HALF_FLOAT;

		case PX_RS_TEXTURE_FORMAT_R32U:
		case PX_RS_TEXTURE_FORMAT_RG32U:
		case PX_RS_TEXTURE_FORMAT_RGB32U:
		case PX_RS_TEXTURE_FORMAT_RGBA32U:
			return GL_UNSIGNED_INT;

		case PX_RS_TEXTURE_FORMAT_R32I:
		case PX_RS_TEXTURE_FORMAT_RG32I:
		case PX_RS_TEXTURE_FORMAT_RGB32I:
		case PX_RS_TEXTURE_FORMAT_RGBA32I:
			return GL_INT;

		case PX_RS_TEXTURE_FORMAT_R32F:
		case PX_RS_TEXTURE_FORMAT_RG32F:
		case PX_RS_TEXTURE_FORMAT_RGB32F:
		case PX_RS_TEXTURE_FORMAT_RGBA32F:
			return GL_FLOAT;

		default: return GL_INVALID_ENUM;
	}
}

static GLenum pxgl_get_texture_filter(PX_TextureFilter filter) {
	switch (filter) {
		case PX_RS_TEXTURE_FILTER_NEAREST: return GL_NEAREST;
		case PX_RS_TEXTURE_FILTER_LINEAR: return GL_LINEAR;

		case PX_RS_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
		case PX_RS_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
		case PX_RS_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case PX_RS_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;

		default: return GL_INVALID_ENUM;
	}
}

static GLenum pxgl_get_texture_address_mode(PX_TextureAddressMode mode) {
	switch (mode) {
		case PX_RS_TEXTURE_ADDRESS_REPEAT: return GL_REPEAT;
		case PX_RS_TEXTURE_ADDRESS_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
		case PX_RS_TEXTURE_ADDRESS_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
		case PX_RS_TEXTURE_ADDRESS_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;

		default: return GL_INVALID_ENUM;
	}
}

static void pxgl_2d_ortho(float left, float right, float bottom, float top, float* out_mat4) {
	if (!out_mat4) return;

    memset(out_mat4, 0, sizeof(float) * 16);

    out_mat4[0] = 2.0f / (right - left);
    out_mat4[5] = -2.0f / (top - bottom);
    out_mat4[10] = -1.0f;
    out_mat4[12] = -(right + left) / (right - left);
    out_mat4[13] = (top + bottom) / (top - bottom);
    out_mat4[15] = 1.0f;
}

PX_GPU_Handle px_rs_gl_get_flat_fbo(void) {
	return (PX_GPU_Handle)gr_gl_3d->flatFBO;
}

PX_GPU_Handle px_rs_gl_get_blank_tex(void) {
	return (PX_GPU_Handle)gr_gl_2d->blank_tex;
}

PX_Scale2 px_rs_gl_get_flat_fbo_scale(void) {
	return (PX_Scale2){.w=gr_gl_3d->screen_w, .h=gr_gl_3d->screen_h};
}

t_err_codes px_rs_gl_init(PX_WContext* ctx) {
	(void)ctx; // No Use

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return ERR_GL_GLEW_INIT_FAILED;
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_gl_init_2d(PX_AnchorRect viewport) {
	PX_Transform2 t = enginef_convert_anchor_to_transform(viewport);

    memset(gr_gl_2d, 0, sizeof(*gr_gl_2d));

    gr_gl_2d->program = pxgl_create_program("2d_vertex.glsl", "2d_fragment.glsl");
    if (gr_gl_2d->program == 0)
        return ERR_GL_PROGRAM_CREATION_FAILED;
    gr_gl_2d->text_program = pxgl_create_program("2d_vertex.glsl", "2d_textfrag.glsl");
    if (gr_gl_2d->text_program == 0) {
        glDeleteProgram(gr_gl_2d->program);
        return ERR_GL_PROGRAM_CREATION_FAILED;
    } 

    // Core UI Programs
    gr_gl_2d->uni_projection = glGetUniformLocation(gr_gl_2d->program, "u_projection");
    gr_gl_2d->uni_size = glGetUniformLocation(gr_gl_2d->program, "u_size");
    gr_gl_2d->uni_corner_radius = glGetUniformLocation(gr_gl_2d->program, "u_corner_radius");
    gr_gl_2d->uni_noise = glGetUniformLocation(gr_gl_2d->program, "u_noise");
    gr_gl_2d->uni_texel_size = glGetUniformLocation(gr_gl_2d->program, "u_texel_size");
    gr_gl_2d->uni_texture = glGetUniformLocation(gr_gl_2d->program, "u_texture");
    gr_gl_2d->attr_pos = glGetAttribLocation(gr_gl_2d->program, "a_pos");
    gr_gl_2d->attr_uv = glGetAttribLocation(gr_gl_2d->program, "a_uv");
    gr_gl_2d->attr_color = glGetAttribLocation(gr_gl_2d->program, "a_color");
    // Text Programs
    gr_gl_2d->text_uni_projection = glGetUniformLocation(gr_gl_2d->text_program, "u_projection");
    gr_gl_2d->text_uni_texture = glGetUniformLocation(gr_gl_2d->text_program, "u_font_text");
    gr_gl_2d->text_uni_sdf_width = glGetUniformLocation(gr_gl_2d->text_program, "u_sdf_width");
    gr_gl_2d->text_uni_pixel_height = glGetUniformLocation(gr_gl_2d->text_program, "u_pixel_height");
    gr_gl_2d->text_uni_outline_width = glGetUniformLocation(gr_gl_2d->text_program, "u_outline_width");
    gr_gl_2d->text_uni_outline_color = glGetUniformLocation(gr_gl_2d->text_program, "u_outline_color");
    gr_gl_2d->text_attr_pos = glGetAttribLocation(gr_gl_2d->text_program, "a_pos");
    gr_gl_2d->text_attr_uv = glGetAttribLocation(gr_gl_2d->text_program, "a_uv");
    gr_gl_2d->text_attr_color = glGetAttribLocation(gr_gl_2d->text_program, "a_color");

    // Textures Pre-made
    uint8_t white_pixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    glGenTextures(1, &gr_gl_2d->blank_tex);
    glBindTexture(GL_TEXTURE_2D, gr_gl_2d->blank_tex);
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

    glGenVertexArrays(1, &gr_gl_2d->vao);
    glGenBuffers(1, &gr_gl_2d->vbo);
    glGenBuffers(1, &gr_gl_2d->ebo);

    glBindVertexArray(gr_gl_2d->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_2d->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_2d->ebo);

    unsigned short indices[MAX_VERTEX_COUNT / 4 * 6];
    for (size_t i = 0, v = 0; i < (MAX_VERTEX_COUNT / 4 * 6); i += 6, v += 4) {
        indices[i + 0] = v + 0; indices[i + 1] = v + 1; indices[i + 2] = v + 2;
        indices[i + 3] = v + 2; indices[i + 4] = v + 3; indices[i + 5] = v + 0;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    gr_gl_2d->screen_w = (uint32_t)t.scale.w;
    gr_gl_2d->screen_h = (uint32_t)t.scale.h;
	gr_gl_2d->screen_x = (uint32_t)t.pos.x;
	gr_gl_2d->screen_y = (uint32_t)t.pos.y;
    gr_gl_2d->initialized = true;

    glViewport((GLint)t.pos.x, (GLint)t.pos.y, (GLsizei)t.scale.w, (GLsizei)t.scale.h);
    return ERR_SUCCESS;
}

t_err_codes px_rs_gl_init_3d(PX_AnchorRect viewport) {
	PX_Transform2 t = enginef_convert_anchor_to_transform(viewport);

    gscene_cam_3d.position[0] = 8.0f;
    gscene_cam_3d.position[1] = 8.0f;
    gscene_cam_3d.position[2] = 8.0f;

    gscene_cam_3d.target[0] = 0.0f;
    gscene_cam_3d.target[1] = 0.0f;
    gscene_cam_3d.target[2] = 0.0f;

    gscene_cam_3d.up[0] = 0.0f;
    gscene_cam_3d.up[1] = 1.0f;
    gscene_cam_3d.up[2] = 0.0f;

    gscene_cam_3d.yaw = -90.0f;
    gscene_cam_3d.pitch = -25.0f;

    gscene_cam_3d.move_speed = 0.1f;
    gscene_cam_3d.mouse_sens = 0.1f;

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
	if (!gr_gl_2d->blank_tex) {
		uint8_t white_pixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		glGenTextures(1, &gr_gl_2d->blank_tex);
		glBindTexture(GL_TEXTURE_2D, gr_gl_2d->blank_tex);
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
	}

    glGenFramebuffers(1, &gr_gl_3d->flatFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gr_gl_3d->flatFBO);

    glGenTextures(1, &gr_gl_3d->flatColorTex);
    glBindTexture(GL_TEXTURE_2D, gr_gl_3d->flatColorTex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        (GLsizei)t.scale.w,
        (GLsizei)t.scale.h,
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
        (GLsizei)t.scale.w,
        (GLsizei)t.scale.h
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

    glViewport((GLint)t.pos.x, (GLint)t.pos.y, (GLsizei)t.scale.w, (GLsizei)t.scale.h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &gr_gl_3d->vao);
    glGenBuffers(1, &gr_gl_3d->vbo);
    glGenBuffers(1, &gr_gl_3d->ebo);

    glBindVertexArray(gr_gl_3d->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);

    gr_gl_3d->screen_w = (uint32_t)t.scale.w;
    gr_gl_3d->screen_h = (uint32_t)t.scale.h;
    gr_gl_3d->screen_x = (uint32_t)t.pos.x;
    gr_gl_3d->screen_y = (uint32_t)t.pos.y;
    gr_gl_3d->initialized = true;

    glViewport((GLint)t.pos.x, (GLint)t.pos.y, (GLsizei)t.scale.w, (GLsizei)t.scale.h);

    return ERR_SUCCESS;
}

void px_rs_gl_shutdown_2d(void) {
    if (!gr_gl_2d->initialized)
        return;

    if (gr_gl_2d->blank_tex) glDeleteTextures(1, &gr_gl_2d->blank_tex);

    if (gr_gl_2d->vbo) glDeleteBuffers(1, &gr_gl_2d->vbo);
    if (gr_gl_2d->vao) glDeleteVertexArrays(1, &gr_gl_2d->vao);
    if (gr_gl_2d->ebo) glDeleteBuffers(1, &gr_gl_2d->ebo);
    if (gr_gl_2d->program) glDeleteProgram(gr_gl_2d->program);
    if (gr_gl_2d->text_program) glDeleteProgram(gr_gl_2d->text_program);

    memset(gr_gl_2d, 0, sizeof(*gr_gl_2d));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void px_rs_gl_shutdown_3d(void) {
    if (!gr_gl_3d->initialized)
        return;

    if (gr_gl_3d->vbo) glDeleteBuffers(1, &gr_gl_3d->vbo);
    if (gr_gl_3d->vao) glDeleteVertexArrays(1, &gr_gl_3d->vao);
    if (gr_gl_3d->ebo) glDeleteBuffers(1, &gr_gl_3d->ebo);
    if (gr_gl_3d->program) glDeleteProgram(gr_gl_3d->program);

    if (gr_gl_3d->flatFBO) glDeleteFramebuffers(1, &gr_gl_3d->flatFBO);
    if (gr_gl_3d->flatColorTex) glDeleteTextures(1, &gr_gl_3d->flatColorTex);
    if (gr_gl_3d->flatDepthRBO) glDeleteRenderbuffers(1, &gr_gl_3d->flatDepthRBO);
    
    memset(gr_gl_3d, 0, sizeof(*gr_gl_3d));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void px_rs_gl_shutdown(void) {
    px_rs_gl_shutdown_2d();
    px_rs_gl_shutdown_3d();
}

void px_rs_gl_frame_start(void) {
}

void px_rs_gl_2d_frame_end(void) {
    if (gr_batch_2d->vertex_count <= 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_2d->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct vertex_2d) * gr_batch_2d->vertex_count,
        gr_batch_2d->vertices,
        GL_DYNAMIC_DRAW
    );
	GL_CHECK("Initializing Buffers");

    float proj[16];
    pxgl_2d_ortho(0.0f, (float)gr_gl_2d->screen_w, 0.0f, (float)gr_gl_2d->screen_h, proj);

    glBindVertexArray(gr_gl_2d->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_2d->ebo);

    unsigned int old_program = 0;
	GLuint curFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&curFBO);
    bool pure_color_enabled = false;

	float oldWidth = 1.0f;
    glLineWidth(oldWidth);
	GL_CHECK("Setting Line Width");

    for (size_t i = 0; i < gr_batch_2d->batch_count; i++) {
        struct batch_2d* b = &gr_batch_2d->batches[i];
        if (b->vertex_count <= 0) continue;

        unsigned int program = 0;
        int attr_pos = 0;
        int attr_uv = 0;
        int attr_color = 0;

        if (b->type == BATCH_2D_TEXT) program = gr_gl_2d->text_program;
        else program = gr_gl_2d->program;
        if (program != old_program) glUseProgram(program);

        if (b->switch_fbo && curFBO != (GLuint)b->fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)b->fbo);
			GL_CHECK("Binding Batch Framebuffer");
            glViewport(b->fbo_x, b->fbo_y, b->fbo_w, b->fbo_h);
			GL_CHECK("Setting Batch Framebuffer's Viewport");
			GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    		glDrawBuffers(1, &drawBuf);
			GL_CHECK("Drawing Batch Framebuffer's Color Attachment Buffer");
            curFBO = (GLuint)b->fbo;
        } else if (!b->switch_fbo && curFBO != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
			GL_CHECK("Binding Main Framebuffer");
            glViewport(gr_gl_2d->screen_x, gr_gl_2d->screen_y, gr_gl_2d->screen_w, gr_gl_2d->screen_h);
			GL_CHECK("Setting Viewport");
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
            case BATCH_2D_PANEL: {
                glUniformMatrix4fv(gr_gl_2d->uni_projection, 1, GL_FALSE, proj);
                glUniform2f(gr_gl_2d->uni_size, b->size.w, b->size.h);
                glUniform2f(gr_gl_2d->uni_texel_size, b->texel_size.w, b->texel_size.h);
                glUniform1f(gr_gl_2d->uni_corner_radius, b->corner_radius);
                glUniform1f(gr_gl_2d->uni_noise, b->noise);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
				if (b->sampler != PX_RS_GPU_INVALID_HANDLE) glBindSampler(0, (GLuint)b->sampler);
                glUniform1i(gr_gl_2d->uni_texture, 0);

                attr_pos = gr_gl_2d->attr_pos;
                attr_uv = gr_gl_2d->attr_uv;
                attr_color = gr_gl_2d->attr_color;
                break;
            }
            case BATCH_2D_TEXT: {
                glUniformMatrix4fv(gr_gl_2d->text_uni_projection, 1, GL_FALSE, proj);
                glUniform1f(gr_gl_2d->text_uni_sdf_width, b->text_sdf_width);
                glUniform1f(gr_gl_2d->text_uni_pixel_height, b->text_pixel_height);
                glUniform1f(gr_gl_2d->text_uni_outline_width, b->text_outline_width);
                glUniform4f(gr_gl_2d->text_uni_outline_color, (float)b->text_outline_color.r, (float)b->text_outline_color.g, (float)b->text_outline_color.b, (float)b->text_outline_color.a);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
				if (b->sampler != PX_RS_GPU_INVALID_HANDLE) glBindSampler(0, (GLuint)b->sampler);
                glUniform1i(gr_gl_2d->text_uni_texture, 0);

                attr_pos = gr_gl_2d->text_attr_pos;
                attr_uv = gr_gl_2d->text_attr_uv;
                attr_color = gr_gl_2d->text_attr_color;
                break;
            }
            case BATCH_2D_LINE: {
                glUniformMatrix4fv(gr_gl_2d->uni_projection, 1, GL_FALSE, proj);
                glUniform2f(gr_gl_2d->uni_size, 0, 0);
                glUniform2f(gr_gl_2d->uni_texel_size, 1, 1);
                glUniform1f(gr_gl_2d->uni_corner_radius, 0.0f);
                glUniform1f(gr_gl_2d->uni_noise, 0.0f);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, (GLuint)b->texture);
				if (b->sampler != PX_RS_GPU_INVALID_HANDLE) glBindSampler(0, (GLuint)b->sampler);
                glUniform1i(gr_gl_2d->uni_texture, 0);

                attr_pos = gr_gl_2d->attr_pos;
                attr_uv = gr_gl_2d->attr_uv;
                attr_color = gr_gl_2d->attr_color;
                break;
            }
            default: continue;
        }
		GL_CHECK("Setting Uniforms");

        uintptr_t base_offset = (uintptr_t)b->vertex_offset * sizeof(struct vertex_2d);
        int stride = sizeof(struct vertex_2d);

        glEnableVertexAttribArray(attr_pos);
        glEnableVertexAttribArray(attr_uv);
        glEnableVertexAttribArray(attr_color);

        glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, stride, (void*)(base_offset + offsetof(struct vertex_2d, x)));
        glVertexAttribPointer(attr_uv, 2, GL_FLOAT, GL_FALSE, stride, (void*)(base_offset + offsetof(struct vertex_2d, u)));
        glVertexAttribPointer(attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)(base_offset + offsetof(struct vertex_2d, r)));

        if (b->type == BATCH_2D_LINE) {
			if (b->line_width != oldWidth) {
				oldWidth = b->line_width;
				glLineWidth(b->line_width);
				GL_CHECK("Setting Batch's Line Width");
			}
			glDrawArrays(GL_LINES, b->vertex_offset, b->vertex_count);
			GL_CHECK("Drawing Lines");
		} else {
			int index_count = (b->vertex_count / 4) * 6;
			glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, (void*)0);
			GL_CHECK("Drawing Elements");
		}

        glDisableVertexAttribArray(attr_pos);
        glDisableVertexAttribArray(attr_uv);
        glDisableVertexAttribArray(attr_color);

        old_program = program;
    }

    glBindVertexArray(0);
}

void px_rs_gl_3d_frame_end(void) {
    if (gr_batch_3d->vertex_count <= 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(struct vertex_3d) * gr_batch_3d->vertex_count,
        gr_batch_3d->vertices,
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(uint32_t) * gr_batch_3d->index_count,
        gr_batch_3d->indices,
        GL_DYNAMIC_DRAW
    );
	GL_CHECK("Initializing Buffers");

    mat4 proj = GLM_MAT4_IDENTITY_INIT;
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    glm_perspective(glm_rad(70.0f), (float)gr_gl_3d->screen_w / (float)gr_gl_3d->screen_h, 0.1f, 1000.0f, proj);
    
    vec3 forward;
    forward[0] = cos(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    forward[1] = sin(glm_rad(gscene_cam_3d.pitch));
    forward[2] = sin(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    glm_normalize(forward);

    vec3 target;
    glm_vec3_add(gscene_cam_3d.position, forward, target);
    glm_lookat(gscene_cam_3d.position, target, gscene_cam_3d.up, view);

    glBindVertexArray(gr_gl_3d->vao);
    glBindBuffer(GL_ARRAY_BUFFER, gr_gl_3d->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gr_gl_3d->ebo);

    unsigned int old_program = 0;
    
    float oldWidth = 1.0f;
    glLineWidth(oldWidth);
	GL_CHECK("Setting Line Width");
    
    bool depth_enabled = true;
    GLuint curFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&curFBO);
    bool pure_color_enabled = false;

    for (size_t i = 0; i < gr_batch_3d->batch_count; i++) {
        struct batch_3d* b = &gr_batch_3d->batches[i];
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
                glBindTexture(GL_TEXTURE_2D, gr_gl_2d->blank_tex);
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
                glBindTexture(GL_TEXTURE_2D, gr_gl_2d->blank_tex);
                glUniform1i(gr_gl_3d->uni_texture, 0);

                attr_pos = gr_gl_3d->attr_pos;
                attr_uv = gr_gl_3d->attr_uv;
                attr_normal = gr_gl_3d->attr_normal;

                break;
            }
            default: continue;
        }
		GL_CHECK("Setting Uniforms");

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
		GL_CHECK("Drawing Elements");

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

    px_rs_gl_2d_frame_update();
    px_rs_gl_2d_frame_end();
}

void px_rs_gl_2d_frame_update(void) {
    if (!gr_gl_2d->initialized)
        return;

    glViewport(0, 0, gr_gl_2d->screen_w, gr_gl_2d->screen_h);

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
}

void px_rs_gl_2d_resize(PX_AnchorRect viewport) {
	PX_Transform2 t = enginef_convert_anchor_to_transform(viewport);

    gr_gl_2d->screen_w = (uint32_t)t.scale.w;
    gr_gl_2d->screen_h = (uint32_t)t.scale.h;
    gr_gl_2d->screen_x = (uint32_t)t.pos.x;
    gr_gl_2d->screen_y = (uint32_t)t.pos.y;
    glViewport((GLint)t.pos.x, (GLint)t.pos.y, (GLsizei)t.scale.w, (GLsizei)t.scale.h);
}

void px_rs_gl_3d_resize(PX_AnchorRect viewport) {
	PX_Transform2 t = enginef_convert_anchor_to_transform(viewport);

    gr_gl_3d->screen_w = (uint32_t)t.scale.w;
    gr_gl_3d->screen_h = (uint32_t)t.scale.h;
    gr_gl_3d->screen_x = (uint32_t)t.pos.x;
    gr_gl_3d->screen_y = (uint32_t)t.pos.y;
    glViewport((GLint)t.pos.x, (GLint)t.pos.y, (GLsizei)t.scale.w, (GLsizei)t.scale.h);
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

t_err_codes px_rs_gl_create_texture(PX_Texture* texture) {
	if (!texture) return ERR_INVALID_ARGUMENTS;
	
	GLenum tex_type = pxgl_get_texture_type(texture->type);
	if (tex_type == GL_INVALID_ENUM) return ERR_RS_INVALID_TEXTURE_TYPE;
	
	GLenum tex_format = pxgl_get_texture_format(texture->format);
	if (tex_format == GL_INVALID_ENUM) return ERR_RS_INVALID_TEXTURE_FORMAT;

	GLuint out = 0;
	glCreateTextures(tex_type, 1, &out);
	if (out == 0) return ERR_FAILURE;

	switch (tex_type) {
		case GL_TEXTURE_1D: {
			glTextureStorage1D(out, texture->mip_levels, tex_format, texture->width);
			break;
		}
		case GL_TEXTURE_1D_ARRAY: {
			glTextureStorage2D(out, texture->mip_levels, tex_format, texture->width, texture->layers);
			break;
		}
		case GL_TEXTURE_2D: {
			glTextureStorage2D(out, texture->mip_levels, tex_format, texture->width, texture->height);
			break;
		}
		case GL_TEXTURE_2D_ARRAY: {
			glTextureStorage3D(out, texture->mip_levels, tex_format, texture->width, texture->height, texture->layers);
			break;
		}
		case GL_TEXTURE_2D_MULTISAMPLE: {
			glTextureStorage2DMultisample(out, texture->samples, tex_format, texture->width, texture->height, GL_TRUE);
			break;
		}
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: {
			glTextureStorage3DMultisample(out, texture->samples, tex_format, texture->width, texture->height, texture->layers, GL_TRUE);
			break;
		}
		case GL_TEXTURE_3D: {
			glTextureStorage3D(out, texture->mip_levels, tex_format, texture->width, texture->height, texture->depth);
			break;
		}
		case GL_TEXTURE_CUBE_MAP: {
			glTextureStorage2D(out, texture->mip_levels, tex_format, texture->width, texture->width);
			break;
		}
		case GL_TEXTURE_CUBE_MAP_ARRAY: {
			glTextureStorage3D(out, texture->mip_levels, tex_format, texture->width, texture->width, texture->layers * 6);
			break;
		}
		default: {
			glDeleteTextures(1, &out);
			return ERR_RS_INVALID_TEXTURE_FORMAT;
		}
	}

	texture->handle = (PX_GPU_Handle)out;
	texture->sampler_handle = PX_RS_GPU_INVALID_HANDLE;
	return ERR_SUCCESS;
}

t_err_codes px_rs_gl_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data) {
	if (!texture || !data) return ERR_INVALID_ARGUMENTS;
	if (texture->handle == PX_RS_GPU_INVALID_HANDLE) return ERR_RS_INVALID_HANDLE;
	if (mip_level >= texture->mip_levels) return ERR_RS_INVALID_MIP_LEVEL;

	GLuint handle = (GLuint)texture->handle;

	GLenum format = pxgl_get_texture_upload_format(source_format);
	if (format == GL_INVALID_ENUM) return ERR_RS_INVALID_TEXTURE_FORMAT;

	GLenum type = pxgl_get_texture_upload_type(source_format);
	if (type == GL_INVALID_ENUM) return ERR_RS_INVALID_TEXTURE_FORMAT;

	uint32_t width = texture->width >> mip_level;
	uint32_t height = texture->height >> mip_level;
	uint32_t depth = texture->depth >> mip_level;

	if (width == 0) width = 1;
	if (height == 0) height = 1;
	if (depth == 0) depth = 1;

	switch (texture->type) {
		case PX_RS_TEXTURE_TYPE_1D: {
			glTextureSubImage1D(handle, mip_level,0, width, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_1DA: {
			glTextureSubImage2D(handle, mip_level, 0, 0, width, texture->layers, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_2D: {
			glTextureSubImage2D(handle, mip_level, 0, 0, width, height,	format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_2DA: {
			glTextureSubImage3D(handle, mip_level, 0, 0, 0, width, height, texture->layers, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_3D: {
			glTextureSubImage3D(handle, mip_level, 0, 0, 0, width, height, depth, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_3DCUBE: {
			glTextureSubImage3D(handle, mip_level, 0, 0, 0, width, height, 6, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_3DACUBE: {
			glTextureSubImage3D(handle, mip_level, 0, 0, 0, width, height, texture->layers * 6, format, type, data);
			break;
		}

		case PX_RS_TEXTURE_TYPE_2DMS:
		case PX_RS_TEXTURE_TYPE_2DMSA:
			return ERR_RS_INVALID_TEXTURE_TYPE;

		default: return ERR_RS_INVALID_TEXTURE_TYPE;
	}

	return ERR_SUCCESS;
}

t_err_codes px_rs_gl_set_sampler(PX_Texture* texture, PX_Sampler* sampler) {
	if (!texture || !sampler) return ERR_INVALID_ARGUMENTS;
	if (texture->handle == PX_RS_GPU_INVALID_HANDLE) return ERR_RS_INVALID_HANDLE;

	GLenum filter_min = pxgl_get_texture_filter(sampler->min_filter);
	if (filter_min == GL_INVALID_ENUM) return ERR_RS_INVALID_SAMPLER_FILTER;
	GLenum filter_mag = pxgl_get_texture_filter(sampler->mag_filter);
	if (filter_mag == GL_INVALID_ENUM) return ERR_RS_INVALID_SAMPLER_FILTER;

	GLenum au = pxgl_get_texture_address_mode(sampler->address_u);
	if (au == GL_INVALID_ENUM) return ERR_RS_INVALID_SAMPLER_ADDRESS_MODE;
	GLenum av = pxgl_get_texture_address_mode(sampler->address_v);
	if (av == GL_INVALID_ENUM) return ERR_RS_INVALID_SAMPLER_ADDRESS_MODE;
	GLenum aw = pxgl_get_texture_address_mode(sampler->address_w);
	if (aw == GL_INVALID_ENUM) return ERR_RS_INVALID_SAMPLER_ADDRESS_MODE;

	GLuint out;
	glCreateSamplers(1, &out);

	glSamplerParameteri(out, GL_TEXTURE_MIN_FILTER, filter_min);
	glSamplerParameteri(out, GL_TEXTURE_MAG_FILTER, filter_mag);

	glSamplerParameteri(out, GL_TEXTURE_WRAP_S, au);
	glSamplerParameteri(out, GL_TEXTURE_WRAP_T, av);
	glSamplerParameteri(out, GL_TEXTURE_WRAP_R, aw);

	sampler->handle = out;
	texture->sampler_handle = out;

	return ERR_SUCCESS;
}

void px_rs_gl_destroy_texture(PX_Texture* texture) {
	if (!texture) return;
	if (texture->handle == PX_RS_GPU_INVALID_HANDLE) return;

	glDeleteTextures(1, (GLuint*)&texture->handle);
}

void px_rs_gl_destroy_sampler(PX_Sampler* sampler) {
	if (!sampler) return;
	if (sampler->handle == PX_RS_GPU_INVALID_HANDLE) return;

	glDeleteSamplers(1, (GLuint*)&sampler->handle);
}
