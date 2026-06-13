/* main.c — STL thumbnailer entry point
 *
 * Usage: stl-thumbnailer <input.stl> <output.png> [size]
 *
 * Registered in /usr/share/thumbnailers/stl.thumbnailer as:
 *   Exec=stl-thumbnailer %i %o %s
 */
#include "stl.h"
#include "render.h"
#include "png_write.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
            "Usage: stl-thumbnailer <input.stl> <output.png> [size]\n");
        return 1;
    }

    const char *input  = argv[1];
    const char *output = argv[2];
    int size = (argc >= 4) ? atoi(argv[3]) : 256;
    if (size < 32 || size > 1024) size = 256;

    STLMesh *mesh = stl_mesh_load(input);
    if (!mesh) {
        fprintf(stderr, "stl-thumbnailer: cannot load '%s'\n", input);
        return 1;
    }

    uint8_t *pixels = render_stl_rgba(mesh, size);
    stl_mesh_free(mesh);

    if (!pixels) {
        fprintf(stderr, "stl-thumbnailer: rendering failed\n");
        return 1;
    }

    int ok = write_png_thumbnail(output, pixels, size, input);
    free(pixels);

    if (!ok) {
        fprintf(stderr, "stl-thumbnailer: cannot write '%s'\n", output);
        return 1;
    }

    return 0;
}
