/* renderer.c — OpenGL 3.3 core renderer */
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Shader sources                                                       */
/* ------------------------------------------------------------------ */

static const char *VERT_SRC =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "uniform mat4 uMVP;\n"
    "uniform mat4 uMV;\n"
    "out vec3 vNormal;\n"
    "out vec3 vFragPos;\n"
    "void main() {\n"
    "    vec4 mv_pos  = uMV * vec4(aPos, 1.0);\n"
    "    vFragPos = mv_pos.xyz;\n"
    "    vNormal  = mat3(uMV) * aNormal;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";

static const char *FRAG_SOLID_SRC =
    "#version 330 core\n"
    "in  vec3 vNormal;\n"
    "in  vec3 vFragPos;\n"
    "uniform vec3 uColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n = normalize(vNormal);\n"
    "    vec3 l0 = normalize(vec3(1.5, 2.0, 2.5));\n"
    "    vec3 l1 = normalize(vec3(-1.0, -1.5, -1.0));\n"
    "    float d0 = max(dot(n, l0), 0.0);\n"
    "    float d1 = max(dot(n, l1), 0.0);\n"
    "    vec3 ambient  = 0.18 * uColor;\n"
    "    vec3 diffuse  = (0.85*d0 + 0.22*d1) * uColor;\n"
    "    vec3 viewDir  = normalize(-vFragPos);\n"
    "    vec3 refl     = reflect(-l0, n);\n"
    "    float spec    = pow(max(dot(viewDir, refl), 0.0), 64.0);\n"
    "    vec3 specular = 0.35 * spec * vec3(0.9, 0.9, 1.0);\n"
    "    fragColor = vec4(clamp(ambient + diffuse + specular, 0.0, 1.0), 1.0);\n"
    "}\n";

static const char *FRAG_WIRE_SRC =
    "#version 330 core\n"
    "uniform vec3 uColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = vec4(uColor, 1.0);\n"
    "}\n";

/* ------------------------------------------------------------------ */
/* Shader utilities                                                     */
/* ------------------------------------------------------------------ */

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(const char *vert_src, const char *frag_src)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0; }

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int renderer_init(Renderer *r)
{
    r->mesh_color[0] = 0.68f;
    r->mesh_color[1] = 0.72f;
    r->mesh_color[2] = 0.82f;
    r->bg_color[0]   = 0.13f;
    r->bg_color[1]   = 0.13f;
    r->bg_color[2]   = 0.18f;
    renderer_reset_view(r);

    r->prog_solid = link_program(VERT_SRC, FRAG_SOLID_SRC);
    r->prog_wire  = link_program(VERT_SRC, FRAG_WIRE_SRC);
    if (!r->prog_solid || !r->prog_wire) return 0;

    r->u_mvp   = glGetUniformLocation(r->prog_solid, "uMVP");
    r->u_mv    = glGetUniformLocation(r->prog_solid, "uMV");
    r->u_color = glGetUniformLocation(r->prog_solid, "uColor");
    r->uw_mvp  = glGetUniformLocation(r->prog_wire,  "uMVP");
    r->uw_color= glGetUniformLocation(r->prog_wire,  "uColor");

    glGenVertexArrays(1, &r->vao);
    glGenBuffers(1, &r->vbo_verts);
    glGenBuffers(1, &r->vbo_normals);

    /* Configure VAO attribute layout once */
    glBindVertexArray(r->vao);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_verts);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_normals);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    return 1;
}

void renderer_load(Renderer *r, const STLMesh *mesh)
{
    r->center    = mesh->center;
    r->fit_scale = mesh->fit_scale;
    r->n_verts   = (int)mesh->n_verts;
    r->has_model = 1;

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_verts);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(mesh->n_verts * 3 * sizeof(float)),
        mesh->verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo_normals);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(mesh->n_verts * 3 * sizeof(float)),
        mesh->normals, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void renderer_draw(Renderer *r, int w, int h)
{
    glClearColor(r->bg_color[0], r->bg_color[1], r->bg_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!r->has_model) return;

    /* --- Build ModelView matrix (same transform order as Python version) ---
     * T(pan) * Rx(rot_x) * Ry(rot_y) * S(fit_scale) * T(-center)
     */
    Mat4 t_pan, rx, ry, s_mat, t_ctr, mv, mvp, proj;

    float depth = 3.0f / fmaxf(r->zoom, 1e-3f);
    mat4_translate    (t_pan,  r->pan_x,       r->pan_y,       -depth);
    mat4_rotate_x     (rx,     r->rot_x);
    mat4_rotate_y     (ry,     r->rot_y);
    mat4_scale_uniform(s_mat,  r->fit_scale);
    mat4_translate    (t_ctr, -r->center.x,   -r->center.y,   -r->center.z);

    mat4_mul(mv, t_pan, rx);
    mat4_mul(mv, mv,    ry);
    mat4_mul(mv, mv,    s_mat);
    mat4_mul(mv, mv,    t_ctr);

    float aspect = (h > 0) ? (float)w / (float)h : 1.0f;
    mat4_perspective(proj, 45.0f, aspect, 0.001f, 1000.0f);
    mat4_mul(mvp, proj, mv);

    glViewport(0, 0, w, h);
    glBindVertexArray(r->vao);

    /* --- Solid pass --- */
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glUseProgram(r->prog_solid);
    glUniformMatrix4fv(r->u_mvp, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(r->u_mv,  1, GL_FALSE, mv);
    glUniform3fv(r->u_color, 1, r->mesh_color);
    glDrawArrays(GL_TRIANGLES, 0, r->n_verts);

    /* --- Wireframe overlay --- */
    if (r->wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glLineWidth(0.8f);

        glUseProgram(r->prog_wire);
        glUniformMatrix4fv(r->uw_mvp, 1, GL_FALSE, mvp);
        float wire_col[3] = { 0.0f, 0.0f, 0.0f };
        glUniform3fv(r->uw_color, 1, wire_col);
        glDrawArrays(GL_TRIANGLES, 0, r->n_verts);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void renderer_reset_view(Renderer *r)
{
    r->rot_x = 30.0f;
    r->rot_y = -45.0f;
    r->zoom  = 1.0f;
    r->pan_x = 0.0f;
    r->pan_y = 0.0f;
}

void renderer_cleanup(Renderer *r)
{
    glDeleteBuffers(1, &r->vbo_verts);
    glDeleteBuffers(1, &r->vbo_normals);
    glDeleteVertexArrays(1, &r->vao);
    glDeleteProgram(r->prog_solid);
    glDeleteProgram(r->prog_wire);
}
