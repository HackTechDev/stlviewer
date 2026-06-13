# STL Viewer

Visionneuse de fichiers 3D au format STL, écrite en Python 3 avec PyQt5 et OpenGL.

![Python](https://img.shields.io/badge/Python-3.8%2B-blue)
![PyQt5](https://img.shields.io/badge/PyQt5-5.15%2B-green)
![OpenGL](https://img.shields.io/badge/OpenGL-2.1-orange)

## Fonctionnalités

- Chargement des fichiers STL **ASCII** et **binaire** (détection automatique)
- Rendu 3D avec éclairage Phong (deux sources lumineuses)
- Performances optimisées via **VBOs** (Vertex Buffer Objects)
- Antialiasing 8×
- Affichage **fil de fer** en superposition
- Couleurs du modèle et du fond personnalisables
- Panneau d'informations : nombre de triangles, dimensions X/Y/Z
- **Glisser-déposer** d'un fichier `.stl` sur la fenêtre
- Passage d'un fichier en argument de ligne de commande

## Prérequis

```bash
pip install -r requirements.txt
```

| Dépendance | Version minimale |
|------------|-----------------|
| PyQt5 | 5.15 |
| PyOpenGL | 3.1 |
| PyOpenGL_accelerate | 3.1 |
| numpy | 1.21 |

## Utilisation

```bash
# Lancer l'application
python3 main.py

# Ouvrir directement un fichier STL
python3 main.py chemin/vers/objet.stl
```

## Contrôles

| Action | Commande |
|--------|----------|
| Ouvrir un fichier | `Ctrl+O` ou menu **Fichier › Ouvrir…** |
| Rotation | Clic gauche + glisser |
| Déplacer | Clic droit + glisser |
| Zoom | Molette de la souris |
| Réinitialiser la vue | Touche `R` |
| Activer/désactiver le fil de fer | Touche `W` |
| Glisser-déposer | Déposer un fichier `.stl` sur la fenêtre |

## Structure

```
STLVIEWER/
├── main.py          # Application complète (parser STL + rendu OpenGL + interface Qt)
└── requirements.txt # Dépendances Python
```
