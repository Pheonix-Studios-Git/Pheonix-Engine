#pragma once

#include <err-codes.h>

t_err_codes image_init_png(void);
void image_free_png(void);
t_err_codes image_get_png(const char* file, int* w, int* h, int* bit_depth, int* color_type, unsigned char** data);
