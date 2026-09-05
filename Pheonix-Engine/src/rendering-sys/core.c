#include <stdlib.h>
#include <string.h>
#include <cglm/cglm.h>

#include <decoders/unicode.h>
#include <event-sys/keycodes.h>
#include <loaders/sdf-loader.h>
#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/internal.h>

#define __PHEONIX_ENGINE__RENDERING_SYS__OPENGL_NO_INC__
#include <rendering-sys/opengl.h>
#define __PHEONIX_ENGINE__RENDERING_SYS__VULKAN_NO_INC__
#include <rendering-sys/vulkan.h>

#include <pheonix-engine.h>

struct batch_sys_2d gr_batch_2d_raw = {0};
struct batch_sys_3d gr_batch_3d_raw = {0};
struct batch_sys_2d* gr_batch_2d = &gr_batch_2d_raw;
struct batch_sys_3d* gr_batch_3d = &gr_batch_3d_raw;

struct scene_cam_3d gscene_cam_3d = {0};
struct scene_cam_2d gscene_cam_2d = {.zoom=10.0f};

PX_AnchorRect viewport_3d = {0};
PX_AnchorRect viewport_2d = {0};

static PX_GPU_Backend current_backend = __PHEONIX_ENGINE__RENDERING_SYS__DEFAULT_BACKEND__;
static bool backend_initialized = false;

static PX_Transform3 combine_transform3(PX_Transform3 parent, PX_Transform3 local) {
    PX_Transform3 out = {0};

    out.scale.w = parent.scale.w * local.scale.w;
    out.scale.h = parent.scale.h * local.scale.h;
    out.scale.l = parent.scale.l * local.scale.l;

    versor p = {
        parent.rot.x,
        parent.rot.y,
        parent.rot.z,
        parent.rot.w
    };

    versor l = {
        local.rot.x,
        local.rot.y,
        local.rot.z,
        local.rot.w
    };

    versor r;
    glm_quat_mul(p, l, r);

    out.rot.x = r[0];
    out.rot.y = r[1];
    out.rot.z = r[2];
    out.rot.w = r[3];

    vec3 lp = {
        local.pos.x,
        local.pos.y,
        local.pos.z
    };

    lp[0] *= parent.scale.w;
    lp[1] *= parent.scale.h;
    lp[2] *= parent.scale.l;

    glm_quat_rotatev(p, lp, lp);

    out.pos.x = parent.pos.x + lp[0];
    out.pos.y = parent.pos.y + lp[1];
    out.pos.z = parent.pos.z + lp[2];

    return out;
}

static PX_Transform2 combine_transform2(PX_Transform2 parent, PX_Transform2 local) {
    PX_Transform2 out = {0};

    out.scale.w = parent.scale.w * local.scale.w;
    out.scale.h = parent.scale.h * local.scale.h;

    out.rot = parent.rot + local.rot;

    float x = local.pos.x * (float)parent.scale.w;
    float y = local.pos.y * (float)parent.scale.h;

    float c = cosf(parent.rot);
    float s = sinf(parent.rot);

    float rotated_x = x * c - y * s;
    float rotated_y = x * s + y * c;

    out.pos.x = parent.pos.x + (int)roundf(rotated_x);
    out.pos.y = parent.pos.y + (int)roundf(rotated_y);

    return out;
}

static PX_Transform2 combine_transform2_as_container(PX_Transform2 parent, PX_Transform2 local) {
    PX_Transform2 out = {0};

    out.scale.w = local.scale.w;
    out.scale.h = local.scale.h;
	out.rot = parent.rot + local.rot;

	float c = cosf(parent.rot);
    float s = sinf(parent.rot);

	float rotated_x = local.pos.x * c - local.pos.y * s;
    float rotated_y = local.pos.x * s + local.pos.y * c;

    out.pos.x = parent.pos.x + rotated_x;
    out.pos.y = parent.pos.y + rotated_y;
    return out;
}

static PX_Vector2 transform_point_2d(PX_Vector2 point, PX_Transform2 transform) {
    float x = (float)point.x * (float)transform.scale.w;
    float y = (float)point.y * (float)transform.scale.h;

    float c = cosf(transform.rot);
    float s = sinf(transform.rot);

    PX_Vector2 out;

    out.x = transform.pos.x + x * c - y * s;
    out.y = transform.pos.y + x * s + y * c;
    return out;
}

static PX_Transform2 apply_camera_2d(PX_Transform2 transform) {
    transform.pos.x -= gscene_cam_2d.position[0];
    transform.pos.y -= gscene_cam_2d.position[1];

    transform.pos.x *= gscene_cam_2d.zoom;
    transform.pos.y *= gscene_cam_2d.zoom;

    transform.scale.w *= gscene_cam_2d.zoom;
    transform.scale.h *= gscene_cam_2d.zoom;
    return transform;
}

static bool is_transform_visible(PX_Transform2 transform, PX_Transform2 container, float zoom) {
    if (zoom <= 0.0f) return false;

    float c_x = container.pos.x;
    float c_y = container.pos.y;

    float half_w = transform.scale.w * 0.5f;
    float half_h = transform.scale.h * 0.5f;

    float left = transform.pos.x - half_w;
    float right = transform.pos.x + half_w;
    float top = transform.pos.y - half_h;
    float bottom = transform.pos.y + half_h;

    float c_w = container.scale.w / zoom;
    float c_h = container.scale.h / zoom;

    float c_left = c_x;
    float c_right = c_x + c_w;
    float c_top = c_y;
    float c_bottom = c_y + c_h;

    if (right < c_left) return false;
    if (left > c_right) return false;
    if (bottom < c_top) return false;
    if (top > c_bottom) return false;

    return true;
}

