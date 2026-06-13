#!/usr/bin/env python3
"""STL 3D File Viewer — PyQt5 + OpenGL"""

import sys
import os
import struct
import numpy as np

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QOpenGLWidget,
    QFileDialog, QAction, QToolBar, QStatusBar,
    QLabel, QVBoxLayout, QWidget, QHBoxLayout,
    QPushButton, QGroupBox, QColorDialog, QMessageBox,
    QFrame, QSizePolicy, QShortcut,
)
from PyQt5.QtCore import Qt, QPoint, QTimer
from PyQt5.QtGui import QKeySequence, QColor, QFont, QSurfaceFormat

from OpenGL.GL import *
from OpenGL.GLU import *


# ---------------------------------------------------------------------------
# STL Parsing
# ---------------------------------------------------------------------------

def _is_binary_stl(filename: str) -> bool:
    size = os.path.getsize(filename)
    with open(filename, "rb") as f:
        f.seek(80)
        data = f.read(4)
    if len(data) < 4:
        return False
    count = struct.unpack("<I", data)[0]
    return size == 80 + 4 + count * 50


def parse_stl(filename: str):
    """Return (vertices, normals) as float32 numpy arrays, one row per vertex."""
    if _is_binary_stl(filename):
        return _parse_binary(filename)
    return _parse_ascii(filename)


def _parse_binary(filename: str):
    with open(filename, "rb") as f:
        f.read(80)
        count = struct.unpack("<I", f.read(4))[0]
        raw = f.read(count * 50)

    # Each triangle: 3×float normal + 3×3×float verts + 2-byte attr
    dt = np.dtype([
        ("normal", np.float32, (3,)),
        ("v0",     np.float32, (3,)),
        ("v1",     np.float32, (3,)),
        ("v2",     np.float32, (3,)),
        ("attr",   np.uint16),
    ])
    tris = np.frombuffer(raw, dtype=dt)

    vertices = np.empty((count * 3, 3), dtype=np.float32)
    vertices[0::3] = tris["v0"]
    vertices[1::3] = tris["v1"]
    vertices[2::3] = tris["v2"]

    stored = tris["normal"]
    bad = np.linalg.norm(stored, axis=1) < 1e-6
    normals_face = np.where(bad[:, None], _compute_face_normals(tris), stored)
    normals = np.repeat(normals_face, 3, axis=0).astype(np.float32)

    return vertices, normals


def _compute_face_normals(tris):
    e1 = tris["v1"] - tris["v0"]
    e2 = tris["v2"] - tris["v0"]
    n = np.cross(e1, e2).astype(np.float64)
    norms = np.linalg.norm(n, axis=1, keepdims=True)
    norms = np.where(norms < 1e-12, 1.0, norms)
    return (n / norms).astype(np.float32)


def _parse_ascii(filename: str):
    vertices, normals = [], []
    current_n = np.array([0.0, 0.0, 1.0])
    face_verts: list = []

    with open(filename, "r", errors="ignore") as f:
        for line in f:
            tok = line.split()
            if not tok:
                continue
            kw = tok[0].lower()
            if kw == "facet" and len(tok) >= 5:
                try:
                    current_n = np.array([float(tok[2]), float(tok[3]), float(tok[4])], dtype=np.float32)
                except ValueError:
                    current_n = np.array([0.0, 0.0, 1.0], dtype=np.float32)
                face_verts = []
            elif kw == "vertex" and len(tok) >= 4:
                try:
                    face_verts.append([float(tok[1]), float(tok[2]), float(tok[3])])
                except ValueError:
                    pass
            elif kw == "endfacet" and len(face_verts) == 3:
                n = current_n
                if np.linalg.norm(n) < 1e-6:
                    e1 = np.array(face_verts[1]) - np.array(face_verts[0])
                    e2 = np.array(face_verts[2]) - np.array(face_verts[0])
                    n = np.cross(e1, e2)
                    norm = np.linalg.norm(n)
                    n = n / norm if norm > 1e-12 else np.array([0.0, 0.0, 1.0])
                vertices.extend(face_verts)
                normals.extend([n, n, n])

    return (
        np.array(vertices, dtype=np.float32),
        np.array(normals, dtype=np.float32),
    )


