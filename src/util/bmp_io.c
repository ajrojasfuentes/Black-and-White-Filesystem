// File: src/util/bmp_io.c

#include "../include/util.h"
#include "../include/bwfs_common.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

int util_create_empty_block(const char *fs_dir, uint32_t block_id) {
    size_t bs = BWFS_BLOCK_SIZE_BYTES;
    uint8_t *zeros = calloc(bs, 1);
    if (!zeros) {
        BWFS_LOG_ERROR("calloc failed");
        return -1;
    }
    int res = util_write_block(fs_dir, block_id, zeros, bs);
    free(zeros);
    return res;
}

int util_write_block(const char *fs_dir, uint32_t block_id, const uint8_t *data, size_t len) {
    char path[512];
    snprintf(path, sizeof(path), "%s/block%u.png", fs_dir, block_id);
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        BWFS_LOG_ERROR("open %s", path);
        return -1;
    }
    ssize_t written = write(fd, data, len);
    if (written != (ssize_t)len) {
        BWFS_LOG_ERROR("write %s", path);
        close(fd);
        return -1;
    }
    close(fd);
    BWFS_LOG_INFO("Wrote %zu bytes to block %u", len, block_id);
    return 0;
}

int util_read_block(const char *fs_dir, uint32_t block_id, uint8_t *out, size_t len) {
    char path[512];
    snprintf(path, sizeof(path), "%s/block%u.png", fs_dir, block_id);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        BWFS_LOG_ERROR("open %s", path);
        return -1;
    }
    ssize_t rd = read(fd, out, len);
    if (rd != (ssize_t)len) {
        BWFS_LOG_ERROR("read %s", path);
        close(fd);
        return -1;
    }
    close(fd);
    BWFS_LOG_INFO("Read %zu bytes from block %u", len, block_id);
    return 0;
}