static void push_3d_grid(PX_EditorGrid_3D* grid, PX_Transform3 transform, struct batch_3d* b) {
	if (!b) return;

    memset(b, 0, sizeof(struct batch_3d));
    memcpy(&b->transform, &transform, sizeof(PX_Transform3));
    
    b->color = grid->color;
    b->line_width = 1.0f;

    b->type = BATCH_3D_LINES;
	b->visible = true;
    b->vertex_offset = gr_batch_3d->vertex_count;
    b->index_offset = gr_batch_3d->index_count;

    const float spacing = grid->spacing;
    const float half = grid->half_size;

    float cam_x = floorf(gscene_cam_3d.position[0] / spacing) * spacing;
    float cam_z = floorf(gscene_cam_3d.position[2] / spacing) * spacing;

    float extent = half * spacing;

    for (float i = -half; i <= half; i++) {
        if (gr_batch_3d->vertex_count + 4 >= gr_batch_3d->vertex_capacity)
            break;

        if (gr_batch_3d->index_count + 4 >= gr_batch_3d->index_capacity)
            break;

        uint32_t base = (uint32_t)gr_batch_3d->vertex_count;

        float p = i * spacing;

        float x = cam_x + p;
        float z = cam_z + p;

        struct vertex_3d v0 = {
            .x = x,
            .y = -0.1f,
            .z = cam_z - extent
        };

        struct vertex_3d v1 = {
            .x = x,
            .y = 0.0f,
            .z = cam_z + extent
        };

        struct vertex_3d v2 = {
            .x = cam_x - extent,
            .y = 0.0f,
            .z = z
        };

        struct vertex_3d v3 = {
            .x = cam_x + extent,
            .y = 0.0f,
            .z = z
        };

        gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v0;
        gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v1;
        gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v2;
        gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v3;

        gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 0;
        gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 1;

        gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 2;
        gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 3;
    }

    b->vertex_count = gr_batch_3d->vertex_count - b->vertex_offset;
    b->index_count = gr_batch_3d->index_count - b->index_offset;
}

static void push_3d_line(PX_Color4 color, PX_Transform3 transform, PX_Vector3 endpos, float width, struct batch_3d* b) {
	if (!b) return;
    memset(b, 0, sizeof(struct batch_3d));
   
    if (gr_batch_3d->vertex_count + 2 >= gr_batch_3d->vertex_capacity) return;
    if (gr_batch_3d->index_count + 2 >= gr_batch_3d->index_capacity) return;
   
    memcpy(&b->transform, &transform, sizeof(PX_Transform3));
    
    b->color = color;

    b->type = BATCH_3D_LINES;
	b->visible = true;
    b->line_width = width;
    b->vertex_offset = gr_batch_3d->vertex_count;
    b->index_offset = gr_batch_3d->index_count;

    uint32_t base = (uint32_t)gr_batch_3d->vertex_count;

    struct vertex_3d v0 = {
        .x = transform.pos.x,
        .y = transform.pos.y,
        .z = transform.pos.z
    };

    struct vertex_3d v1 = {
        .x = endpos.x,
        .y = endpos.y,
        .z = endpos.z
    };

    gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v0;
    gr_batch_3d->vertices[gr_batch_3d->vertex_count++] = v1;

    gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 0;
    gr_batch_3d->indices[gr_batch_3d->index_count++] = base + 1;

    b->vertex_count = gr_batch_3d->vertex_count - b->vertex_offset;
    b->index_count = gr_batch_3d->index_count - b->index_offset;
}

static void push_2d_quad(PX_Transform2 container, PX_Transform2 t, PX_Color4 c, struct batch_2d* b, bool fixed_on_screen) {
    if (!b) return;
    if (gr_batch_2d->vertex_count + 4 > gr_batch_2d->vertex_capacity) return;

    memset(b, 0, sizeof(struct batch_2d));

    b->type = BATCH_2D_PANEL;
	b->visible = true;

	PX_Transform2 transform = fixed_on_screen ? t : apply_camera_2d(t);
	if (!is_transform_visible(transform, container, fixed_on_screen ? 1.0f : gscene_cam_2d.zoom)) {
		b->visible = false;
		return;
	}

	b->vertex_offset = gr_batch_2d->vertex_count;
    b->vertex_count = 4;

	float pos_x = transform.pos.x;
    float pos_y = transform.pos.y;

    float c_rot = cosf(transform.rot);
    float s_rot = sinf(transform.rot);

	const float cx = transform.scale.w * 0.5f;
    const float cy = transform.scale.h * 0.5f;

    float corners[4][2] = {
        {0.0f, 0.0f},
        {transform.scale.w, 0.0f},
        {transform.scale.w, transform.scale.h},
        {0.0f, transform.scale.h}
    };

    struct vertex_2d* v = gr_batch_2d->vertices + gr_batch_2d->vertex_count;
    for (int i = 0; i < 4; ++i) {
        float x = corners[i][0] - cx;
        float y = corners[i][1] - cy;

        float rx = x * c_rot - y * s_rot;
        float ry = x * s_rot + y * c_rot;

        v[i] = (struct vertex_2d){
            .x = transform.pos.x + cx + rx,
            .y = transform.pos.y + cy + ry,
            .u = (i == 1 || i == 2) ? 1.0f : 0.0f,
            .v = (i >= 2) ? 1.0f : 0.0f,
            .r = c.r,
            .g = c.g,
            .b = c.b,
            .a = c.a
        };
    }

    gr_batch_2d->vertex_count += 4;
}