# ---------------------------------------------------------------------------
# OpenGL Viewport
# ---------------------------------------------------------------------------

class STLViewport(QOpenGLWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.vertices: np.ndarray | None = None
        self.normals:  np.ndarray | None = None
        self._vbo_v = None
        self._vbo_n = None

        self.rot_x = 30.0
        self.rot_y = -45.0
        self.zoom  = 1.0
        self.pan_x = 0.0
        self.pan_y = 0.0
        self._last_pos = QPoint()
        self._btn = None

        self._center = np.zeros(3, dtype=np.float32)
        self._scale  = 1.0

        self.mesh_color = (0.68, 0.72, 0.82)
        self.bg_color   = (0.13, 0.13, 0.18, 1.0)
        self.wireframe  = False

        self.setMinimumSize(500, 400)
        self.setFocusPolicy(Qt.StrongFocus)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def load(self, filename: str) -> int:
        self.vertices, self.normals = parse_stl(filename)
        self._fit_model()
        self.reset_view()
        self.makeCurrent()
        self._upload_vbos()
        self.doneCurrent()
        self.update()
        return len(self.vertices) // 3

    def reset_view(self):
        self.rot_x = 30.0
        self.rot_y = -45.0
        self.zoom  = 1.0
        self.pan_x = 0.0
        self.pan_y = 0.0
        self.update()

    def set_mesh_color(self, r, g, b):
        self.mesh_color = (r, g, b)
        self.update()

    def set_bg_color(self, r, g, b):
        self.bg_color = (r, g, b, 1.0)
        self.makeCurrent()
        glClearColor(r, g, b, 1.0)
        self.doneCurrent()
        self.update()

    def toggle_wireframe(self, state: bool):
        self.wireframe = state
        self.update()

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _fit_model(self):
        if self.vertices is None or len(self.vertices) == 0:
            return
        lo = self.vertices.min(axis=0)
        hi = self.vertices.max(axis=0)
        self._center = (lo + hi) / 2.0
        extent = float(np.max(hi - lo))
        self._scale = 2.0 / extent if extent > 1e-9 else 1.0

    def _upload_vbos(self):
        if self._vbo_v is not None:
            glDeleteBuffers(2, [self._vbo_v, self._vbo_n])
        self._vbo_v, self._vbo_n = glGenBuffers(2)

        glBindBuffer(GL_ARRAY_BUFFER, self._vbo_v)
        glBufferData(GL_ARRAY_BUFFER, self.vertices.nbytes, self.vertices, GL_STATIC_DRAW)

        glBindBuffer(GL_ARRAY_BUFFER, self._vbo_n)
        glBufferData(GL_ARRAY_BUFFER, self.normals.nbytes, self.normals, GL_STATIC_DRAW)

        glBindBuffer(GL_ARRAY_BUFFER, 0)

    # ------------------------------------------------------------------
    # OpenGL callbacks
    # ------------------------------------------------------------------

    def initializeGL(self):
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glEnable(GL_LIGHT1)
        glEnable(GL_COLOR_MATERIAL)
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
        glEnable(GL_NORMALIZE)
        glShadeModel(GL_SMOOTH)

        glLightfv(GL_LIGHT0, GL_POSITION, [1.5, 2.0, 2.5, 0.0])
        glLightfv(GL_LIGHT0, GL_DIFFUSE,  [0.85, 0.85, 0.85, 1.0])
        glLightfv(GL_LIGHT0, GL_AMBIENT,  [0.18, 0.18, 0.18, 1.0])
        glLightfv(GL_LIGHT0, GL_SPECULAR, [0.6,  0.6,  0.6,  1.0])

        glLightfv(GL_LIGHT1, GL_POSITION, [-1.0, -1.5, -1.0, 0.0])
        glLightfv(GL_LIGHT1, GL_DIFFUSE,  [0.25, 0.25, 0.30, 1.0])
        glLightfv(GL_LIGHT1, GL_SPECULAR, [0.0,  0.0,  0.0,  1.0])

        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, [0.4, 0.4, 0.4, 1.0])
        glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 60)

        glClearColor(*self.bg_color)

    def resizeGL(self, w, h):
        h = max(h, 1)
        glViewport(0, 0, w, h)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45.0, w / h, 0.001, 1000.0)
        glMatrixMode(GL_MODELVIEW)

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()

        depth = 3.0 / max(self.zoom, 1e-3)
        glTranslatef(self.pan_x, self.pan_y, -depth)
        glRotatef(self.rot_x, 1, 0, 0)
        glRotatef(self.rot_y, 0, 1, 0)

        if self.vertices is None or len(self.vertices) == 0:
            self._draw_empty_hint()
            return

        glScalef(self._scale, self._scale, self._scale)
        glTranslatef(-self._center[0], -self._center[1], -self._center[2])

        # Solid pass
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
        glColor3f(*self.mesh_color)
        self._draw_mesh()

        # Wireframe overlay
        if self.wireframe:
            glDisable(GL_LIGHTING)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
            glEnable(GL_POLYGON_OFFSET_LINE)
            glPolygonOffset(-1.0, -1.0)
            glLineWidth(0.8)
            glColor3f(0.0, 0.0, 0.0)
            self._draw_mesh()
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
            glDisable(GL_POLYGON_OFFSET_LINE)
            glEnable(GL_LIGHTING)

    def _draw_mesh(self):
        glEnableClientState(GL_VERTEX_ARRAY)
        glEnableClientState(GL_NORMAL_ARRAY)

        glBindBuffer(GL_ARRAY_BUFFER, self._vbo_v)
        glVertexPointer(3, GL_FLOAT, 0, None)

        glBindBuffer(GL_ARRAY_BUFFER, self._vbo_n)
        glNormalPointer(GL_FLOAT, 0, None)

        glDrawArrays(GL_TRIANGLES, 0, len(self.vertices))

        glDisableClientState(GL_VERTEX_ARRAY)
        glDisableClientState(GL_NORMAL_ARRAY)
        glBindBuffer(GL_ARRAY_BUFFER, 0)

    def _draw_empty_hint(self):
        """Draw a faint cube to indicate the viewport is working."""
        glDisable(GL_LIGHTING)
        glColor3f(0.35, 0.35, 0.45)
        glBegin(GL_LINES)
        corners = [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                   (-1,-1, 1),(1,-1, 1),(1,1, 1),(-1,1, 1)]
        edges = [(0,1),(1,2),(2,3),(3,0),(4,5),(5,6),(6,7),(7,4),
                 (0,4),(1,5),(2,6),(3,7)]
        for a, b in edges:
            glVertex3fv(corners[a])
            glVertex3fv(corners[b])
        glEnd()
        glEnable(GL_LIGHTING)

    # ------------------------------------------------------------------
    # Input
    # ------------------------------------------------------------------

    def mousePressEvent(self, e):
        self._last_pos = e.pos()
        self._btn = e.button()

    def mouseMoveEvent(self, e):
        dx = e.x() - self._last_pos.x()
        dy = e.y() - self._last_pos.y()
        if self._btn == Qt.LeftButton:
            self.rot_y += dx * 0.5
            self.rot_x += dy * 0.5
        elif self._btn == Qt.RightButton:
            speed = 0.003 / max(self.zoom, 1e-3)
            self.pan_x += dx * speed
            self.pan_y -= dy * speed
        self._last_pos = e.pos()
        self.update()

    def mouseReleaseEvent(self, e):
        self._btn = None

    def wheelEvent(self, e):
        factor = 1.12 if e.angleDelta().y() > 0 else 1.0 / 1.12
        self.zoom = max(0.01, min(500.0, self.zoom * factor))
        self.update()

    def keyPressEvent(self, e):
        if e.key() == Qt.Key_R:
            self.reset_view()
        elif e.key() == Qt.Key_W:
            self.wireframe = not self.wireframe
            self.update()


