/*
 * compress.c — File Compression/Decompression Module
 *
 * Implements chunk-based file compression and decompression using zlib.
 * This is the "wow factor" feature demonstrating advanced I/O efficiency.
 */

#include "compress.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>

/* Configuration */
#define CHUNK_SIZE 65536  /* 64 KB chunks for efficient I/O */
#define COMPRESSION_LEVEL Z_DEFAULT_COMPRESSION

/* Track statistics */
static struct {
    long files_compressed;
    long files_decompressed;
    long bytes_compressed;
    long bytes_decompressed;
} g_stats = {0};

/* ── helper: compress chunk ─────────────────────────────────── */
static int deflate_chunk(z_stream *strm, int flush,
                         FILE *fin, FILE *fout)
{
    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    int ret;

    /* Read input chunk */
    size_t have = fread(in, 1, CHUNK_SIZE, fin);
    if (ferror(fin)) {
        return Z_ERRNO;
    }

    strm->avail_in = have;
    strm->next_in = in;

    /* Compress until end of input or end of stream */
    do {
        strm->avail_out = CHUNK_SIZE;
        strm->next_out = out;

        ret = deflate(strm, flush);
        if (ret == Z_STREAM_ERROR) {
            return ret;
        }

        size_t have_out = CHUNK_SIZE - strm->avail_out;
        if (fwrite(out, 1, have_out, fout) != have_out || ferror(fout)) {
            return Z_ERRNO;
        }
    } while (strm->avail_out == 0);

    return ret;
}

/* ── helper: decompress chunk ────────────────────────────────── */
static int inflate_chunk(z_stream *strm, FILE *fin, FILE *fout)
{
    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    int ret;

    /* Read compressed chunk */
    size_t have = fread(in, 1, CHUNK_SIZE, fin);
    if (ferror(fin)) {
        return Z_ERRNO;
    }

    if (have == 0) {
        return Z_OK;
    }

    strm->avail_in = have;
    strm->next_in = in;

    /* Decompress until end of stream */
    do {
        strm->avail_out = CHUNK_SIZE;
        strm->next_out = out;

        ret = inflate(strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            return ret;
        }

        size_t have_out = CHUNK_SIZE - strm->avail_out;
        if (fwrite(out, 1, have_out, fout) != have_out || ferror(fout)) {
            return Z_ERRNO;
        }
    } while (strm->avail_out == 0);

    return ret;
}

/* ── compression ────────────────────────────────────────────── */
int compress_file(const char *source, const char *dest, int thread_id)
{
    if (!source || !dest) {
        log_operation("[Thread-%d] COMPRESS: Invalid arguments", thread_id);
        return -1;
    }

    log_operation("[Thread-%d] COMPRESS: Starting '%s' → '%s'",
                  thread_id, source, dest);

    FILE *fin = fopen(source, "rb");
    if (!fin) {
        log_operation("[Thread-%d] COMPRESS: ERROR opening source '%s'",
                      thread_id, source);
        return -1;
    }

    FILE *fout = fopen(dest, "wb");
    if (!fout) {
        log_operation("[Thread-%d] COMPRESS: ERROR opening dest '%s'",
                      thread_id, dest);
        fclose(fin);
        return -1;
    }

    /* Initialize compression stream */
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    if (deflateInit(&strm, COMPRESSION_LEVEL) != Z_OK) {
        log_operation("[Thread-%d] COMPRESS: deflateInit failed",
                      thread_id);
        fclose(fin);
        fclose(fout);
        return -1;
    }

    int ret = Z_OK;
    long bytes_in = 0;
    long bytes_out = 0;

    /* Compress all chunks */
    while (ret == Z_OK) {
        /* Track input size */
        long pos_in = ftell(fin);
        ret = deflate_chunk(&strm, Z_NO_FLUSH, fin, fout);
        bytes_in += ftell(fin) - pos_in;
    }

    /* Get final compressed data */
    if (ret == Z_OK) {
        ret = deflate_chunk(&strm, Z_FINISH, fin, fout);
    }

    /* Get output size */
    bytes_out = ftell(fout);

    deflateEnd(&strm);
    fclose(fin);
    fclose(fout);

    if (ret != Z_STREAM_END) {
        log_operation("[Thread-%d] COMPRESS: Compression failed (ret=%d)",
                      thread_id, ret);
        return -1;
    }

    log_operation("[Thread-%d] COMPRESS: Success! %ld → %ld bytes (%.1f%% ratio)",
                  thread_id, bytes_in, bytes_out,
                  (100.0 * bytes_out) / (bytes_in > 0 ? bytes_in : 1));

    g_stats.files_compressed++;
    g_stats.bytes_compressed += bytes_in;

    return 0;
}

/* ── decompression ───────────────────────────────────────────── */
int decompress_file(const char *source, const char *dest, int thread_id)
{
    if (!source || !dest) {
        log_operation("[Thread-%d] DECOMPRESS: Invalid arguments", thread_id);
        return -1;
    }

    log_operation("[Thread-%d] DECOMPRESS: Starting '%s' → '%s'",
                  thread_id, source, dest);

    FILE *fin = fopen(source, "rb");
    if (!fin) {
        log_operation("[Thread-%d] DECOMPRESS: ERROR opening source '%s'",
                      thread_id, source);
        return -1;
    }

    FILE *fout = fopen(dest, "wb");
    if (!fout) {
        log_operation("[Thread-%d] DECOMPRESS: ERROR opening dest '%s'",
                      thread_id, dest);
        fclose(fin);
        return -1;
    }

    /* Initialize decompression stream */
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    if (inflateInit(&strm) != Z_OK) {
        log_operation("[Thread-%d] DECOMPRESS: inflateInit failed",
                      thread_id);
        fclose(fin);
        fclose(fout);
        return -1;
    }

    int ret = Z_OK;
    long bytes_in = 0;
    long bytes_out = 0;

    /* Decompress all chunks */
    while (ret == Z_OK || ret == Z_BUF_ERROR) {
        long pos_in = ftell(fin);
        ret = inflate_chunk(&strm, fin, fout);
        bytes_in += ftell(fin) - pos_in;

        if (feof(fin)) {
            break;
        }
    }

    /* Get output size */
    bytes_out = ftell(fout);

    inflateEnd(&strm);
    fclose(fin);
    fclose(fout);

    if (ret != Z_OK && ret != Z_STREAM_END) {
        log_operation("[Thread-%d] DECOMPRESS: Decompression failed (ret=%d)",
                      thread_id, ret);
        return -1;
    }

    log_operation("[Thread-%d] DECOMPRESS: Success! %ld → %ld bytes",
                  thread_id, bytes_in, bytes_out);

    g_stats.files_decompressed++;
    g_stats.bytes_decompressed += bytes_out;

    return 0;
}

/* ── statistics ─────────────────────────────────────────────── */
char *compress_get_stats(void)
{
    char *stats = malloc(512);
    if (stats) {
        snprintf(stats, 512,
                 "Compression Stats:\n"
                 "  Files compressed: %ld (total %ld bytes)\n"
                 "  Files decompressed: %ld (total %ld bytes)\n",
                 g_stats.files_compressed,
                 g_stats.bytes_compressed,
                 g_stats.files_decompressed,
                 g_stats.bytes_decompressed);
    }
    return stats;
}