static void push_2d_glyph(float x0, float y0, float x1, float y1, struct px_sdf_glyph* g, PX_Color4 c) {
	if (!g) return;
    if (gr_batch_2d->vertex_count + 6 > gr_batch_2d->vertex_capacity) return;

    struct vertex_2d* v = gr_batch_2d->vertices + gr_batch_2d->vertex_count;

    v[0] = (struct vertex_2d){x0, y0, g->u0, g->v0, c.r, c.g, c.b, c.a};
    v[1] = (struct vertex_2d){x1, y0, g->u1, g->v0, c.r, c.g, c.b, c.a};
    v[2] = (struct vertex_2d){x1, y1, g->u1, g->v1, c.r, c.g, c.b, c.a};
    v[3] = (struct vertex_2d){x0, y1, g->u0, g->v1, c.r, c.g, c.b, c.a};

    gr_batch_2d->vertex_count += 4;
}

static void push_2d_line(PX_Color4 color, PX_Vector2 startpos, PX_Vector2 endpos, float width, struct batch_2d* b) {
	if (!b || width < 1.0f) return;
	if (gr_batch_2d->vertex_count + 2 >= gr_batch_2d->vertex_capacity) return;
	
    memset(b, 0, sizeof(struct batch_2d));
	b->type = BATCH_2D_LINE;
	b->visible = true;
	b->line_width = width;
	b->vertex_offset = gr_batch_2d->vertex_count;
    uint32_t base = (uint32_t)gr_batch_2d->vertex_count;

    struct vertex_2d v0 = {
        .x = startpos.x,
        .y = startpos.y,
        .u = 0.0f,
        .v = 0.0f,
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a
    };

    struct vertex_2d v1 = {
        .x = endpos.x,
        .y = endpos.y,
        .u = 0.0f,
        .v = 0.0f,
        .r = color.r,
        .g = color.g,
        .b = color.b,
        .a = color.a
    };

    gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v0;
    gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v1;

    b->vertex_count = gr_batch_2d->vertex_count - b->vertex_offset;
}

static void push_2d_grid(PX_EditorGrid_2D* grid, PX_Transform2 container, PX_Transform2 t, struct batch_2d* b, bool fixed_on_screen) {
    if (!grid || !b) return;
    memset(b, 0, sizeof(struct batch_2d));

    b->type = BATCH_2D_LINE;
	b->visible = true;
	
	PX_Transform2 transform = fixed_on_screen ? t : apply_camera_2d(t);
	
	b->line_width = 1.0f;
    b->vertex_offset = gr_batch_2d->vertex_count;

    const float spacing = grid->spacing;

	float half_view_x = ceilf((container.scale.w / gscene_cam_2d.zoom) / spacing);
	float half_view_y = ceilf((container.scale.h / gscene_cam_2d.zoom) / spacing);
	float half_view = fmaxf(half_view_x, half_view_y);

    float cam_x = floorf(gscene_cam_2d.position[0] / spacing) * spacing;
    float cam_y = floorf(gscene_cam_2d.position[1] / spacing) * spacing;

    float extent = half_view * spacing;

    for (float i = -half_view; i <= half_view; i++) {
        if (gr_batch_2d->vertex_count + 4 >= gr_batch_2d->vertex_capacity) break;

        float p = i * spacing;

		PX_Vector2 v0_pos = {
			.x = ((cam_x + p) - gscene_cam_2d.position[0]) * gscene_cam_2d.zoom + container.pos.x,
			.y = ((cam_y - extent) - gscene_cam_2d.position[1]) * gscene_cam_2d.zoom + container.pos.y
		};
		PX_Vector2 v1_pos = {
			.x = ((cam_x + p) - gscene_cam_2d.position[0]) * gscene_cam_2d.zoom + container.pos.x,
			.y = ((cam_y + extent) - gscene_cam_2d.position[1]) * gscene_cam_2d.zoom + container.pos.y
		};
		PX_Vector2 v2_pos = {
			.x = ((cam_x - extent) - gscene_cam_2d.position[0]) * gscene_cam_2d.zoom + container.pos.x,
			.y = ((cam_y + p) - gscene_cam_2d.position[1]) * gscene_cam_2d.zoom + container.pos.y
		};
		PX_Vector2 v3_pos = {
			.x = ((cam_x + extent) - gscene_cam_2d.position[0]) * gscene_cam_2d.zoom + container.pos.x,
			.y = ((cam_y + p) - gscene_cam_2d.position[1]) * gscene_cam_2d.zoom + container.pos.y
		};

        struct vertex_2d v0 = {
            .x = v0_pos.x,
            .y = v0_pos.y,
            .u = 0.0f,
            .v = 0.0f,
            .r = grid->color.r,
            .g = grid->color.g,
            .b = grid->color.b,
            .a = grid->color.a
        };

        struct vertex_2d v1 = {
            .x = v1_pos.x,
            .y = v1_pos.y,
            .u = 0.0f,
            .v = 0.0f,
            .r = grid->color.r,
            .g = grid->color.g,
            .b = grid->color.b,
            .a = grid->color.a
        };

        struct vertex_2d v2 = {
            .x = v2_pos.x,
            .y = v2_pos.y,
            .u = 0.0f,
            .v = 0.0f,
            .r = grid->color.r,
            .g = grid->color.g,
            .b = grid->color.b,
            .a = grid->color.a
        };

        struct vertex_2d v3 = {
            .x = v3_pos.x,
            .y = v3_pos.y,
            .u = 0.0f,
            .v = 0.0f,
            .r = grid->color.r,
            .g = grid->color.g,
            .b = grid->color.b,
            .a = grid->color.a
        };

        gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v0;
        gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v1;
        gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v2;
        gr_batch_2d->vertices[gr_batch_2d->vertex_count++] = v3;
    }

    b->vertex_count = gr_batch_2d->vertex_count - b->vertex_offset;
}

