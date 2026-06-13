/* png_write.c — PNG output with FreeDesktop thumbnail metadata */
#include "png_write.h"
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

/* Build a percent-encoded file:// URI from an absolute path. */
static char *make_file_uri(const char *path)
{
    char *resolved = realpath(path, NULL);
    if (!resolved) return NULL;

    /* Worst case: every byte becomes %XX (3 chars) */
    char *uri = malloc(7 + strlen(resolved) * 3 + 1);
    if (!uri) { free(resolved); return NULL; }

    int j = sprintf(uri, "file://");
    for (const char *p = resolved; *p; p++) {
        unsigned char c = (unsigned char)*p;
        /* RFC-3986 unreserved + '/' are pass-through */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            uri[j++] = (char)c;
        } else {
            j += sprintf(uri + j, "%%%02X", c);
        }
    }
    uri[j] = '\0';
    free(resolved);
    return uri;
}

int write_png_thumbnail(const char *output_path,
                        const uint8_t *rgba, int size,
                        const char *source_path)
{
    /* --- Gather source file metadata --- */
    char mtime_str[32] = "0";
    struct stat st;
    if (stat(source_path, &st) == 0)
        snprintf(mtime_str, sizeof(mtime_str), "%ld", (long)st.st_mtime);

    char *uri = make_file_uri(source_path);

    /* --- Open output file --- */
    FILE *f = fopen(output_path, "wb");
    if (!f) { free(uri); return 0; }

    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(f); free(uri); return 0; }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(f); free(uri); return 0;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(f); free(uri); return 0;
    }

    png_init_io(png, f);

    /* RGBA 8-bit — alpha channel preserves the rendered background */
    png_set_IHDR(png, info,
        (png_uint_32)size, (png_uint_32)size, 8,
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    /* FreeDesktop thumbnail specification metadata
       https://specifications.freedesktop.org/thumbnail-spec/latest/ */
    png_text texts[3];
    memset(texts, 0, sizeof(texts));
    int n_texts = 0;

    texts[n_texts].compression = PNG_TEXT_COMPRESSION_NONE;
    texts[n_texts].key  = (png_charp)"Thumb::URI";
    texts[n_texts].text = uri ? uri : (char *)"";
    n_texts++;

    texts[n_texts].compression = PNG_TEXT_COMPRESSION_NONE;
    texts[n_texts].key  = (png_charp)"Thumb::MTime";
    texts[n_texts].text = mtime_str;
    n_texts++;

    texts[n_texts].compression = PNG_TEXT_COMPRESSION_NONE;
    texts[n_texts].key  = (png_charp)"Thumb::Mimetype";
    texts[n_texts].text = (png_charp)"model/stl";
    n_texts++;

    png_set_text(png, info, texts, n_texts);
    png_write_info(png, info);

    for (int y = 0; y < size; y++)
        png_write_row(png, (png_bytep)(rgba + (size_t)y * size * 4));

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    free(uri);
    return 1;
}
