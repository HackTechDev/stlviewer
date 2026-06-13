/* app.h — GTK3 application window */
#pragma once
#include <gtk/gtk.h>
#include "renderer.h"

typedef struct {
    GtkApplication *gapp;
    GtkWidget *window;
    GtkWidget *gl_area;
    GtkWidget *lbl_tris;
    GtkWidget *lbl_dims;
    GtkWidget *btn_wire;
    GtkWidget *btn_mesh_col;
    GtkWidget *btn_bg_col;

    Renderer renderer;

    /* Mouse state */
    int    btn_down;   /* 0=none 1=left 3=right */
    double last_x;
    double last_y;
} AppData;

int app_run(int argc, char **argv);