static void push_2d_batch(struct batch_2d* b) {
	if (!b) return;
	if (!b->visible) return; // Don't Push

    if (gr_batch_2d->batch_count > 0) {
        struct batch_2d* last_b = &gr_batch_2d->batches[gr_batch_2d->batch_count - 1];

        if (last_b->type == b->type && last_b->texture == b->texture) {
            if (b->type == BATCH_2D_PANEL) {
                if (b->corner_radius == last_b->corner_radius && b->noise == last_b->noise) {
                    last_b->vertex_count += b->vertex_count;
                    return;
                }
            } else if (b->type == BATCH_2D_TEXT) {
                if (b->text_sdf_width == last_b->text_sdf_width && b->text_outline_width == last_b->text_outline_width &&
                    (b->text_outline_color.r == last_b->text_outline_color.r &&
                    b->text_outline_color.g == last_b->text_outline_color.g &&
                    b->text_outline_color.b == last_b->text_outline_color.b &&
                    b->text_outline_color.a == last_b->text_outline_color.a)
                ){
                    last_b->vertex_count += b->vertex_count;
                    return;
                }
            }
        }
    }

    if (gr_batch_2d->batch_count < MAX_BATCHES) {
        memcpy(&gr_batch_2d->batches[gr_batch_2d->batch_count++], b, sizeof(struct batch_2d));
    }
}

static void push_3d_batch(struct batch_3d* b) {
	if (!b) return;
	if (!b->visible) return; // Don't Push

    if (gr_batch_3d->batch_count >= MAX_BATCHES) return;

    memcpy(&gr_batch_3d->batches[gr_batch_3d->batch_count], b, sizeof(struct batch_3d));
    gr_batch_3d->batch_count++;
}

static PX_GPU_Handle get_flat_fbo(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_get_flat_fbo();
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_get_flat_fbo();
		default: return PX_RS_GPU_INVALID_HANDLE;
	}
}

static PX_GPU_Handle get_blank_tex(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_get_blank_tex();
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_get_blank_tex();
		default: return PX_RS_GPU_INVALID_HANDLE;
	}
}

t_err_codes px_rs_init(PX_WContext* ctx) {
	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init(ctx); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init(ctx); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	return code;
}

t_err_codes px_rs_init_3d(PX_AnchorRect viewport) {
	viewport_3d = viewport;

	gr_batch_3d->vertex_capacity = MAX_VERTEX_COUNT;
    gr_batch_3d->vertices = (struct vertex_3d*)malloc(sizeof(struct vertex_3d) * gr_batch_3d->vertex_capacity);
    if (!gr_batch_3d->vertices) {
        return ERR_ALLOC_FAILED;
    }

    gr_batch_3d->index_capacity = MAX_3D_INDICES;
    gr_batch_3d->indices = (uint32_t*)malloc(sizeof(uint32_t) * gr_batch_3d->index_capacity);
    if (!gr_batch_3d->indices) {
        free(gr_batch_3d->vertices);
        return ERR_ALLOC_FAILED;
    }

	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init_3d(viewport); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init_3d(viewport); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	else {
		gr_batch_3d->vertex_capacity = 0;
		free(gr_batch_3d->vertices);
		gr_batch_3d->vertices = NULL;

		gr_batch_3d->index_capacity = 0;
		free(gr_batch_3d->indices);
		gr_batch_3d->indices = NULL;
	}
	return code;
}

t_err_codes px_rs_init_2d(PX_AnchorRect viewport) {
	viewport_2d = viewport;

	gr_batch_2d->vertex_capacity = MAX_VERTEX_COUNT;
    gr_batch_2d->vertices = (struct vertex_2d*)malloc(sizeof(struct vertex_2d) * gr_batch_2d->vertex_capacity);
    if (!gr_batch_2d->vertices) {
        return ERR_ALLOC_FAILED;
    }

	t_err_codes code = ERR_FAILURE;
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: code = px_rs_gl_init_2d(viewport); break;
		case PX_RS_GPU_BACKEND_VULKAN: code = px_rs_vk_init_2d(viewport); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (code == ERR_SUCCESS) backend_initialized = true;
	else {
		gr_batch_2d->vertex_capacity = 0;
		free(gr_batch_2d->vertices);
		gr_batch_2d->vertices = NULL;
	}
	return code;
}

void px_rs_shutdown_2d(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_2d(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_2d(); break;
		default: return;
	}
}

void px_rs_shutdown_3d(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown_3d(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown_3d(); break;
		default: return;
	}
}

void px_rs_shutdown(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_shutdown(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_shutdown(); break;
		default: return;
	}

	if (gr_batch_3d->vertices) free(gr_batch_3d->vertices);
    if (gr_batch_3d->indices) free(gr_batch_3d->indices);

	backend_initialized = false;
}

void px_rs_frame_start(void) {
	gr_batch_2d->vertex_count = 0;
    gr_batch_2d->batch_count = 0;
    
    gr_batch_3d->vertex_count = 0;
    gr_batch_3d->index_count = 0;
    gr_batch_3d->batch_count = 0;

	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_start(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_start(); break;
		default: return;
	}
}

void px_rs_frame_end(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_end(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_end(); break;
		default: return;
	}
}

