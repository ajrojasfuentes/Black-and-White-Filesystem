// File: src/cli/mkfs_bwfs.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/bwfs_common.h"
#include "../include/util.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: mkfs_bwfs <fs_directory>\n");
        return EXIT_FAILURE;
    }
    const char *fs_dir = argv[1];

    // Create root directory if it doesn't exist
    if (mkdir(fs_dir, 0755) && errno != EEXIST) {
        BWFS_LOG_ERROR("mkdir %s", fs_dir);
        return EXIT_FAILURE;
    }

    // Define total blocks (could be parameterized later)
    uint32_t total_blocks = 1024;

    // Initialize and write superblock to block 0
    bwfs_superblock_t sb;
    bwfs_init_superblock(&sb, total_blocks);
    if (util_write_block(fs_dir, 0, (uint8_t*)&sb, sizeof(sb)) != 0) {
        BWFS_LOG_ERROR("writing superblock block");
        return EXIT_FAILURE;
    }
    BWFS_LOG_INFO("Superblock written to block 0");

    // Initialize bitmap and write to block 1
    bwfs_bitmap_t bm;
    bwfs_init_bitmap(&bm, total_blocks);
    size_t bm_bytes = (total_blocks + 7) / 8;
    if (util_write_block(fs_dir, 1, bm.map, bm_bytes) != 0) {
        BWFS_LOG_ERROR("writing bitmap block");
        free(bm.map);
        return EXIT_FAILURE;
    }
    BWFS_LOG_INFO("Bitmap written to block 1");
    free(bm.map);

    // Create empty blocks for data starting at block 2
    for (uint32_t i = 2; i < total_blocks; i++) {
        if (util_create_empty_block(fs_dir, i) != 0) {
            BWFS_LOG_ERROR("creating empty block %u", i);
            return EXIT_FAILURE;
        }
    }
    BWFS_LOG_INFO("Created %u empty blocks", total_blocks - 2);

    printf("BWFS created: %s with %u blocks\n", fs_dir, total_blocks);
    return EXIT_SUCCESS;
}
