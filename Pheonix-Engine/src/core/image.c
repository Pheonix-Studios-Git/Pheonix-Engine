#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <png.h>

#include <core/image.h>
#include <err-codes.h>

static png_structp png_ptr;
static png_infop info_ptr;
static bool initialized_png_ptr = false;
static bool initialized_info_ptr = false;

void image_free_png(void) {
    if (!initialized_info_ptr && initialized_png_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        initialized_png_ptr = false;
        return;
    } else if (!initialized_png_ptr && initialized_info_ptr) {
        png_destroy_read_struct(NULL, &info_ptr, NULL);
        initialized_info_ptr = false;
        return;
    } else if (!initialized_png_ptr && !initialized_info_ptr) {
        return;
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    initialized_info_ptr = false;
    initialized_png_ptr = false;
}

t_err_codes image_init_png(void) {
    if (!initialized_png_ptr) {
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) return ERR_ALLOC_FAILED;
        initialized_png_ptr = true;
    }
    if (!initialized_info_ptr) {
        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            image_free_png();
            return ERR_ALLOC_FAILED;
        }
        initialized_info_ptr = true;
    }
    return ERR_SUCCESS;
}

t_err_codes image_get_png(const char* file, int* w, int* h, int* bit_depth, int* color_type, unsigned char** data) {
    FILE* f = fopen(file, "rb");
    if (!f)
        return ERR_COULD_NOT_OPEN_FILE;
    unsigned char sig[8];
    fread(sig, 1, 8, f);
    if (!png_check_sig(sig, 8)) {
        fclose(f);
        return ERR_MAGIC_INVALID;
    }

    if (!initialized_png_ptr || !initialized_info_ptr)
        image_init_png();

    if (setjmp(png_jmpbuf(png_ptr))) {
        image_free_png();
        fclose(f);
        return ERR_INTERNAL;
    }

    png_init_io(png_ptr, f);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)w, (png_uint_32*)h, bit_depth, color_type, NULL, NULL, NULL);

    png_uint_32 i, row_bytes;
    png_bytep* row_pointers = malloc(sizeof(png_bytep) * (*h));
    if (!row_pointers) {
        image_free_png();
        fclose(f);
        return ERR_ALLOC_FAILED;
    }

    png_set_expand(png_ptr);
    png_set_strip_16(png_ptr);
    if (!(*color_type & PNG_COLOR_MASK_ALPHA)) {
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    } else {
        png_set_tRNS_to_alpha(png_ptr);
    }
    png_set_gray_to_rgb(png_ptr);
    png_read_update_info(png_ptr, info_ptr);
    row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if ((*data = (unsigned char*)malloc(row_bytes * (*h))) == NULL){
        image_free_png();
        fclose(f);
        return ERR_INTERNAL;
    }
    for (i = 0; i < (png_uint_32)*h; i++) {
        row_pointers[i] = *data + i*row_bytes;
    }
    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);
    fclose(f);

    return ERR_SUCCESS;
}