void px_rs_2d_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_2d_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_2d_frame_update(); break;
		default: return;
	}
}

void px_rs_3d_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_frame_update(); break;
		default: return;
	}
}

void px_rs_frame_update(void) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_frame_update(); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_frame_update(); break;
		default: return;
	}
}

void px_rs_2d_resize(PX_AnchorRect viewport) {
	viewport_2d = viewport;

	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_2d_resize(viewport); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_2d_resize(viewport); break;
		default: return;
	}
}

void px_rs_3d_resize(PX_AnchorRect viewport) {
	viewport_3d = viewport;

	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_3d_resize(viewport); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_3d_resize(viewport); break;
		default: return;
	}
}

void px_rs_update_scene_cam_3d(PX_Vector2 mdelta, PX_EKeycodes key) {
    gscene_cam_3d.yaw += mdelta.x * gscene_cam_3d.mouse_sens;
    gscene_cam_3d.pitch -= mdelta.y * gscene_cam_3d.mouse_sens;

    if (gscene_cam_3d.pitch > 89.0f) gscene_cam_3d.pitch = 89.0f;
    if (gscene_cam_3d.pitch < -89.0f) gscene_cam_3d.pitch = -89.0f;

    vec3 forward;
    forward[0] = cos(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    forward[1] = sin(glm_rad(gscene_cam_3d.pitch));
    forward[2] = sin(glm_rad(gscene_cam_3d.yaw)) * cos(glm_rad(gscene_cam_3d.pitch));
    glm_normalize(forward);

    vec3 right;
    glm_vec3_cross(forward, gscene_cam_3d.up, right);
    glm_normalize(right);

    float speed = gscene_cam_3d.move_speed;

    switch (key) {
        case EKeycode_W:
            glm_vec3_muladds(forward, speed, gscene_cam_3d.position);
            break;

        case EKeycode_S:
            glm_vec3_muladds(forward, -speed, gscene_cam_3d.position);
            break;

        case EKeycode_D:
            glm_vec3_muladds(right, speed, gscene_cam_3d.position);
            break;

        case EKeycode_A:
            glm_vec3_muladds(right, -speed, gscene_cam_3d.position);
            break;

        case EKeycode_Space:
            gscene_cam_3d.position[1] += speed;
            break;

        case EKeycode_LControl:
            gscene_cam_3d.position[1] -= speed;
            break;

        default:
            break;
    }
}

void px_rs_update_scene_cam_2d(PX_Vector2 mdelta, PX_EKeycodes key) {
    float speed = gscene_cam_2d.move_speed;

    switch (key) {
        case EKeycode_W:
            gscene_cam_2d.position[1] -= speed;
            break;

        case EKeycode_S:
            gscene_cam_2d.position[1] += speed;
            break;

        case EKeycode_D:
            gscene_cam_2d.position[0] += speed;
            break;

        case EKeycode_A:
            gscene_cam_2d.position[0] -= speed;
            break;

		case EKeycode_MouseScrollUp:
		case EKeycode_MouseScrollDown: {
			float old_zoom = gscene_cam_2d.zoom;
			float zoom_factor = (key == EKeycode_MouseScrollUp) ? 1.1f : (1.0f / 1.1f);
			float new_zoom = old_zoom * zoom_factor;

			if (new_zoom > 20.0f) new_zoom = 20.0f;
			if (new_zoom < 0.5f) new_zoom = 0.5f;

			// MDelta here is just mouse coords
			float mouse_x = mdelta.x;
			float mouse_y = mdelta.y;

			PX_Transform2 viewport_2d_t = enginef_convert_anchor_to_transform(viewport_2d);

			float viewport_w = viewport_2d_t.scale.w;
			float viewport_h = viewport_2d_t.scale.h;

			float center_x = viewport_w * 0.5f;
			float center_y = viewport_h * 0.5f;

			float world_x = gscene_cam_2d.position[0] + (mouse_x - center_x) / old_zoom;
			float world_y = gscene_cam_2d.position[1] + (mouse_y - center_y) / old_zoom;

			gscene_cam_2d.zoom = new_zoom;
			gscene_cam_2d.position[0] = world_x - (mouse_x - center_x) / new_zoom;
			gscene_cam_2d.position[1] = world_y - (mouse_y - center_y) / new_zoom;
			break;
		}

        default: break;
    }
}

void px_rs_config_scene_cam_3d(float mouse_sensitivity, float speed) {
    if (mouse_sensitivity > 0.0f) gscene_cam_3d.mouse_sens = mouse_sensitivity;
    if (speed > 0.0f) gscene_cam_3d.move_speed = speed;
}

void px_rs_config_scene_cam_2d(float speed) {
    if (speed > 0.0f) gscene_cam_2d.move_speed = speed;
}

int px_rs_text_width(PX_Font* font, const char* text, float pixel_height) {
    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));
    float pen_x = 0.0f;

    for (const char* p = text; *p; ) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        pen_x += g->advance * scale;
    }

    return (int)(pen_x + 0.5f);
}

t_err_codes px_rs_draw_panel(PX_AnchorRect local_viewport, PX_Transform2 tran, PX_Color4 color, float noise, float cradius, bool fixed_on_screen) {
	PX_Transform2 lvt = enginef_convert_anchor_to_transform(local_viewport);
	PX_Transform2 ft = combine_transform2_as_container(lvt, tran);

	struct batch_2d b = {0};
    push_2d_quad(lvt, ft, color, &b, fixed_on_screen);

    b.type = BATCH_2D_PANEL;
    b.size = ft.scale;
    b.texel_size = (PX_Scale2){1, 1};
    b.corner_radius = cradius;
    b.noise = noise;
    b.texture = (PX_GPU_Handle)get_blank_tex();
	b.pure_color = false;
	b.switch_fbo = false;

    push_2d_batch(&b);

    return ERR_SUCCESS;
}

