/*
 * compress.h — File Compression/Decompression Module
 *
 * This module provides thread-safe compression and decompression
 * using zlib's deflate/inflate API. A dedicated compression worker
 * thread processes files on demand.
 *
 * Key features:
 *   - Chunk-based reading for memory efficiency
 *   - Synchronous compression/decompression API
 *   - Uses zlib deflate for compression
 *   - Uses zlib inflate for decompression
 */

#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>

/**
 * Compresses a file using zlib deflate algorithm.
 * Reads the source file in chunks and writes compressed data to dest.
 *
 * @param source The source file path
 * @param dest The output compressed file path
 * @param thread_id Thread ID for logging
 * @return 0 on success, -1 on error
 */
int compress_file(const char *source, const char *dest, int thread_id);

/**
 * Decompresses a file using zlib inflate algorithm.
 * Reads the compressed file and writes decompressed data to dest.
 *
 * @param source The compressed file path
 * @param dest The output decompressed file path
 * @param thread_id Thread ID for logging
 * @return 0 on success, -1 on error
 */
int decompress_file(const char *source, const char *dest, int thread_id);

/**
 * Gets compression statistics
 * @return Pointer to a string with stats (caller must free)
 */
char *compress_get_stats(void);

#endif /* COMPRESS_H */
