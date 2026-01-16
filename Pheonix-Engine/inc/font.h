#pragma once

#include <stdbool.h>

#include <rendering-sys/opengl.h>
#include <err-codes.h>

typedef enum {
    PX_FONT_BACKEND_SDF = 1,
    PX_FONT_BACKEND_MSDF = 2
} PX_FontBackend;

typedef struct PX_Font {
    PX_FontBackend backend;

    union {
        struct {
            GLuint texture;
            struct px_sdf_glyph* glyphs;
            uint16_t glyph_count;

            float ascent;
            float descent;
            float line_gap;
            float sdf_range;
        } sdf;

        // struct { ... } msdf; 
    } impl;
} PX_Font;

typedef struct {
    float outline_width;
    int outline_r, outline_g, outline_b;
    float softness;
} PX_FontStyle;

typedef struct {
    uint32_t pixel_size; // base font size
    uint32_t atlas_size; // atlas width/height (square)
    uint32_t sdf_range; // distance range in pixels
    bool ascii_only; // true = 32–126
} PX_SDFBuildDesc;

PX_Font* px_font_load(const char* path);
void px_font_destroy(PX_Font* font);

t_err_codes px_sdf_build_font(const char* input_json, const char* output_psdf, const PX_SDFBuildDesc* desc);


