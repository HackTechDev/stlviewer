/* stl.c — STL file parser */
#include "stl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int is_binary_stl(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long file_size = ftell(f);
    fseek(f, 80, SEEK_SET);

    uint32_t count = 0;
    if (fread(&count, 4, 1, f) != 1) { fclose(f); return 0; }
    fclose(f);

    /* Binary STL: header(80) + count(4) + count × 50 bytes */
    return (file_size == 80 + 4 + (long)count * 50);
}

static void compute_normal(const float *v0, const float *v1, const float *v2,
                            float *out_n)
{
    Vec3 a = { v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2] };
    Vec3 b = { v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2] };
    Vec3 n = vec3_normalize(vec3_cross(a, b));
    out_n[0] = n.x; out_n[1] = n.y; out_n[2] = n.z;
}

static float vec3f_len(const float *v)
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

/* Fill mesh->bbox_min, bbox_max, center, fit_scale from mesh->verts. */
static void compute_bbox(STLMesh *mesh)
{
    float xmin = FLT_MAX, ymin = FLT_MAX, zmin = FLT_MAX;
    float xmax = -FLT_MAX, ymax = -FLT_MAX, zmax = -FLT_MAX;

    for (size_t i = 0; i < mesh->n_verts; i++) {
        float x = mesh->verts[i*3+0];
        float y = mesh->verts[i*3+1];
        float z = mesh->verts[i*3+2];
        if (x < xmin) xmin = x;
        if (x > xmax) xmax = x;
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
        if (z < zmin) zmin = z;
        if (z > zmax) zmax = z;
    }

    mesh->bbox_min = (Vec3){ xmin, ymin, zmin };
    mesh->bbox_max = (Vec3){ xmax, ymax, zmax };
    mesh->center   = (Vec3){
        (xmin+xmax)*0.5f, (ymin+ymax)*0.5f, (zmin+zmax)*0.5f
    };

    float extent = fmaxf(xmax-xmin, fmaxf(ymax-ymin, zmax-zmin));
    mesh->fit_scale = (extent > 1e-9f) ? 2.0f / extent : 1.0f;
}

/* ------------------------------------------------------------------ */
/* Binary parser                                                        */
/* ------------------------------------------------------------------ */

static STLMesh *parse_binary(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 80, SEEK_SET);

    uint32_t count = 0;
    if (fread(&count, 4, 1, f) != 1 || count == 0) { fclose(f); return NULL; }

    STLMesh *mesh = calloc(1, sizeof(STLMesh));
    if (!mesh) { fclose(f); return NULL; }

    mesh->n_verts = (size_t)count * 3;
    mesh->verts   = malloc(mesh->n_verts * 3 * sizeof(float));
    mesh->normals = malloc(mesh->n_verts * 3 * sizeof(float));
    if (!mesh->verts || !mesh->normals) {
        stl_mesh_free(mesh);
        fclose(f);
        return NULL;
    }

    /* Each triangle: 12B normal + 3×12B verts + 2B attr = 50B */
    float raw[12]; /* n(3) + v0(3) + v1(3) + v2(3) */
    uint16_t attr;

    for (uint32_t i = 0; i < count; i++) {
        if (fread(raw, sizeof(float), 12, f) != 12) goto fail;
        if (fread(&attr, 2, 1, f) != 1) goto fail;

        float *n  = raw + 0;
        float *v0 = raw + 3;
        float *v1 = raw + 6;
        float *v2 = raw + 9;

        /* Recompute normal if the stored one is degenerate */
        if (vec3f_len(n) < 1e-6f)
            compute_normal(v0, v1, v2, n);

        size_t base = (size_t)i * 9; /* 3 verts × 3 components */
        memcpy(mesh->verts   + base,     v0, 12);
        memcpy(mesh->verts   + base + 3, v1, 12);
        memcpy(mesh->verts   + base + 6, v2, 12);
        memcpy(mesh->normals + base,     n,  12);
        memcpy(mesh->normals + base + 3, n,  12);
        memcpy(mesh->normals + base + 6, n,  12);
    }

    fclose(f);
    compute_bbox(mesh);
    return mesh;

fail:
    fclose(f);
    stl_mesh_free(mesh);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* ASCII parser                                                         */
/* ------------------------------------------------------------------ */

#define CHUNK 1024

static STLMesh *parse_ascii(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    size_t capacity = CHUNK;
    float *verts   = malloc(capacity * 9 * sizeof(float));
    float *normals = malloc(capacity * 9 * sizeof(float));
    if (!verts || !normals) { free(verts); free(normals); fclose(f); return NULL; }

    size_t n_tris = 0;
    float cur_n[3] = {0,0,1};
    float face[3][3];
    int   v_idx = 0;
    char  line[256];

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strncasecmp(p, "facet normal", 12) == 0) {
            sscanf(p + 12, "%f %f %f", &cur_n[0], &cur_n[1], &cur_n[2]);
            v_idx = 0;

        } else if (strncasecmp(p, "vertex", 6) == 0 && v_idx < 3) {
            sscanf(p + 6, "%f %f %f",
                &face[v_idx][0], &face[v_idx][1], &face[v_idx][2]);
            v_idx++;

        } else if (strncasecmp(p, "endfacet", 8) == 0 && v_idx == 3) {
            if (vec3f_len(cur_n) < 1e-6f)
                compute_normal(face[0], face[1], face[2], cur_n);

            /* Grow buffers if needed */
            if (n_tris >= capacity) {
                capacity *= 2;
                float *tv = realloc(verts,   capacity * 9 * sizeof(float));
                float *tn = realloc(normals, capacity * 9 * sizeof(float));
                if (!tv || !tn) {
                    free(verts); free(normals); fclose(f); return NULL;
                }
                verts = tv; normals = tn;
            }

            size_t base = n_tris * 9;
            memcpy(verts   + base,     face[0], 12);
            memcpy(verts   + base + 3, face[1], 12);
            memcpy(verts   + base + 6, face[2], 12);
            memcpy(normals + base,     cur_n,   12);
            memcpy(normals + base + 3, cur_n,   12);
            memcpy(normals + base + 6, cur_n,   12);
            n_tris++;
        }
    }
    fclose(f);

    if (n_tris == 0) { free(verts); free(normals); return NULL; }

    STLMesh *mesh = calloc(1, sizeof(STLMesh));
    if (!mesh) { free(verts); free(normals); return NULL; }

    mesh->n_verts = n_tris * 3;
    mesh->verts   = verts;
    mesh->normals = normals;
    compute_bbox(mesh);
    return mesh;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

STLMesh *stl_mesh_load(const char *path)
{
    if (is_binary_stl(path))
        return parse_binary(path);
    return parse_ascii(path);
}

void stl_mesh_free(STLMesh *mesh)
{
    if (!mesh) return;
    free(mesh->verts);
    free(mesh->normals);
    free(mesh);
}
