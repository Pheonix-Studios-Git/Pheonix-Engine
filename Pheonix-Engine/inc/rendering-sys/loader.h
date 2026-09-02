#pragma once

#include <rendering-sys.h>
#include <font.h>
#include <err-codes.h>

size_t px_rs_loader_load_file(PX_Scene* s, const char* path);
void px_rs_loader_destroy_load(PX_Scene* s, PX_3D_Object* object);
t_err_codes px_rs_loader_export_load(PX_Scene* s, PX_3D_Object* object, const char* out_path);