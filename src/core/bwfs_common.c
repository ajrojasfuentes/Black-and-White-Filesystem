// File: src/core/bwfs_common.c

#include "../include/bwfs_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initialize the superblock with default values.
 * @param sb     Pointer to superblock structure to initialize.
 * @param blocks Total number of blocks available.
 */
void bwfs_init_superblock(bwfs_superblock_t *sb, uint32_t blocks) {
    sb->magic        = BWFS_MAGIC;
    sb->total_blocks = blocks;
    sb->block_size   = BWFS_BLOCK_SIZE_BITS;
    sb->inode_count  = 0;
    sb->root_inode   = 0;
}

/**
 * Read superblock from disk (block 0).
 * @param sb   Pointer to superblock structure to fill.
 * @param path Filesystem directory.
 * @return 0 on success, -1 on error.
 */
int bwfs_read_superblock(bwfs_superblock_t *sb, const char *path) {
    // TODO: integrate util_read_block for block 0
    if (sb->magic != BWFS_MAGIC) {
        fprintf(stderr, "Invalid BWFS magic: 0x%08x\n", sb->magic);
        return -1;
    }
    return 0;
}

/**
 * Initialize a bitmap tracking free blocks.
 * @param bm     Pointer to bitmap structure.
 * @param blocks Total number of blocks to track.
 */
void bwfs_init_bitmap(bwfs_bitmap_t *bm, uint32_t blocks) {
    bm->bits_per_block = BWFS_BLOCK_SIZE_BITS;
    bm->total_blocks   = blocks;
    size_t bytes = (blocks + 7) / 8;
    bm->map = calloc(bytes, 1);
    if (!bm->map) {
        perror("bwfs_init_bitmap: calloc failed");
        exit(EXIT_FAILURE);
    }
    bm->map[0] |= 0x80;  // Reserve block 0 for superblock
    bm->map[1] |= 0x40;  // Reserve block 1 for bitmap
}