t_err_codes px_rs_render_text(const char* text, float pixel_height, PX_AnchorRect local_viewport, PX_Vector2 pos, PX_Color4 color, PX_Font* font) {
	if (!text) return ERR_INVALID_ARGUMENTS;
	PX_Transform2 lvt = enginef_convert_anchor_to_transform(local_viewport);
	PX_Vector2 fpos = (combine_transform2_as_container(lvt, (PX_Transform2){.pos=pos, .scale=(PX_Scale2){1}, .rot=0})).pos;

    int start_vertex = gr_batch_2d->vertex_count;
    float scale = pixel_height / (px_sdf_ascent(font) - px_sdf_descent(font));

    float pen_x = fpos.x;
    float pen_y = fpos.y + px_sdf_ascent(font) * scale;

    for (const char* p = text; *p;) {
        uint32_t cp = px_utf8_decode(&p);
        const struct px_sdf_glyph* g = px_sdf_find_glyph(font, cp);
        if (!g) continue;

        float y1 = pen_y - g->bearing_y * scale;
        float y0 = y1 + g->height * scale;
        float x0 = pen_x + g->bearing_x * scale;
        float x1 = x0 + g->width * scale;

        push_2d_glyph(
            x0,
            y0,
            x1,
            y1,
            (struct px_sdf_glyph*)g,
            color
        );

        pen_x += g->advance * scale;
    }
    int vertex_count = gr_batch_2d->vertex_count - start_vertex;

    float sdf_width = px_sdf_range(font) / pixel_height;
    sdf_width = fmaxf(0.015f, fminf(sdf_width, 0.03));

    struct batch_2d b = {0};
    b.type = BATCH_2D_TEXT;
	b.visible = true;
    b.text_sdf_width = sdf_width;
    b.text_pixel_height = pixel_height;
    b.text_outline_width = sdf_width * 2.0f;
    b.text_outline_color = (PX_Color4){0x00, 0x00, 0x00, 0xFF};
    b.texture = px_sdf_get_texture(font, &b.sampler);
	b.pure_color = false;
	b.switch_fbo = false;
    b.vertex_count = vertex_count;
    b.vertex_offset = start_vertex;

    push_2d_batch(&b);

    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_line(PX_AnchorRect local_viewport, PX_Vector2 start, PX_Vector2 end, float thickness, PX_Color4 color) {
	if (thickness < 1.0f) return ERR_INVALID_ARGUMENTS;

	PX_Transform2 lvt = enginef_convert_anchor_to_transform(local_viewport);
	PX_Vector2 fspos = (combine_transform2_as_container(lvt, (PX_Transform2){.pos=start, .scale=(PX_Scale2){1}, .rot=0})).pos;
	PX_Vector2 fepos = (combine_transform2_as_container(lvt, (PX_Transform2){.pos=end, .scale=(PX_Scale2){1}, .rot=0})).pos;

	struct batch_2d b = {0};
    push_2d_line(color, fspos, fepos, thickness, &b);

    b.type = BATCH_2D_LINE;
    b.size = (PX_Scale2){0,0};
    b.texel_size = (PX_Scale2){1,1};
    b.texture = (PX_GPU_Handle)get_blank_tex();
    b.noise = 0.0f;
    b.corner_radius = 0.0f;
	b.pure_color = false;
	b.switch_fbo = false;
	b.line_width = thickness;

    push_2d_batch(&b);
    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_dropdown(PX_AnchorRect local_viewport, PX_Dropdown* dd) {
	if (!dd) return ERR_INVALID_ARGUMENTS;
	if (!dd->visible) return ERR_SUCCESS;

	PX_Transform2 ddt = (PX_Transform2){.pos = dd->pos, .scale = {dd->width, dd->height}, .rot = 0.0f};

    PX_Color4 color = dd->color; 
    px_rs_draw_panel(local_viewport, ddt, color, dd->noise, dd->cradius, true);

    PX_Vector2 item_pos = dd->stext_pos;
    for (int i = 0; i < dd->item_count; i++) {
        PX_DropdownItem* item = &dd->items[i];
		PX_Vector2 absolute_item_pos = {dd->pos.x + item_pos.x, dd->pos.y + item_pos.y};
        PX_Color4 tcolor = dd->hover_index == i ? dd->hover_color : dd->text_color;

        px_rs_render_text(item->label, dd->font_size, local_viewport, (PX_Vector2){absolute_item_pos.x + item->stext_pos.x, absolute_item_pos.y + item->stext_pos.y}, tcolor, dd->font);
        if (item->is_open) {
			PX_Transform2 item_transform = {.pos = absolute_item_pos, .scale = {item->width, item->height}, .rot = 0.0f};
            PX_Transform2 option_panel_transform = combine_transform2_as_container(item_transform, item->panel_tran);

			px_rs_draw_panel(local_viewport, option_panel_transform, item->panel_color, item->panel_noise, item->panel_cradius, true);

            PX_Vector2 option_pos = item->stext_pos;
            for (int j = 0; j < item->option_count; j++) {
                PX_DropdownOption* option = &item->options[j];

                PX_Color4 ptcolor = item->hover_index == j ? item->hover_color : item->text_color;
                px_rs_render_text(option->label, item->font_size, local_viewport, (PX_Vector2){option_panel_transform.pos.x + option_pos.x, option_panel_transform.pos.y + option_pos.y}, ptcolor, dd->font);

                option_pos.y += item->spacing;
            }
        }

		item_pos.x += item->width + dd->spacing;
    }

    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_editor_objects_3d(PX_Scene_3D* scene) {
	if (!scene) return ERR_INVALID_ARGUMENTS;

	PX_Scale2 sscale = (PX_Scale2){0};
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: sscale = px_rs_gl_get_flat_fbo_scale(); break;
		case PX_RS_GPU_BACKEND_VULKAN: sscale = px_rs_vk_get_flat_fbo_scale(); break;
		default: return ERR_RS_INVALID_BACKEND;
	}

    for (size_t i = 0; i < scene->editor_object_count; i++) {
        struct batch_3d batch = {0};
        batch.depth_override = false;
        batch.switch_fbo = false;
        batch.pure_color = false;

        PX_3D_Editor_Object* obj = &scene->editor_objects[i];
        if (!obj->active) continue;
        PX_Transform3 localT = obj->local_transform;
        PX_Transform3 worldT = obj->world_transform;
        PX_Transform3 finalT = combine_transform3(worldT, localT);

        switch (obj->type){
            case PX_RS_OBJECT_3D_EDITOR_GRID: {
                if (!obj->ex_data) continue;
                push_3d_grid(obj->ex_data, worldT, &batch);
				push_3d_batch(&batch);
                break;
            }
            case PX_RS_OBJECT_3D_EDITOR_GIZMO: {
				if (!obj->ex_data) continue;
				
                PX_Vector3 GXep = (PX_Vector3){finalT.pos.x + 5, finalT.pos.y, finalT.pos.z};
                PX_Vector3 GYep = (PX_Vector3){finalT.pos.x, finalT.pos.y + 5, finalT.pos.z};
                PX_Vector3 GZep = (PX_Vector3){finalT.pos.x, finalT.pos.y, finalT.pos.z + 5};
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x75,0x0,0x0,0xFF}, finalT, GXep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0xFF,0x0,0x0,0xFF}, finalT, GXep, 4.0f, &batch);
                batch.depth_override = true;
                push_3d_batch(&batch);
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x0,0x75,0x0,0xFF}, finalT, GYep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0x0,0xFF,0x0,0xFF}, finalT, GYep, 4.0f, &batch);
                batch.depth_override = true;
                push_3d_batch(&batch);
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_3d_line((PX_Color4){0x0,0x0,0x75,0xFF}, finalT, GZep, 4.0f, &batch);
                else
                    push_3d_line((PX_Color4){0x0,0x0,0xFF,0xFF}, finalT, GZep, 4.0f, &batch);
                batch.depth_override = true;
                push_3d_batch(&batch);
                
                // Picker
                PX_Color3 main_color = {
                    .r = 0xFF, //obj->id & 0xFF,
                    .g = (obj->id >> 8) & 0xFF,
                    .b = obj->type
                };
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GXep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PX_GPU_Handle)get_flat_fbo();
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = sscale.w;
                batch.fbo_h = sscale.h;
                push_3d_batch(&batch);
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GYep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PX_GPU_Handle)get_flat_fbo();
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = sscale.w;
                batch.fbo_h = sscale.h;
                push_3d_batch(&batch);
                push_3d_line((PX_Color4){main_color.r, main_color.g, main_color.b, 0xFF}, finalT, GZep, 4.0f, &batch);
                batch.depth_override = true;
                batch.fbo = (PX_GPU_Handle)get_flat_fbo();
                batch.switch_fbo = true;
                batch.pure_color = true;
                batch.fbo_x = 0;
                batch.fbo_y = 0;
                batch.fbo_w = sscale.w;
                batch.fbo_h = sscale.h;
                push_3d_batch(&batch);
                break;
            }
            default: continue;
        }
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_editor_objects_2d(PX_AnchorRect local_viewport, PX_Scene_2D* scene) {
	if (!scene) return ERR_INVALID_ARGUMENTS;
	PX_Transform2 lvt = enginef_convert_anchor_to_transform(local_viewport);

    for (size_t i = 0; i < scene->editor_object_count; i++) {
        struct batch_2d batch = {0};
        batch.switch_fbo = false;
        batch.pure_color = false;

        PX_2D_Editor_Object* obj = &scene->editor_objects[i];
        if (!obj->active) continue;
        PX_Transform2 localT = obj->local_transform;
        PX_Transform2 worldT = obj->world_transform;
        PX_Transform2 finalT = combine_transform2_as_container(lvt, combine_transform2(worldT, localT));

        switch (obj->type){
            case PX_RS_OBJECT_2D_EDITOR_GRID: {
                if (!obj->ex_data) continue;
                push_2d_grid(obj->ex_data, lvt, worldT, &batch, false);
				push_2d_batch(&batch);
                break;
            }
            case PX_RS_OBJECT_2D_EDITOR_GIZMO: {
				if (!obj->ex_data) continue;

                PX_Vector2 GXep = (PX_Vector2){finalT.pos.x + 5, finalT.pos.y};
                PX_Vector2 GYep = (PX_Vector2){finalT.pos.x, finalT.pos.y + 5};
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_2d_line((PX_Color4){0x75,0x0,0x0,0xFF}, finalT.pos, GXep, 4.0f, &batch);
                else
                    push_2d_line((PX_Color4){0xFF,0x0,0x0,0xFF}, finalT.pos, GXep, 4.0f, &batch);
                
                push_2d_batch(&batch);
                if (obj->ex_data && *(bool*)obj->ex_data)
                    push_2d_line((PX_Color4){0x0,0x75,0x0,0xFF}, finalT.pos, GYep, 4.0f, &batch);
                else
                    push_2d_line((PX_Color4){0x0,0xFF,0x0,0xFF}, finalT.pos, GYep, 4.0f, &batch);
                
                push_2d_batch(&batch);
                
                // Picker
                PX_Color3 main_color = {
                    .r = 0xFF, //obj->id & 0xFF,
                    .g = (obj->id >> 8) & 0xFF,
                    .b = obj->type
                };
				break;
            }
            default: continue;
        }
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_scene_3d(PX_Scene_3D* scene) {
	if (!scene) return ERR_INVALID_ARGUMENTS;

    for (size_t i = 0; i < scene->object_count; i++) {
        PX_3D_Object* obj = &scene->objects[i];
        if (!obj->active) continue;
        PX_Transform3 localT = obj->local_transform;
        PX_Transform3 worldT = obj->world_transform;
        PX_Transform3 finalT = combine_transform3(worldT, localT);

        struct batch_3d batch = {0};
        batch.depth_override = false;
        batch.switch_fbo = false;
        batch.pure_color = false;

        switch (obj->type){
            case PX_RS_OBJECT_3D_TYPE_MESH: {
                if (!obj->ex_data) continue;
                struct batch_3d* b = (struct batch_3d*)obj->ex_data;
                if (gr_batch_3d->vertex_count + b->vertex_count > gr_batch_3d->vertex_capacity) continue; // Skip, too large
                if (gr_batch_3d->index_count + b->index_count > gr_batch_3d->index_capacity) continue; // Skip, too large

                memcpy(&batch, b, sizeof(struct batch_3d));

                batch.vertices = NULL;
                batch.indices = NULL;
                batch.transform = finalT;

                batch.vertex_offset = gr_batch_3d->vertex_count;
                memcpy(&gr_batch_3d->vertices[gr_batch_3d->vertex_count], b->vertices, sizeof(struct vertex_3d) * b->vertex_count);
                gr_batch_3d->vertex_count += b->vertex_count;

                batch.index_offset = gr_batch_3d->index_count;
                for (size_t j = 0; j < b->index_count; j++) {
                    gr_batch_3d->indices[gr_batch_3d->index_count + j] =  b->indices[j] + batch.vertex_offset;
                }
                gr_batch_3d->index_count += b->index_count;

				push_3d_batch(&batch);
                break;
            }
            default: continue;
        }
    }
    return ERR_SUCCESS;
}

t_err_codes px_rs_draw_scene_2d(PX_AnchorRect local_viewport, PX_Scene_2D* scene) {
	if (!scene) return ERR_INVALID_ARGUMENTS;
	PX_Transform2 lvt = enginef_convert_anchor_to_transform(local_viewport);

    for (size_t i = 0; i < scene->object_count; i++) {
        PX_2D_Object* obj = &scene->objects[i];
        if (!obj->active) continue;
        PX_Transform2 localT = obj->local_transform;
        PX_Transform2 worldT = obj->world_transform;
        PX_Transform2 finalT = combine_transform2_as_container(lvt, combine_transform2(worldT, localT));

        struct batch_2d batch = {0};
        batch.switch_fbo = false;
        batch.pure_color = false;

        switch (obj->type){
            case PX_RS_OBJECT_2D_TYPE_PANEL: {
				if (!obj->ex_data || obj->ex_data_type != PX_RS_OBJECT_2D_TYPE_PANEL) continue;
                push_2d_quad(lvt, finalT, *((PX_Color4*)(obj->ex_data)), &batch, obj->screen_pos_fixed);
				push_2d_batch(&batch);
                break;
            }
            default: continue;
        }
    }
    return ERR_SUCCESS;
}

void px_rs_handle_mouse_move(PX_Vector2 mpos, PX_Scale2 screen_scale) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_handle_mouse_move(mpos, screen_scale); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_handle_mouse_move(mpos, screen_scale); break;
		default: return;
	}
}

