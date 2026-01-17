#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <loaders/sdf-loader.h>
#include <font.h>
#include <err-codes.h>

t_err_codes px_sdf_load(const char* path, struct px_sdf_font_data* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return ERR_COULD_NOT_OPEN_FILE;

    struct px_sdf_header h;
    fread(&h, sizeof(h), 1, f);

    if (h.magic != PX_SDF_MAGIC) {
        fclose(f);
        return ERR_MAGIC_INVALID;
    }

    if (h.version > PX_SDF_CUR_VERSION) {
        fclose(f);
        return ERR_VERSION_INVALID;
    }

    struct px_sdf_glyph* glyphs = (struct px_sdf_glyph*)malloc(sizeof(*glyphs) * h.glyph_count);
    if (!glyphs) {
        fclose(f);
        return ERR_ALLOC_FAILED;
    }
    fread(glyphs, sizeof(*glyphs), h.glyph_count, f);

    size_t atlas_size = h.atlas_width * h.atlas_height;
    unsigned char* pixels = (unsigned char*)malloc(atlas_size);
    if (!pixels) {
        free(glyphs);
        fclose(f);
        return ERR_ALLOC_FAILED;
    }
    fread(pixels, 1, atlas_size, f);

    fclose(f);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_LUMINANCE,
        h.atlas_width, h.atlas_height,
        0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(pixels);

    out->texture = tex;
    out->glyphs = glyphs;
    out->glyph_count = h.glyph_count;
    out->ascent = h.ascent;
    out->descent = h.descent;
    out->line_gap = h.line_gap;
    out->sdf_range = h.sdf_range;

    return ERR_SUCCESS;
}

void px_sdf_free(struct px_sdf_font_data* data) {
    if (!data) return;

    glDeleteTextures(1, &data->texture);
    free(data->glyphs);
    memset(data, 0, sizeof(*data));
}

float px_sdf_range(const PX_Font* font) {
    return font->impl.sdf.sdf_range;
}

float px_sdf_ascent(const PX_Font* font) {
    return font->impl.sdf.ascent;
}

float px_sdf_descent(const PX_Font* font) {
    return font->impl.sdf.descent;
}

float px_sdf_line_gap(const PX_Font* font) {
    return font->impl.sdf.line_gap;
}

// TODO: replace with hash table for large fonts
const struct px_sdf_glyph* px_sdf_find_glyph(const PX_Font* font, uint32_t cp) {
    for (uint16_t i = 0; i < font->impl.sdf.glyph_count; i++) {
        if (font->impl.sdf.glyphs[i].codepoint == cp)
            return &font->impl.sdf.glyphs[i];
    }

    return NULL;
}

GLuint px_sdf_gl_texture(const PX_Font* font) {
    if (!font || font->backend != PX_FONT_BACKEND_SDF)
        return 0;
    return font->impl.sdf.texture;
}
