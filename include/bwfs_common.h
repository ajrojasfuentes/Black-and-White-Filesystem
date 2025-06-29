#ifndef BWFS_COMMON_H
#define BWFS_COMMON_H

#include <stdint.h>
#include <stdbool.h>

// Fixed parameters for BWFS
#define BWFS_BLOCK_SIZE_BITS 1000000   // Logical block size: 1000 x 1000 bits
#define BWFS_BLOCK_SIZE_BYTES (BWFS_BLOCK_SIZE_BITS / 8)  // Bytes per block

#define BWFS_MAGIC 0x42465753  // 'BWFS' magic number for validation

/**
 * Superblock: stored in the first block (PNG) of the filesystem.
 * Contains global metadata for the filesystem.
 */
typedef struct {
    uint32_t magic;          // Magic number to identify BWFS
    uint32_t total_blocks;   // Total number of logical blocks
    uint32_t block_size;     // Block size in bits (should equal BWFS_BLOCK_SIZE_BITS)
    uint32_t inode_count;    // Total number of inodes in the filesystem
    uint32_t root_inode;     // Inode number of the root directory
    // Future extension: flags for encryption, compression, etc.
} bwfs_superblock_t;

/**
 * Inode: represents a file or directory.
 * Each inode maps to metadata and data blocks.
 */
typedef struct {
    uint32_t ino;            // Inode number (unique identifier)
    bool is_directory;       // True if directory, false if regular file
    uint32_t size;           // File size in bytes
    uint32_t block_count;    // Number of data blocks allocated
    uint32_t blocks[];       // Flexible array of block indices
    // Note: allocate block pointers in allocation.c using worst-fit
} bwfs_inode_t;

/**
 * Directory Entry: fixed-size record inside a directory block.
 * Maps a name to an inode.
 */
#define BWFS_NAME_MAX 255

typedef struct {
    uint32_t ino;                    // Inode number
    char name[BWFS_NAME_MAX + 1];   // Null-terminated filename
} bwfs_dir_entry_t;

/**
 * Bitmap: tracks free/allocated blocks.
 * Stored as a sequence of bits, using BWFS_BLOCK_SIZE_BYTES per block.
 */
typedef struct {
    uint32_t bits_per_block;        // Should equal BWFS_BLOCK_SIZE_BITS
    uint32_t total_blocks;          // Number of blocks tracked
    uint8_t *map;                   // Byte array: 1 bit = 1 block
} bwfs_bitmap_t;

#endif // BWFS_COMMON_H