# ---------------------------------------------------------------------------
# Side panel
# ---------------------------------------------------------------------------

class SidePanel(QFrame):
    def __init__(self, viewport: STLViewport, parent=None):
        super().__init__(parent)
        self.vp = viewport
        self.setFrameStyle(QFrame.StyledPanel | QFrame.Sunken)
        self.setFixedWidth(190)

        root = QVBoxLayout(self)
        root.setContentsMargins(8, 10, 8, 10)
        root.setSpacing(10)

        # --- Vue ---
        g1 = QGroupBox("Vue")
        l1 = QVBoxLayout(g1)
        l1.setSpacing(4)

        btn_reset = QPushButton("Réinitialiser  [R]")
        btn_reset.clicked.connect(viewport.reset_view)
        l1.addWidget(btn_reset)

        self.btn_wire = QPushButton("Fil de fer  [W]")
        self.btn_wire.setCheckable(True)
        self.btn_wire.toggled.connect(viewport.toggle_wireframe)
        l1.addWidget(self.btn_wire)
        root.addWidget(g1)

        # --- Couleurs ---
        g2 = QGroupBox("Couleurs")
        l2 = QVBoxLayout(g2)
        l2.setSpacing(4)

        btn_mesh_col = QPushButton("Couleur du modèle")
        btn_mesh_col.clicked.connect(self._pick_mesh)
        l2.addWidget(btn_mesh_col)

        btn_bg_col = QPushButton("Couleur du fond")
        btn_bg_col.clicked.connect(self._pick_bg)
        l2.addWidget(btn_bg_col)
        root.addWidget(g2)

        # --- Infos ---
        g3 = QGroupBox("Informations")
        l3 = QVBoxLayout(g3)
        l3.setSpacing(2)

        self.lbl_tris = QLabel("Triangles : —")
        self.lbl_tris.setWordWrap(True)
        self.lbl_dim  = QLabel("Dimensions : —")
        self.lbl_dim.setWordWrap(True)
        l3.addWidget(self.lbl_tris)
        l3.addWidget(self.lbl_dim)
        root.addWidget(g3)

        # --- Aide ---
        g4 = QGroupBox("Contrôles souris")
        l4 = QVBoxLayout(g4)
        help_lbl = QLabel(
            "Rotation    — clic gauche\n"
            "Déplacer  — clic droit\n"
            "Zoom         — molette\n"
            "Reset          — R\n"
            "Fil de fer  — W"
        )
        f = QFont("Monospace")
        f.setPointSize(9)
        help_lbl.setFont(f)
        help_lbl.setStyleSheet("color: #999;")
        l4.addWidget(help_lbl)
        root.addWidget(g4)

        root.addStretch()

    def update_info(self, n_tris: int, verts: np.ndarray):
        self.lbl_tris.setText(f"Triangles : {n_tris:,}")
        lo = verts.min(axis=0)
        hi = verts.max(axis=0)
        d  = hi - lo
        self.lbl_dim.setText(
            f"X : {d[0]:.3g}\n"
            f"Y : {d[1]:.3g}\n"
            f"Z : {d[2]:.3g}"
        )

    def _pick_mesh(self):
        r, g, b = self.vp.mesh_color
        c = QColorDialog.getColor(QColor(int(r*255), int(g*255), int(b*255)), self)
        if c.isValid():
            self.vp.set_mesh_color(c.redF(), c.greenF(), c.blueF())

    def _pick_bg(self):
        r, g, b, _ = self.vp.bg_color
        c = QColorDialog.getColor(QColor(int(r*255), int(g*255), int(b*255)), self)
        if c.isValid():
            self.vp.set_bg_color(c.redF(), c.greenF(), c.blueF())


