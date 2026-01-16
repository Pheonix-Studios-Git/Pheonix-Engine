#pragma once

#include <stdint.h>

#include <rendering-sys/opengl.h>
#include <err-codes.h>
#include <font.h>

#define PX_SDF_MAGIC 0x46534450 // PSDF
#define PX_SDF_CUR_VERSION 0x0100

#pragma pack(push, 1)
struct px_sdf_header {
    uint32_t magic;
    uint16_t version;

    uint16_t glyph_count;
    uint16_t atlas_width;
    uint16_t atlas_height;
    
    float sdf_range;
    
    float ascent;
    float descent;
    float line_gap;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct px_sdf_glyph {
    uint32_t codepoint;

    float advance;
    float bearing_x;
    float bearing_y;
    
    float width;
    float height;
    
    float u0, v0;
    float u1, v1;
};
#pragma pack(pop)

struct px_sdf_font_data {
    GLuint texture;
    struct px_sdf_glyph* glyphs;
    uint16_t glyph_count;

    float ascent;
    float descent;
    float line_gap;
    float sdf_range;
};

t_err_codes px_sdf_load(const char* path, struct px_sdf_font_data* out);
void px_sdf_free(struct px_sdf_font_data* data);
float px_sdf_ascent(const PX_Font* font);
float px_sdf_descent(const PX_Font* font);
float px_sdf_line_gap(const PX_Font* font);

float px_sdf_range(const PX_Font* font);
const struct px_sdf_glyph* px_sdf_find_glyph(const PX_Font* font, uint32_t cp);
GLuint px_sdf_gl_texture(const PX_Font* font);
