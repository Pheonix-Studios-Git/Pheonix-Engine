#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <font.h>
#include <err-codes.h>
#include <loaders/sdf-loader.h>

#include <cJSON.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

PX_Font* px_font_load(const char* path) {
    PX_Font* font = calloc(1, sizeof(PX_Font));
    if (!font) return NULL;

    struct px_sdf_font_data sdf;
    if (px_sdf_load(path, &sdf) != ERR_SUCCESS) {
        free(font);
        return NULL;
    }

    font->backend = PX_FONT_BACKEND_SDF;
    font->impl.sdf.texture = sdf.texture;
    font->impl.sdf.glyphs = sdf.glyphs;
    font->impl.sdf.glyph_count = sdf.glyph_count;
    font->impl.sdf.ascent = sdf.ascent;
    font->impl.sdf.descent = sdf.descent;
    font->impl.sdf.line_gap = sdf.line_gap;
    font->impl.sdf.sdf_range = sdf.sdf_range;

    return font;
}

void px_font_destroy(PX_Font* font) {
    if (!font) return;

    if (font->backend == PX_FONT_BACKEND_SDF) {
        glDeleteTextures(1, &font->impl.sdf.texture);
        free(font->impl.sdf.glyphs);
    }

    free(font);
}

static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
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

t_err_codes px_sdf_build_font(const char* input_json, const char* output_psdf, const PX_SDFBuildDesc* desc) {
    char* json_text = read_file(input_json);
    if (!json_text) return ERR_COULD_NOT_OPEN_FILE;

    cJSON* root = cJSON_Parse(json_text);
    free(json_text);
    if (!root) return ERR_INTERNAL;

    cJSON* atlas = cJSON_GetObjectItem(root, "atlas");
    if (!atlas) {
        fprintf(stderr, "SDF is missing atlas!\n");
        cJSON_Delete(root);
        return ERR_INTERNAL;
    }
    cJSON* metrics = cJSON_GetObjectItem(root, "metrics");
    if (!metrics) {
        fprintf(stderr, "SDF is missing metrics!\n");
        cJSON_Delete(root);
        return ERR_INTERNAL;
    }
    cJSON* glyphs_json = cJSON_GetObjectItem(root, "glyphs");
    if (!glyphs_json) {
        fprintf(stderr, "SDF is missing glyphs!\n");
        cJSON_Delete(root);
        return ERR_INTERNAL;
    }

    int atlas_w = cJSON_GetObjectItem(atlas, "width")->valueint;
    int atlas_h = cJSON_GetObjectItem(atlas, "height")->valueint;
    float sdf_range = (float)cJSON_GetObjectItem(atlas, "distanceRange")->valuedouble;

    int glyph_count = cJSON_GetArraySize(glyphs_json);

    char png_path[512];
    strcpy(png_path, input_json);
    strcpy(strrchr(png_path, '.'), ".png");

    int img_w, img_h, img_c;
    unsigned char* pixels = stbi_load(png_path, &img_w, &img_h, &img_c, 1);
    if (!pixels) {
        cJSON_Delete(root);
        return ERR_INTERNAL;
    }

    struct px_sdf_glyph* glyphs = (struct px_sdf_glyph*)calloc(glyph_count, sizeof(*glyphs));
    if (!glyphs) {
        cJSON_Delete(root);
        stbi_image_free(pixels);
        return ERR_ALLOC_FAILED;
    }

    for (int i = 0; i < glyph_count; i++) {
        cJSON* g = cJSON_GetArrayItem(glyphs_json, i);
        if (!g) {
            fprintf(stderr, "Glyph [%d] itself is missing!\n", i);
            cJSON_Delete(root);
            stbi_image_free(pixels);
            return ERR_INTERNAL;
        }

        cJSON* plane = cJSON_GetObjectItem(g, "planeBounds"); 
        cJSON* atlasb = cJSON_GetObjectItem(g, "atlasBounds");
        struct px_sdf_glyph* out = &glyphs[i];

        if (!plane || !atlasb) {
            memset(out, 0, sizeof(*out));
            continue;
        }

        cJSON* unicode = cJSON_GetObjectItem(g, "unicode");
        if (!unicode) {
            cJSON* index = cJSON_GetObjectItem(g, "index");
            if (!index) {
                fprintf(stderr, "Glyph [%d] missing both unicode and index!\n", i);
                cJSON_Delete(root);
                stbi_image_free(pixels);
                return ERR_INTERNAL;
            }
            out->codepoint = index->valueint;
        } else {
            out->codepoint = unicode->valueint;
        }
        cJSON* advance = cJSON_GetObjectItem(g, "advance");
        if (!advance) {
            fprintf(stderr, "Glyph [%d] missing advance!\n", i);
            cJSON_Delete(root);
            stbi_image_free(pixels);
            return ERR_INTERNAL;
        }
        out->advance = advance->valuedouble * desc->pixel_size;
        cJSON* leftj = cJSON_GetObjectItem(plane, "left");
        cJSON* rightj = cJSON_GetObjectItem(plane, "right");
        cJSON* topj = cJSON_GetObjectItem(plane, "top");
        cJSON* bottomj = cJSON_GetObjectItem(plane, "bottom");

        if (!leftj || !rightj || !topj || !bottomj) {
            fprintf(stderr, "Glyph [%d] missing left/right/top/bottom!\n", i);
            cJSON_Delete(root);
            stbi_image_free(pixels);
            return ERR_INTERNAL;
        }

        float left = (float)leftj->valuedouble;
        float right = (float)rightj->valuedouble;
        float top = (float)topj->valuedouble;
        float bottom = (float)bottomj->valuedouble;

        out->bearing_x = left * desc->pixel_size;
        out->bearing_y = top  * desc->pixel_size;
        out->width = (right - left) * desc->pixel_size;
        out->height = (top - bottom) * desc->pixel_size;

        leftj = cJSON_GetObjectItem(atlasb, "left");
        rightj = cJSON_GetObjectItem(atlasb, "right");
        topj = cJSON_GetObjectItem(atlasb, "top");
        bottomj = cJSON_GetObjectItem(atlasb, "bottom");

        if (!leftj || !rightj || !topj || !bottomj) {
            fprintf(stderr, "Glyph [%d]->atlasBounds missing left/right/top/bottom!\n", i);
            cJSON_Delete(root);
            stbi_image_free(pixels);
            return ERR_INTERNAL;
        }

        float al = (float)leftj->valuedouble;
        float ar = (float)rightj->valuedouble;
        float ab = (float)bottomj->valuedouble;
        float at = (float)topj->valuedouble;

        out->u0 = al / atlas_w;
        out->v0 = ab / atlas_h;
        out->u1 = ar / atlas_w;
        out->v1 = at / atlas_h;
    }

    FILE* f = fopen(output_psdf, "wb");
    if (!f) return ERR_COULD_NOT_OPEN_FILE;

    cJSON* ascender = cJSON_GetObjectItem(metrics, "ascender");
    cJSON* descender = cJSON_GetObjectItem(metrics, "descender");
    cJSON* lineHeight = cJSON_GetObjectItem(metrics, "lineHeight");

    if (!ascender || !descender || !lineHeight) {
        fprintf(stderr, "Metrics is missing Ascender/Descender/Line height\n");
        cJSON_Delete(root);
        stbi_image_free(pixels);
        return ERR_INTERNAL;
    }

    struct px_sdf_header h = {
        .magic = PX_SDF_MAGIC,
        .version = 1,
        .glyph_count = glyph_count,
        .atlas_width = atlas_w,
        .atlas_height = atlas_h,
        .ascent = (float)ascender->valuedouble * desc->pixel_size,
        .descent = (float)descender->valuedouble * desc->pixel_size,
        .line_gap = (float)lineHeight->valuedouble * desc->pixel_size,
        .sdf_range = sdf_range
    };

    fwrite(&h, sizeof(h), 1, f);
    fwrite(glyphs, sizeof(*glyphs), glyph_count, f);
    fwrite(pixels, atlas_w * atlas_h, 1, f);
    fclose(f);

    free(glyphs);
    stbi_image_free(pixels);
    cJSON_Delete(root);

    return ERR_SUCCESS;
}

