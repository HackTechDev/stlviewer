/* render.c — off-screen OpenGL rendering via OSMesa (fixed-function) */
#include "render.h"
#include "math3d.h"
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <string.h>

uint8_t *render_stl_rgba(const STLMesh *mesh, int size)
{
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 32, 0, 0, NULL);
    if (!ctx) return NULL;

    uint8_t *pixels = malloc((size_t)size * size * 4);
    if (!pixels) { OSMesaDestroyContext(ctx); return NULL; }

    if (!OSMesaMakeCurrent(ctx, pixels, GL_UNSIGNED_BYTE, size, size)) {
        free(pixels); OSMesaDestroyContext(ctx); return NULL;
    }

    /* ---- GL state ---- */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat l0_pos[]  = { 1.5f, 2.0f, 2.5f, 0.0f };
    GLfloat l0_diff[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    GLfloat l0_amb[]  = { 0.18f, 0.18f, 0.18f, 1.0f };
    GLfloat l0_spec[] = { 0.60f, 0.60f, 0.60f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, l0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  l0_diff);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  l0_amb);
    glLightfv(GL_LIGHT0, GL_SPECULAR, l0_spec);

    GLfloat l1_pos[]  = { -1.0f, -1.5f, -1.0f, 0.0f };
    GLfloat l1_diff[] = { 0.25f, 0.25f, 0.30f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, l1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  l1_diff);

    GLfloat mat_spec[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_spec);
    glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 60);

    /* ---- Viewport + projection ---- */
    glClearColor(0.13f, 0.13f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, size, size);

    Mat4 proj;
    mat4_perspective(proj, 45.0f, 1.0f, 0.001f, 1000.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);

    /* ---- ModelView (same default angle as the viewer) ---- */
    Mat4 t_eye, rx, ry, s_mat, t_ctr, mv;
    mat4_translate    (t_eye,  0.0f,          0.0f,          -3.0f);
    mat4_rotate_x     (rx,     30.0f);
    mat4_rotate_y     (ry,    -45.0f);
    mat4_scale_uniform(s_mat,  mesh->fit_scale);
    mat4_translate    (t_ctr, -mesh->center.x, -mesh->center.y, -mesh->center.z);

    mat4_mul(mv, t_eye, rx);
    mat4_mul(mv, mv,    ry);
    mat4_mul(mv, mv,    s_mat);
    mat4_mul(mv, mv,    t_ctr);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(mv);

    /* ---- Draw ---- */
    glColor3f(0.68f, 0.72f, 0.82f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh->verts);
    glNormalPointer(GL_FLOAT, 0, mesh->normals);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh->n_verts);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    glFinish();
    OSMesaDestroyContext(ctx);

    /* ---- Flip rows: OpenGL bottom-up → PNG top-down ---- */
    size_t stride = (size_t)size * 4;
    uint8_t *tmp = malloc(stride);
    if (tmp) {
        for (int i = 0; i < size / 2; i++) {
            uint8_t *top = pixels + i               * stride;
            uint8_t *bot = pixels + (size - 1 - i) * stride;
            memcpy(tmp, top, stride);
            memcpy(top, bot, stride);
            memcpy(bot, tmp, stride);
        }
        free(tmp);
    }

    return pixels;
}
