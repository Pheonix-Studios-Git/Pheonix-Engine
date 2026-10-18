#include <core/stdlib_pstdlib_connector.h>
#include <pstdlib.h>
#include <pio.h>

#include <pheonix-engine.h>
#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/internal.h>

#include <loaders/sdf-loader.h>
#include <err-codes.h>

t_err_codes px_sdf_load(const char* path, struct px_sdf_font_data* out) {
    PIO_Stream* f = sopen_file(path, PSTREAM_FLAG_READ | PSTREAM_FLAG_BINARY);
    if (!f) return ERR_COULD_NOT_OPEN_FILE;

    struct px_sdf_header h;
    sread(f, &h, sizeof(h));

    if (h.magic != PX_SDF_MAGIC) {
        sclose(f);
        return ERR_MAGIC_INVALID;
    }

    if (h.version > PX_SDF_CUR_VERSION) {
        sclose(f);
        return ERR_VERSION_INVALID;
    }

    struct px_sdf_glyph* glyphs = (struct px_sdf_glyph*)alloc(sizeof(*glyphs) * h.glyph_count);
    if (!glyphs) {
        sclose(f);
        return ERR_ALLOC_FAILED;
    }
    sread(f, glyphs, sizeof(*glyphs) * h.glyph_count);

    size_t atlas_size = h.atlas_width * h.atlas_height;
    unsigned char* pixels = (unsigned char*)alloc(atlas_size);
    if (!pixels) {
        dealloc(glyphs);
        sclose(f);
        return ERR_ALLOC_FAILED;
    }
    sread(f, pixels, atlas_size);

    sclose(f);

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
		dealloc(pixels);
		dealloc(glyphs);
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
		dealloc(pixels);
		dealloc(glyphs);
		return err;
	}

	err = px_rs_upload_texture(&tex, PX_RS_TEXTURE_FORMAT_R8UNORM, 0, pixels);
	if (err != ERR_SUCCESS) {
		px_rs_destroy_texture(&tex);
		px_rs_destroy_sampler(&sampler);
		dealloc(pixels);
		dealloc(glyphs);
		return err;
	}

    dealloc(pixels);

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

void px_sdf_dealloc(struct px_sdf_font_data* data) {
    if (!data) return;

	px_rs_destroy_texture(&data->texture);
	px_rs_destroy_sampler(&data->sampler);
    
    dealloc(data->glyphs);
    fillbuf(data, 0, sizeof(*data));
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
