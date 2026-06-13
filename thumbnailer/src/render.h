/* render.h — off-screen STL rendering via OSMesa */
#pragma once
#include "stl.h"
#include <stdint.h>

/* Render mesh to a freshly malloc'd RGBA buffer (size×size pixels, top-down).
   Returns NULL on error. Caller must free() the result. */
uint8_t *render_stl_rgba(const STLMesh *mesh, int size);