t_err_codes px_rs_change_backend(PX_GPU_Backend new_backend) {
	switch (new_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: break;
		case PX_RS_GPU_BACKEND_VULKAN: break;
		default: return ERR_RS_INVALID_BACKEND;
	}

	if (new_backend == current_backend) return ERR_SUCCESS;

	if (backend_initialized) {
		px_rs_shutdown();
	}
	
	current_backend = new_backend;
	return ERR_SUCCESS;
}

t_err_codes px_rs_create_texture(PX_Texture* texture) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_create_texture(texture);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_create_texture(texture);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_upload_texture(PX_Texture* texture, PX_TextureFormat source_format, uint32_t mip_level, const void* data) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_upload_texture(texture, source_format, mip_level, data);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_upload_texture(texture, source_format, mip_level, data);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

t_err_codes px_rs_set_sampler(PX_Texture* texture, PX_Sampler* sampler) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: return px_rs_gl_set_sampler(texture, sampler);
		case PX_RS_GPU_BACKEND_VULKAN: return px_rs_vk_set_sampler(texture, sampler);
		default: return ERR_RS_INVALID_BACKEND;
	}
}

void px_rs_destroy_texture(PX_Texture* texture) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_texture(texture); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_texture(texture); break;
		default: return;
	}
}

void px_rs_destroy_sampler(PX_Sampler* sampler) {
	switch (current_backend) {
		case PX_RS_GPU_BACKEND_OPENGL: px_rs_gl_destroy_sampler(sampler); break;
		case PX_RS_GPU_BACKEND_VULKAN: px_rs_vk_destroy_sampler(sampler); break;
		default: return;
	}
}
