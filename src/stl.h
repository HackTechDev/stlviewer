/* stl.h — STL file parser (ASCII + binary) */
#pragma once
#include "math3d.h"
#include <stddef.h>

typedef struct {
    float  *verts;      /* n_verts × 3 floats (XYZ interleaved) */
    float  *normals;    /* n_verts × 3 floats (XYZ interleaved) */
    size_t  n_verts;    /* total vertices = n_triangles × 3     */
    Vec3    bbox_min;
    Vec3    bbox_max;
    Vec3    center;
    float   fit_scale;  /* scale factor so the model fits in [-1,1] */
} STLMesh;

/* Returns a heap-allocated STLMesh or NULL on error.
   The caller must free it with stl_mesh_free(). */
STLMesh *stl_mesh_load(const char *path);
void     stl_mesh_free(STLMesh *mesh);
