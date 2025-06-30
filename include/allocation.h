// File: include/allocation.h
#ifndef ALLOCATION_H
#define ALLOCATION_H

#include <stdint.h>
#include "bwfs_common.h"

/**
 * Allocate a contiguous region of free blocks using worst-fit strategy.
 * @param bm     Pointer to the bitmap tracking blocks.
 * @param count  Number of contiguous blocks requested.
 * @return Index of the first block in the allocated region, or UINT32_MAX on failure.
 */
uint32_t bwfs_alloc_blocks(bwfs_bitmap_t *bm, uint32_t count);

/**
 * Free a previously allocated region of blocks.
 * @param bm     Pointer to the bitmap tracking blocks.
 * @param start  Starting block index of the region to free.
 * @param count  Number of contiguous blocks to free.
 */
void bwfs_free_blocks(bwfs_bitmap_t *bm, uint32_t start, uint32_t count);

#endif // ALLOCATION_H