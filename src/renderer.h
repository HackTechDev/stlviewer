/* renderer.h — OpenGL 3.3 core renderer */
#pragma once
#include <epoxy/gl.h>
#include "math3d.h"
#include "stl.h"

typedef struct {
    /* OpenGL objects */
    GLuint vao;
    GLuint vbo_verts;
    GLuint vbo_normals;
    GLuint prog_solid;   /* Phong shading */
    GLuint prog_wire;    /* flat colour for wireframe */

    /* Uniform locations (solid prog) */
    GLint u_mvp;
    GLint u_mv;
    GLint u_color;

    /* Uniform locations (wire prog) */
    GLint uw_mvp;
    GLint uw_color;

    /* View state */
    float rot_x, rot_y;
    float zoom;
    float pan_x, pan_y;

    /* Model info */
    Vec3  center;
    float fit_scale;
    int   n_verts;
    int   has_model;

    /* Appearance */
    float mesh_color[3];
    float bg_color[3];
    int   wireframe;
} Renderer;

/* Call once, inside the GtkGLArea "realize" callback. */
int  renderer_init(Renderer *r);

/* Upload new mesh data (VBOs). */
void renderer_load(Renderer *r, const STLMesh *mesh);

/* Draw a single frame. */
void renderer_draw(Renderer *r, int width, int height);

/* Reset camera to default. */
void renderer_reset_view(Renderer *r);

/* Free all GL objects. */
void renderer_cleanup(Renderer *r);
