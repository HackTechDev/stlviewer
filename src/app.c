/* app.c — GTK3 application window */
#include "app.h"
#include "stl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* File loading                                                         */
/* ------------------------------------------------------------------ */

static void load_file(AppData *d, const char *path)
{
    GtkStatusbar *sb = GTK_STATUSBAR(
        gtk_widget_get_ancestor(d->gl_area, GTK_TYPE_STATUSBAR));
    (void)sb; /* statusbar set below via window title + labels */

    STLMesh *mesh = stl_mesh_load(path);
    if (!mesh) {
        GtkWidget *dlg = gtk_message_dialog_new(
            GTK_WINDOW(d->window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            "Impossible de charger le fichier :\n%s", path);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return;
    }

    /* Upload to GPU (context must be current — it is if we're in a GL callback,
       otherwise make it current explicitly). */
    gtk_gl_area_make_current(GTK_GL_AREA(d->gl_area));
    if (gtk_gl_area_get_error(GTK_GL_AREA(d->gl_area))) {
        stl_mesh_free(mesh);
        return;
    }

    renderer_load(&d->renderer, mesh);

    /* Update info labels */
    char buf[128];
    snprintf(buf, sizeof(buf), "Triangles : %zu",
             mesh->n_verts / 3);
    gtk_label_set_text(GTK_LABEL(d->lbl_tris), buf);

    Vec3 lo = mesh->bbox_min, hi = mesh->bbox_max;
    snprintf(buf, sizeof(buf), "X : %.3g\nY : %.3g\nZ : %.3g",
             hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
    gtk_label_set_text(GTK_LABEL(d->lbl_dims), buf);

    stl_mesh_free(mesh);

    /* Update window title */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char title[256];
    snprintf(title, sizeof(title), "STL Viewer — %s", base);
    gtk_window_set_title(GTK_WINDOW(d->window), title);

    gtk_widget_queue_draw(d->gl_area);
}

/* ------------------------------------------------------------------ */
/* GtkGLArea callbacks                                                  */
/* ------------------------------------------------------------------ */

static void on_gl_realize(GtkGLArea *area, gpointer user_data)
{
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) return;

    AppData *d = user_data;
    if (!renderer_init(&d->renderer)) {
        gtk_gl_area_set_error(area,
            g_error_new_literal(g_quark_from_static_string("stlviewer"), 1,
                                "Renderer init failed"));
    }
}

static void on_gl_unrealize(GtkGLArea *area, gpointer user_data)
{
    gtk_gl_area_make_current(area);
    if (!gtk_gl_area_get_error(area))
        renderer_cleanup(&((AppData *)user_data)->renderer);
}

static gboolean on_gl_render(GtkGLArea *area, GdkGLContext *ctx, gpointer user_data)
{
    (void)ctx;
    AppData *d = user_data;
    int w = gtk_widget_get_allocated_width(GTK_WIDGET(area));
    int h = gtk_widget_get_allocated_height(GTK_WIDGET(area));
    renderer_draw(&d->renderer, w, h);
    return TRUE;
}

static void on_gl_resize(GtkGLArea *area, gint w, gint h, gpointer user_data)
{
    (void)area; (void)w; (void)h; (void)user_data;
    /* Nothing needed: renderer_draw() calls glViewport each frame. */
}

/* ------------------------------------------------------------------ */
/* Mouse / scroll callbacks                                             */
/* ------------------------------------------------------------------ */

static gboolean on_button_press(GtkWidget *w, GdkEventButton *e, gpointer ud)
{
    (void)w;
    AppData *d = ud;
    if      (e->button == 1) d->btn_down = 1;
    else if (e->button == 3) d->btn_down = 3;
    d->last_x = e->x;
    d->last_y = e->y;
    return TRUE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *e, gpointer ud)
{
    (void)w; (void)e;
    ((AppData *)ud)->btn_down = 0;
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer ud)
{
    AppData *d = ud;
    if (!d->btn_down) return TRUE;

    float dx = (float)(e->x - d->last_x);
    float dy = (float)(e->y - d->last_y);
    d->last_x = e->x;
    d->last_y = e->y;

    Renderer *r = &d->renderer;
    if (d->btn_down == 1) {
        r->rot_y += dx * 0.5f;
        r->rot_x += dy * 0.5f;
    } else if (d->btn_down == 3) {
        float speed = 0.003f / fmaxf(r->zoom, 1e-3f);
        r->pan_x += dx * speed;
        r->pan_y -= dy * speed;
    }

    gtk_widget_queue_draw(w);
    return TRUE;
}

static gboolean on_scroll(GtkWidget *w, GdkEventScroll *e, gpointer ud)
{
    Renderer *r = &((AppData *)ud)->renderer;
    double dy = 0.0;

    if (e->direction == GDK_SCROLL_UP)        dy = -1.0;
    else if (e->direction == GDK_SCROLL_DOWN)  dy =  1.0;
    else if (e->direction == GDK_SCROLL_SMOOTH) dy = e->delta_y;

    float factor = (dy < 0) ? 1.12f : (1.0f / 1.12f);
    r->zoom = fmaxf(0.01f, fminf(500.0f, r->zoom * factor));
    gtk_widget_queue_draw(w);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Button / menu callbacks                                              */
/* ------------------------------------------------------------------ */

static void on_open(GtkWidget *w, gpointer ud)
{
    (void)w;
    AppData *d = ud;
    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Ouvrir un fichier STL", GTK_WINDOW(d->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Annuler", GTK_RESPONSE_CANCEL,
        "_Ouvrir",  GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *ff = gtk_file_filter_new();
    gtk_file_filter_set_name(ff, "Fichiers STL");
    gtk_file_filter_add_pattern(ff, "*.stl");
    gtk_file_filter_add_pattern(ff, "*.STL");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ff);

    GtkFileFilter *fa = gtk_file_filter_new();
    gtk_file_filter_set_name(fa, "Tous les fichiers");
    gtk_file_filter_add_pattern(fa, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), fa);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        load_file(d, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}

static void on_reset_view(GtkWidget *w, gpointer ud)
{
    (void)w;
    AppData *d = ud;
    renderer_reset_view(&d->renderer);
    gtk_widget_queue_draw(d->gl_area);
}

static void on_toggle_wireframe(GtkToggleButton *btn, gpointer ud)
{
    AppData *d = ud;
    d->renderer.wireframe = gtk_toggle_button_get_active(btn) ? 1 : 0;
    gtk_widget_queue_draw(d->gl_area);
}

static void on_mesh_color_set(GtkColorButton *btn, gpointer ud)
{
    AppData *d = ud;
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &c);
    d->renderer.mesh_color[0] = (float)c.red;
    d->renderer.mesh_color[1] = (float)c.green;
    d->renderer.mesh_color[2] = (float)c.blue;
    gtk_widget_queue_draw(d->gl_area);
}

