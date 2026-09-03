#include <stdlib.h>

#include <rendering-sys.h>
#include <font.h>
#include <rendering-sys/loader.h>
#include <rendering-sys/internal.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <external/stb_image.h>

size_t px_rs_loader_load_file(PX_Scene_3D* s, const char* path) {
    const struct aiScene* scene =  aiImportFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_FindInvalidData |
        aiProcess_GenUVCoords |
        aiProcess_TransformUVCoords |
        aiProcess_ValidateDataStructure
    );

    if (!scene) return 0;
    if (scene->mNumMeshes <= 0) {
        aiReleaseImport(scene);
        return 0;
    }

    uint64_t base_idx = s->object_count; // 1-based

    for (uint32_t mesh_idx = 0; mesh_idx < scene->mNumMeshes; mesh_idx++) {
        struct aiMesh* mesh = scene->mMeshes[mesh_idx];
        struct batch_3d* b = (struct batch_3d*)malloc(sizeof(struct batch_3d));
        if (!b) continue;

        PX_3D_Object obj = {0};

        if (mesh->mNumVertices <= 0) {free(b); continue;}

        uint64_t total_indices = 0;
        for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
            struct aiFace face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++) {
                total_indices++;
            }
        }
        if (total_indices <= 0) {free(b); continue;}

        struct vertex_3d* vertices = (struct vertex_3d*)malloc(sizeof(struct vertex_3d) * mesh->mNumVertices);
        if (!vertices) {free(b); continue;}
        uint32_t* indices = (uint32_t*)malloc(sizeof(uint32_t) * total_indices);
        if (!indices) {free(vertices); free(b); continue;}

        for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
            struct vertex_3d v = {0};

            v.x = mesh->mVertices[i].x;
            v.y = mesh->mVertices[i].y;
            v.z = mesh->mVertices[i].z;

            if (mesh->mNormals) {
                v.nx = mesh->mNormals[i].x;
                v.ny = mesh->mNormals[i].y;
                v.nz = mesh->mNormals[i].z;
            }

            if (mesh->mTextureCoords[0]) {
                v.u = mesh->mTextureCoords[0][i].x;
                v.v = mesh->mTextureCoords[0][i].y;
            }

            vertices[i] = v;
        }
        uint64_t idx_ptr = 0;
        for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
            struct aiFace face = mesh->mFaces[i];

            for (uint32_t j = 0; j < face.mNumIndices; j++) {
                indices[idx_ptr++] = face.mIndices[j];
            }
        }

        b->vertex_offset = 0;
        b->vertex_count  = mesh->mNumVertices;
        b->index_offset = 0;
        b->index_count  = total_indices;
        b->type = BATCH_3D_SIMPLE;
        b->color = (PX_Color4){0xFF, 0xFF, 0xFF, 0xFF};
        b->vertices = vertices;
        b->indices = indices;

        obj.active = true;
        obj.has_children = false;
        obj.ex_data = b;
        obj.ex_data_type = PX_RS_OBJECT_3D_TYPE_MESH;
        obj.local_transform = (PX_Transform3){.rot.w=1,.scale=(PX_Scale3){1,1,1}};
        obj.world_transform = (PX_Transform3){.rot.w=1,.scale=(PX_Scale3){1,1,1}};
        obj.static_object = false;
        obj.type = PX_RS_OBJECT_3D_TYPE_MESH;

        if (mesh->mName.length > 0) {
            obj.name = (char*)malloc(mesh->mName.length + 1);
            if (!obj.name) {
                obj.name = "Unamed Mesh";
            } else {
                strncpy(obj.name, mesh->mName.data, mesh->mName.length);
                obj.name[mesh->mName.length] = '\0';
            }
        } else {
            obj.name = "Unamed Mesh";
        }

        if (s->object_count + 1 > PX_RS_MAX_OBJECTS_PER_SCENE) {
            fprintf(stderr, "Error: Too many objects in Scene! Skipping %s\n", obj.name);
            free(vertices);
            free(indices);
            free(b);
        } else {
            memcpy(&s->objects[s->object_count++], &obj, sizeof(PX_3D_Object));
        }
    }
    aiReleaseImport(scene);

    return base_idx;
}

void px_rs_loader_destroy_load(PX_Scene_3D* s, PX_3D_Object* object) {
    if (!s || !object) return;

    if (object->type == PX_RS_OBJECT_3D_TYPE_MESH && object->ex_data && object->ex_data_type == PX_RS_OBJECT_3D_TYPE_MESH) {
        struct batch_3d* b = (struct batch_3d*)object->ex_data;

        if (b->vertices) {
            free(b->vertices);
            b->vertices = NULL;
        }

        if (b->indices) {
            free(b->indices);
            b->indices = NULL;
        }

        free(b);
        object->ex_data = NULL;
    }

    if (object->name) {
        if (strcmp(object->name, "Unamed Mesh") != 0) {
            free(object->name);
        }

        object->name = NULL;
    }
    bool found = false;

    for (uint32_t i = 0; i < s->object_count; i++) {
        if (&s->objects[i] == object) {
            found = true;
        }

        if (found && i + 1 < s->object_count) {
            s->objects[i] = s->objects[i + 1];
        }
    }
    if (found && s->object_count > 0) {
        s->object_count--;
    }

    memset(object, 0, sizeof(PX_3D_Object));
}
