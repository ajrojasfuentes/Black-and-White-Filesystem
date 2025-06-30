// File: include/util.h
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/**
 * Logging macros for BWFS - Versión corregida
 */
#define BWFS_LOG_INFO(fmt, ...) \
    fprintf(stdout, "[BWFS INFO] " fmt "\n", ##__VA_ARGS__)

#define BWFS_LOG_ERROR(fmt, ...) \
    do { \
        fprintf(stderr, "[BWFS ERROR] " fmt, ##__VA_ARGS__); \
        if (errno != 0) fprintf(stderr, " (%s)", strerror(errno)); \
        fprintf(stderr, "\n"); \
    } while(0)

/**
 * Create an empty block file (PNG image) initialized to all black pixels.
 * @param fs_dir   Filesystem directory
 * @param block_id Block index
 * @return 0 on success, -1 on failure
 */
int util_create_empty_block(const char *fs_dir, uint32_t block_id);

/**
 * Write raw data to a block file (PNG image).
 * Data is converted from bits to black/white pixels.
 * @param fs_dir   Filesystem directory
 * @param block_id Block index
 * @param data     Pointer to data buffer (max BWFS_BLOCK_SIZE_BYTES)
 * @param len      Length of data (bytes)
 * @return 0 on success, -1 on failure
 */
int util_write_block(const char *fs_dir, uint32_t block_id, const uint8_t *data, size_t len);

/**
 * Read raw data from a block file (PNG image).
 * Pixels are converted back to bits.
 * @param fs_dir   Filesystem directory
 * @param block_id Block index
 * @param out      Buffer to receive data (must be at least len bytes)
 * @param len      Length of data to read (bytes, max BWFS_BLOCK_SIZE_BYTES)
 * @return 0 on success, -1 on failure
 */
int util_read_block(const char *fs_dir, uint32_t block_id, uint8_t *out, size_t len);

#endif // UTIL_H