static void on_bg_color_set(GtkColorButton *btn, gpointer ud)
{
    AppData *d = ud;
    GdkRGBA c;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &c);
    d->renderer.bg_color[0] = (float)c.red;
    d->renderer.bg_color[1] = (float)c.green;
    d->renderer.bg_color[2] = (float)c.blue;
    gtk_widget_queue_draw(d->gl_area);
}

/* ------------------------------------------------------------------ */
/* Drag-and-drop                                                        */
/* ------------------------------------------------------------------ */

static void on_drag_data_received(GtkWidget *w, GdkDragContext *ctx,
    gint x, gint y, GtkSelectionData *sel,
    guint info, guint time, gpointer ud)
{
    (void)w; (void)x; (void)y; (void)info;
    gchar **uris = gtk_selection_data_get_uris(sel);
    if (uris && uris[0]) {
        gchar *path = g_filename_from_uri(uris[0], NULL, NULL);
        if (path) {
            load_file(ud, path);
            g_free(path);
        }
        g_strfreev(uris);
    }
    gtk_drag_finish(ctx, TRUE, FALSE, time);
}

/* ------------------------------------------------------------------ */
/* Window construction                                                  */
/* ------------------------------------------------------------------ */

static GtkWidget *make_label_group(GtkWidget **lbl_tris_out,
                                   GtkWidget **lbl_dims_out)
{
    GtkWidget *frame = gtk_frame_new("Informations");
    GtkWidget *box   = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);
    gtk_container_add(GTK_CONTAINER(frame), box);

    *lbl_tris_out = gtk_label_new("Triangles : —");
    *lbl_dims_out = gtk_label_new("X : —\nY : —\nZ : —");
    gtk_label_set_xalign(GTK_LABEL(*lbl_tris_out), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(*lbl_dims_out), 0.0f);
    gtk_box_pack_start(GTK_BOX(box), *lbl_tris_out, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), *lbl_dims_out, FALSE, FALSE, 0);
    return frame;
}

