#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/internal.h>

#include <loaders/sdf-loader.h>
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

	PX_Texture tex = {
		.type = PX_RS_TEXTURE_TYPE_2D,
		.format = PX_RS_TEXTURE_FORMAT_R8UNORM,
		.width = h.atlas_width,
		.height = h.atlas_height,
		.mip_levels = 1,
		.samples = 1,
	};

	t_err_codes err = px_rs_create_texture(&tex);
	if (err != ERR_SUCCESS) {
		free(pixels);
		free(glyphs);
		return err;
	}

	PX_Sampler sampler = {
		.min_filter = PX_RS_TEXTURE_FILTER_LINEAR,
		.mag_filter = PX_RS_TEXTURE_FILTER_LINEAR,
		
		.address_u = PX_RS_TEXTURE_ADDRESS_CLAMP_TO_EDGE,
		.address_v = PX_RS_TEXTURE_ADDRESS_CLAMP_TO_EDGE,
		.address_w = PX_RS_TEXTURE_ADDRESS_CLAMP_TO_EDGE,
	};

	err = px_rs_set_sampler(&tex, &sampler);
	if (err != ERR_SUCCESS) {
		px_rs_destroy_texture(&tex);
		free(pixels);
		free(glyphs);
		return err;
	}

	err = px_rs_upload_texture(&tex, PX_RS_TEXTURE_FORMAT_R8UNORM, 0, pixels);
	if (err != ERR_SUCCESS) {
		px_rs_destroy_texture(&tex);
		px_rs_destroy_sampler(&sampler);
		free(pixels);
		free(glyphs);
		return err;
	}

    free(pixels);

    out->texture = tex;
	out->sampler = sampler;
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

	px_rs_destroy_texture(&data->texture);
	px_rs_destroy_sampler(&data->sampler);
    
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

PX_GPU_Handle px_sdf_get_texture(const PX_Font* font, PX_GPU_Handle* sampler_out) {
    if (!font || !sampler_out) return 0;
	if (font->backend != PX_FONT_BACKEND_SDF) return 0;

	*sampler_out = font->impl.sdf.texture.sampler_handle;
    return font->impl.sdf.texture.handle;
}