# ---------------------------------------------------------------------------
# Main window
# ---------------------------------------------------------------------------

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STL Viewer")
        self.resize(1200, 750)
        self.setMinimumSize(700, 500)

        self._vp = STLViewport()
        self._panel = SidePanel(self._vp)

        container = QWidget()
        hl = QHBoxLayout(container)
        hl.setContentsMargins(0, 0, 0, 0)
        hl.setSpacing(0)
        hl.addWidget(self._vp, 1)
        hl.addWidget(self._panel)
        self.setCentralWidget(container)

        self._build_menu()
        self._build_toolbar()
        self.statusBar().showMessage("Ouvrez un fichier STL via Fichier › Ouvrir…")

        # Accept drag-and-drop of STL files
        self.setAcceptDrops(True)

    # ------------------------------------------------------------------
    # Menu / toolbar
    # ------------------------------------------------------------------

    def _build_menu(self):
        mb = self.menuBar()

        fm = mb.addMenu("Fichier")
        a = QAction("Ouvrir…", self)
        a.setShortcut(QKeySequence.Open)
        a.triggered.connect(self.open_dialog)
        fm.addAction(a)
        fm.addSeparator()
        a2 = QAction("Quitter", self)
        a2.setShortcut(QKeySequence.Quit)
        a2.triggered.connect(self.close)
        fm.addAction(a2)

        vm = mb.addMenu("Vue")
        ra = QAction("Réinitialiser la vue", self)
        ra.setShortcut("R")
        ra.triggered.connect(self._vp.reset_view)
        vm.addAction(ra)

        self._wf_action = QAction("Fil de fer", self)
        self._wf_action.setCheckable(True)
        self._wf_action.setShortcut("W")
        self._wf_action.toggled.connect(self._sync_wireframe)
        vm.addAction(self._wf_action)

    def _build_toolbar(self):
        tb = QToolBar("Outils")
        tb.setMovable(False)
        tb.setToolButtonStyle(Qt.ToolButtonTextOnly)
        self.addToolBar(tb)

        a_open = QAction("Ouvrir STL", self)
        a_open.triggered.connect(self.open_dialog)
        tb.addAction(a_open)

        tb.addSeparator()

        a_reset = QAction("Reset vue", self)
        a_reset.triggered.connect(self._vp.reset_view)
        tb.addAction(a_reset)

    # ------------------------------------------------------------------
    # File handling
    # ------------------------------------------------------------------

    def open_dialog(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Ouvrir un fichier STL", "",
            "Fichiers STL (*.stl *.STL);;Tous les fichiers (*)"
        )
        if path:
            self._load(path)

    def _load(self, path: str):
        self.statusBar().showMessage(f"Chargement de {os.path.basename(path)} …")
        QApplication.processEvents()
        try:
            n = self._vp.load(path)
            self._panel.update_info(n, self._vp.vertices)
            self.setWindowTitle(f"STL Viewer — {os.path.basename(path)}")
            self.statusBar().showMessage(
                f"{os.path.basename(path)}  ·  {n:,} triangles"
            )
        except Exception as exc:
            QMessageBox.critical(self, "Erreur de chargement", str(exc))
            self.statusBar().showMessage("Échec du chargement.")

    # ------------------------------------------------------------------
    # Wireframe sync between panel button and menu action
    # ------------------------------------------------------------------

    def _sync_wireframe(self, state: bool):
        self._vp.toggle_wireframe(state)
        self._panel.btn_wire.setChecked(state)

    # ------------------------------------------------------------------
    # Drag & drop
    # ------------------------------------------------------------------

    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            urls = e.mimeData().urls()
            if urls and urls[0].toLocalFile().lower().endswith(".stl"):
                e.acceptProposedAction()

    def dropEvent(self, e):
        path = e.mimeData().urls()[0].toLocalFile()
        self._load(path)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

    fmt = QSurfaceFormat()
    fmt.setDepthBufferSize(24)
    fmt.setSamples(8)
    fmt.setVersion(2, 1)
    QSurfaceFormat.setDefaultFormat(fmt)

    app = QApplication(sys.argv)
    app.setApplicationName("STL Viewer")

    win = MainWindow()
    win.show()

    # Optional: open file passed as CLI argument
    if len(sys.argv) > 1:
        path = sys.argv[1]
        if os.path.isfile(path):
            QTimer.singleShot(100, lambda: win._load(path))
        else:
            print(f"Fichier introuvable : {path}", file=sys.stderr)

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