static GtkWidget *make_side_panel(AppData *d)
{
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(panel, 190, -1);
    gtk_container_set_border_width(GTK_CONTAINER(panel), 8);

    /* --- Vue --- */
    GtkWidget *fr_vue = gtk_frame_new("Vue");
    GtkWidget *bx_vue = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(bx_vue), 6);
    gtk_container_add(GTK_CONTAINER(fr_vue), bx_vue);

    GtkWidget *btn_open = gtk_button_new_with_label("Ouvrir…  [Ctrl+O]");
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open), d);
    gtk_box_pack_start(GTK_BOX(bx_vue), btn_open, FALSE, FALSE, 0);

    GtkWidget *btn_reset = gtk_button_new_with_label("Réinitialiser  [R]");
    g_signal_connect(btn_reset, "clicked", G_CALLBACK(on_reset_view), d);
    gtk_box_pack_start(GTK_BOX(bx_vue), btn_reset, FALSE, FALSE, 0);

    d->btn_wire = gtk_toggle_button_new_with_label("Fil de fer  [W]");
    g_signal_connect(d->btn_wire, "toggled", G_CALLBACK(on_toggle_wireframe), d);
    gtk_box_pack_start(GTK_BOX(bx_vue), d->btn_wire, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), fr_vue, FALSE, FALSE, 0);

    /* --- Couleurs --- */
    GtkWidget *fr_col = gtk_frame_new("Couleurs");
    GtkWidget *bx_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(bx_col), 6);
    gtk_container_add(GTK_CONTAINER(fr_col), bx_col);

    GtkWidget *row0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(row0),
        gtk_label_new("Modèle :"), FALSE, FALSE, 0);
    GdkRGBA init_mesh = { 0.68, 0.72, 0.82, 1.0 };
    d->btn_mesh_col = gtk_color_button_new_with_rgba(&init_mesh);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(d->btn_mesh_col), FALSE);
    g_signal_connect(d->btn_mesh_col, "color-set",
                     G_CALLBACK(on_mesh_color_set), d);
    gtk_box_pack_end(GTK_BOX(row0), d->btn_mesh_col, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bx_col), row0, FALSE, FALSE, 0);

    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(row1),
        gtk_label_new("Fond :"), FALSE, FALSE, 0);
    GdkRGBA init_bg = { 0.13, 0.13, 0.18, 1.0 };
    d->btn_bg_col = gtk_color_button_new_with_rgba(&init_bg);
    gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(d->btn_bg_col), FALSE);
    g_signal_connect(d->btn_bg_col, "color-set",
                     G_CALLBACK(on_bg_color_set), d);
    gtk_box_pack_end(GTK_BOX(row1), d->btn_bg_col, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bx_col), row1, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(panel), fr_col, FALSE, FALSE, 0);

    /* --- Infos --- */
    gtk_box_pack_start(GTK_BOX(panel),
        make_label_group(&d->lbl_tris, &d->lbl_dims), FALSE, FALSE, 0);

    /* --- Aide --- */
    GtkWidget *fr_help = gtk_frame_new("Contrôles souris");
    GtkWidget *lbl_help = gtk_label_new(
        "Rotation    — clic gauche\n"
        "Déplacer  — clic droit\n"
        "Zoom         — molette\n"
        "Reset          — R\n"
        "Fil de fer  — W");
    gtk_label_set_xalign(GTK_LABEL(lbl_help), 0.0f);
    gtk_widget_set_margin_start(lbl_help, 6);
    gtk_widget_set_margin_end(lbl_help, 6);
    gtk_widget_set_margin_top(lbl_help, 4);
    gtk_widget_set_margin_bottom(lbl_help, 4);
    gtk_container_add(GTK_CONTAINER(fr_help), lbl_help);
    gtk_box_pack_start(GTK_BOX(panel), fr_help, FALSE, FALSE, 0);

    return panel;
}

