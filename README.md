# STL Viewer

Visionneuse de fichiers 3D au format STL disponible en deux versions : Python/PyQt5 et C/GTK3.
Inclut également un générateur de vignettes pour les explorateurs de fichiers KDE et GNOME.

## Fonctionnalités du viewer

- Chargement des fichiers STL **ASCII** et **binaire** (détection automatique)
- Rendu 3D avec éclairage **Phong** (deux sources lumineuses)
- Performances optimisées via **VBOs** (Vertex Buffer Objects)
- Affichage **fil de fer** en superposition
- Couleurs du modèle et du fond personnalisables
- Panneau d'informations : nombre de triangles, dimensions X/Y/Z
- **Glisser-déposer** d'un fichier `.stl` sur la fenêtre
- Passage d'un fichier en argument de ligne de commande

## Contrôles

| Action | Commande |
|--------|----------|
| Ouvrir un fichier | `Ctrl+O` ou menu **Fichier › Ouvrir…** |
| Rotation | Clic gauche + glisser |
| Déplacer | Clic droit + glisser |
| Zoom | Molette de la souris |
| Réinitialiser la vue | Touche `R` |
| Fil de fer | Touche `W` |
| Glisser-déposer | Déposer un fichier `.stl` sur la fenêtre |

---

## Version Python

![Python](https://img.shields.io/badge/Python-3.8%2B-blue)
![PyQt5](https://img.shields.io/badge/PyQt5-5.15%2B-green)
![OpenGL](https://img.shields.io/badge/OpenGL-2.1-orange)

Utilise PyQt5 + OpenGL (pipeline fixe 2.1) + numpy.

### Prérequis

```bash
pip install -r requirements.txt
```

| Dépendance | Version minimale |
|------------|-----------------|
| PyQt5 | 5.15 |
| PyOpenGL | 3.1 |
| PyOpenGL_accelerate | 3.1 |
| numpy | 1.21 |

### Lancer

```bash
python3 main.py
python3 main.py chemin/vers/objet.stl
```

---

## Version C

![C](https://img.shields.io/badge/C-C11-blue)
![GTK3](https://img.shields.io/badge/GTK-3.24%2B-green)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3_core-orange)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-red)

Utilise GTK3 + GtkGLArea + OpenGL 3.3 core (shaders GLSL) + libepoxy.

### Prérequis (Ubuntu/Debian)

```bash
sudo apt install build-essential cmake libgtk-3-dev libepoxy-dev
```

### Compiler

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Lancer

```bash
./build/stlviewer
./build/stlviewer chemin/vers/objet.stl
```

---

## Générateur de vignettes (KDE / GNOME)

![OSMesa](https://img.shields.io/badge/OSMesa-off--screen-blue)
![FreeDesktop](https://img.shields.io/badge/FreeDesktop-thumbnail_spec-green)

`stl-thumbnailer` génère des vignettes PNG conformes à la
[spécification FreeDesktop](https://specifications.freedesktop.org/thumbnail-spec/latest/)
pour les explorateurs **Dolphin (KDE)** et **Nautilus (GNOME)**.

Il utilise OSMesa (rendu OpenGL sans affichage) et intègre les métadonnées
`Thumb::URI` et `Thumb::MTime` requises dans le fichier PNG.

### Prérequis supplémentaires

```bash
sudo apt install libosmesa6-dev
```

### Compiler et installer

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build   # installe stl-thumbnailer + stl.thumbnailer
```

### Utilisation en ligne de commande

```bash
stl-thumbnailer fichier.stl vignette.png [taille]
# Exemple : vignette 256×256
stl-thumbnailer objet.stl /tmp/thumb.png 256
```

### Activer dans l'explorateur de fichiers

Après `cmake --install`, forcer la mise à jour de la base MIME :

```bash
update-mime-database /usr/share/mime
```

Redémarrer l'explorateur de fichiers. Les vignettes apparaîtront automatiquement
pour tous les fichiers `.stl`.

---

## Structure du projet

```
STLVIEWER/
├── main.py                      # Version Python (PyQt5 + OpenGL 2.1)
├── requirements.txt             # Dépendances Python
├── CMakeLists.txt               # Build C (viewer + thumbnailer)
├── src/
│   ├── main.c                   # Point d'entrée du viewer
│   ├── math3d.h                 # Bibliothèque mathématique (mat4, vec3)
│   ├── stl.h / stl.c            # Parser STL ASCII + binaire (partagé)
│   ├── renderer.h / renderer.c  # Rendu OpenGL 3.3 (shaders Phong)
│   └── app.h / app.c            # Fenêtre GTK3 + GtkGLArea
└── thumbnailer/
    ├── CMakeLists.txt           # Build du thumbnailer
    ├── stl.thumbnailer          # Enregistrement GNOME/KDE
    └── src/
        ├── main.c               # Point d'entrée du thumbnailer
        ├── render.h / render.c  # Rendu off-screen OSMesa
        └── png_write.h / png_write.c  # Export PNG + métadonnées FreeDesktop
```
