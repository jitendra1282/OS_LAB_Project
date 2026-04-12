#include <zlib.h>
#include <stdio.h>

int compress_file(const char *src, const char *dest) {
    FILE *f_in = fopen(src, "rb");
    gzFile f_out = gzopen(dest, "wb");
    
    if (!f_in || !f_out) return -1;

    char buffer[1024];
    int bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f_in)) > 0) {
        gzwrite(f_out, buffer, bytes);
    }

    fclose(f_in);
    gzclose(f_out);
    return 0;
}

int decompress_file(const char *src, const char *dest) {
    gzFile f_in = gzopen(src, "rb");
    FILE *f_out = fopen(dest, "wb");
    
    if (!f_in || !f_out) return -1;

    char buffer[1024];
    int bytes;
    while ((bytes = gzread(f_in, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes, f_out);
    }

    gzclose(f_in);
    fclose(f_out);
    return 0;
}