static void create_window(AppData *d)
{
    d->window = gtk_application_window_new(d->gapp);
    gtk_window_set_title(GTK_WINDOW(d->window), "STL Viewer");
    gtk_window_set_default_size(GTK_WINDOW(d->window), 1200, 750);

    GtkAccelGroup *ag = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(d->window), ag);

    /* Root layout: vbox holds menubar / content / statusbar */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(d->window), vbox);

    /* --- Menu bar (built first, packed first) --- */
    GtkWidget *menubar = gtk_menu_bar_new();

    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_Fichier");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);

    GtkWidget *mi_open = gtk_menu_item_new_with_mnemonic("_Ouvrir…");
    gtk_widget_add_accelerator(mi_open, "activate", ag,
        GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(mi_open, "activate", G_CALLBACK(on_open), d);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), mi_open);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        gtk_separator_menu_item_new());

    GtkWidget *mi_quit = gtk_menu_item_new_with_mnemonic("_Quitter");
    gtk_widget_add_accelerator(mi_quit, "activate", ag,
        GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect_swapped(mi_quit, "activate",
        G_CALLBACK(gtk_widget_destroy), d->window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), mi_quit);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    GtkWidget *view_item = gtk_menu_item_new_with_mnemonic("_Vue");
    GtkWidget *view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);

    GtkWidget *mi_reset = gtk_menu_item_new_with_label("Réinitialiser la vue");
    gtk_widget_add_accelerator(mi_reset, "activate", ag,
        GDK_KEY_r, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(mi_reset, "activate", G_CALLBACK(on_reset_view), d);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), mi_reset);

    GtkWidget *mi_wire = gtk_check_menu_item_new_with_label("Fil de fer");
    gtk_widget_add_accelerator(mi_wire, "activate", ag,
        GDK_KEY_w, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(mi_wire, "toggled", G_CALLBACK(on_toggle_wireframe), d);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), mi_wire);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    /* --- Main content area: GL view + separator + side panel --- */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    d->gl_area = gtk_gl_area_new();
    gtk_gl_area_set_required_version(GTK_GL_AREA(d->gl_area), 3, 3);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(d->gl_area), TRUE);
    gtk_widget_set_hexpand(d->gl_area, TRUE);
    gtk_widget_set_vexpand(d->gl_area, TRUE);

    gtk_widget_add_events(d->gl_area,
        GDK_BUTTON_PRESS_MASK   |
        GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK |
        GDK_SCROLL_MASK         |
        GDK_SMOOTH_SCROLL_MASK);

    g_signal_connect(d->gl_area, "realize",   G_CALLBACK(on_gl_realize),   d);
    g_signal_connect(d->gl_area, "unrealize", G_CALLBACK(on_gl_unrealize), d);
    g_signal_connect(d->gl_area, "render",    G_CALLBACK(on_gl_render),    d);
    g_signal_connect(d->gl_area, "resize",    G_CALLBACK(on_gl_resize),    d);
    g_signal_connect(d->gl_area, "button-press-event",   G_CALLBACK(on_button_press),   d);
    g_signal_connect(d->gl_area, "button-release-event", G_CALLBACK(on_button_release), d);
    g_signal_connect(d->gl_area, "motion-notify-event",  G_CALLBACK(on_motion),         d);
    g_signal_connect(d->gl_area, "scroll-event",         G_CALLBACK(on_scroll),         d);

    gtk_box_pack_start(GTK_BOX(hbox), d->gl_area, TRUE, TRUE, 0);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), make_side_panel(d), FALSE, FALSE, 0);

    /* --- Status bar --- */
    GtkWidget *sb = gtk_statusbar_new();
    gtk_box_pack_end(GTK_BOX(vbox), sb, FALSE, FALSE, 0);
    gtk_statusbar_push(GTK_STATUSBAR(sb), 0,
        "Ouvrez un fichier STL via Fichier › Ouvrir…");

    /* --- Drag-and-drop --- */
    static GtkTargetEntry dnd_targets[] = {
        { (gchar*)"text/uri-list", 0, 0 }
    };
    gtk_drag_dest_set(d->window, GTK_DEST_DEFAULT_ALL,
        dnd_targets, 1, GDK_ACTION_COPY);
    g_signal_connect(d->window, "drag-data-received",
        G_CALLBACK(on_drag_data_received), d);

    gtk_widget_show_all(d->window);
}

/* ------------------------------------------------------------------ */
/* GApplication callbacks                                               */
/* ------------------------------------------------------------------ */

static void on_activate(GtkApplication *gapp, gpointer ud)
{
    AppData *d = ud;
    d->gapp = gapp;
    create_window(d);
}

static void on_open_files(GtkApplication *gapp, GFile **files,
                          gint n_files, const gchar *hint, gpointer ud)
{
    (void)hint;
    AppData *d = ud;
    d->gapp = gapp;
    create_window(d);

    if (n_files > 0) {
        char *path = g_file_get_path(files[0]);
        if (path) { load_file(d, path); g_free(path); }
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                          */
/* ------------------------------------------------------------------ */

int app_run(int argc, char **argv)
{
    AppData *d = calloc(1, sizeof(AppData));
    if (!d) return 1;

    GtkApplication *app = gtk_application_new(
        "org.stlviewer", G_APPLICATION_HANDLES_OPEN);
    d->gapp = app;

    g_signal_connect(app, "activate",  G_CALLBACK(on_activate),   d);
    g_signal_connect(app, "open",      G_CALLBACK(on_open_files),  d);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    free(d);
    return status;
}
