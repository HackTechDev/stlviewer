/* png_write.h — PNG output with FreeDesktop thumbnail metadata */
#pragma once
#include <stdint.h>

/* Write an RGBA pixel buffer (top-down, size×size) to a PNG file.
   Embeds Thumb::URI and Thumb::MTime text chunks required by the
   FreeDesktop thumbnail specification.
   Returns 1 on success, 0 on error. */
int write_png_thumbnail(const char *output_path,
                        const uint8_t *rgba, int size,
                        const char *source_path);
