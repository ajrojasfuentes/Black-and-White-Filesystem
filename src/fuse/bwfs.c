// -----------------------------------------------------------------------------
// File: src/fuse/bwfs.c
// -----------------------------------------------------------------------------
/**
 * \file bwfs.c
 * \brief Capa FUSE 3 completa para BWFS.
 *
 * Funciones implementadas:
 *   - getattr, access, opendir, readdir, mkdir, rmdir
 *   - create, open, read, write, flush, fsync, lseek, unlink, rename
 *   - statfs  (información de espacio libre)
 *
 * Limitaciones deliberadas (MVP):
 *   • Máx. 10 bloques directos por archivo (≈ 1.25 MiB).
 *   • Cada directorio cabe en 1 bloque; no hay sub-bloques adicionales.
 *   • rename() solo dentro del mismo directorio.
 */

#define _GNU_SOURCE     /* Para realpath() y otras funciones GNU */
#include <limits.h>     /* Para PATH_MAX */
#include <time.h>       /* Para struct timespec - DEBE IR ANTES DE FUSE */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define FUSE_USE_VERSION 32
#include <fuse3/fuse.h>

#include "bwfs_common.h"
#include "bitmap.h"
#include "inode.h"
#include "dir.h"
#include "util.h"

#include <string.h>
#include <errno.h>
#include <limits.h>     /* PATH_MAX */
#include <sys/stat.h>   /* S_IFREG, S_IFDIR */

/* Si por alguna razón PATH_MAX no vino del encabezado anterior */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

extern const char *fs_dir;

/* ------------------------------------------------------------------------- */
/* Estado global                                                             */
/* ------------------------------------------------------------------------- */
static bwfs_superblock_t g_sb;
static bwfs_bitmap_t     g_bm;

/* ------------------------------------------------------------------------- */
/* Resolución de rutas                                                       */
/* ------------------------------------------------------------------------- */
static int bwfs_resolve(const char *path, bwfs_inode_t *out)
{
    if (strcmp(path, "/") == 0)
        return bwfs_read_inode(g_sb.root_inode, out, fs_dir);

    char buf[PATH_MAX];
    strncpy(buf, path, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    bwfs_inode_t cur;
    if (bwfs_read_inode(g_sb.root_inode, &cur, fs_dir) != BWFS_OK)
        return -ENOENT;

    char *save = NULL;
    char *tok  = strtok_r(buf + 1, "/", &save);
    while (tok) {
        if (!(cur.flags & BWFS_INODE_DIR))
            return -ENOENT;

        uint32_t ino = bwfs_dir_lookup(&cur, fs_dir, tok);
        if (ino == UINT32_MAX)
            return -ENOENT;

        if (bwfs_read_inode(ino, &cur, fs_dir) != BWFS_OK)
            return -ENOENT;

        tok = strtok_r(NULL, "/", &save);
    }
    *out = cur;
    return BWFS_OK;
}

/* ------------------------------------------------------------------------- */
/* Utilidades de manipulación de rutas                                       */
/* ------------------------------------------------------------------------- */
static int split_path(const char *path,
                      char       *parent,   /* PATH_MAX */
                      char       *name)     /* BWFS_NAME_MAX+1 */
{
    strncpy(parent, path, PATH_MAX - 1);
    parent[PATH_MAX - 1] = '\0';

    char *slash = strrchr(parent, '/');
    if (!slash) return -EINVAL;

    strncpy(name, slash + 1, BWFS_NAME_MAX);
    name[BWFS_NAME_MAX] = '\0';

    *slash = '\0';
    if (strlen(parent) == 0) strcpy(parent, "/");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Operaciones FUSE                                                          */
/* ------------------------------------------------------------------------- */
static int op_access(const char *path, int mask)
{
    (void)mask;                 /* permisos no implementados */
    bwfs_inode_t tmp;
    return (bwfs_resolve(path, &tmp) == BWFS_OK) ? 0 : -ENOENT;
}

static int op_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi)
{
    (void)fi;
    memset(st, 0, sizeof *st);

    bwfs_inode_t ino;
    if (bwfs_resolve(path, &ino) != BWFS_OK)
        return -ENOENT;

    st->st_mode  = (ino.flags & BWFS_INODE_DIR) ? (S_IFDIR | 0755)
                                               : (S_IFREG | 0644);
    st->st_nlink = 1;
    st->st_size  = ino.size;
    st->st_blocks = ino.block_count;
    return 0;
}

static int op_opendir(const char *path, struct fuse_file_info *fi)
{
    bwfs_inode_t dir;
    if (bwfs_resolve(path, &dir) != BWFS_OK)
        return -ENOENT;
    if (!(dir.flags & BWFS_INODE_DIR))
        return -ENOTDIR;
    return 0;
}

static int op_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t off, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    (void)off; (void)fi; (void)flags;

    bwfs_inode_t dir;
    if (bwfs_resolve(path, &dir) != BWFS_OK)
        return -ENOENT;

    if (!(dir.flags & BWFS_INODE_DIR))
        return -ENOTDIR;

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (dir.block_count == 0) return 0;

    size_t max = BWFS_BLOCK_SIZE_BYTES / sizeof(bwfs_dir_entry_t);
    bwfs_dir_entry_t *entries = malloc(max * sizeof *entries);
    if (!entries) return -ENOMEM;

    if (util_read_block(fs_dir, dir.blocks[0],
                        (uint8_t *)entries, BWFS_BLOCK_SIZE_BYTES) != 0) {
        free(entries); return -EIO;
    }

    for (size_t i = 0; i < max; ++i)
        if (entries[i].ino)
            filler(buf, entries[i].name, NULL, 0, 0);

    free(entries);
    return 0;
}

static int op_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    char parent[PATH_MAX], name[BWFS_NAME_MAX + 1];
    if (split_path(path, parent, name) != 0) return -EINVAL;

    bwfs_inode_t pdir;
    if (bwfs_resolve(parent, &pdir) != BWFS_OK) return -ENOENT;
    if (!(pdir.flags & BWFS_INODE_DIR))        return -ENOTDIR;

    uint32_t ino = bwfs_create_inode(&g_bm, true, fs_dir);
    if (ino == UINT32_MAX) return -ENOSPC;

    if (bwfs_dir_add(&g_bm, &pdir, fs_dir, name, ino) != BWFS_OK)
        return -EIO;
    return 0;
}

static int op_rmdir(const char *path)
{
    char parent[PATH_MAX], name[BWFS_NAME_MAX + 1];
    if (split_path(path, parent, name) != 0) return -EINVAL;

    bwfs_inode_t pdir;
    if (bwfs_resolve(parent, &pdir) != BWFS_OK) return -ENOENT;

    uint32_t ino = bwfs_dir_lookup(&pdir, fs_dir, name);
    if (ino == UINT32_MAX) return -ENOENT;

    bwfs_inode_t dir;
    if (bwfs_read_inode(ino, &dir, fs_dir) != BWFS_OK) return -EIO;
    if (!(dir.flags & BWFS_INODE_DIR)) return -ENOTDIR;
    if (dir.size > 0)                  return -ENOTEMPTY;

    bwfs_free_blocks(&g_bm, dir.blocks[0], dir.block_count);
    bwfs_free_blocks(&g_bm, ino, 1);
    bwfs_write_bitmap(&g_bm, fs_dir);

    if (bwfs_dir_remove(&pdir, fs_dir, name) != BWFS_OK)
        return -EIO;
    return 0;
}

static int op_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void)mode; (void)fi;
    char parent[PATH_MAX], name[BWFS_NAME_MAX + 1];
    if (split_path(path, parent, name) != 0) return -EINVAL;

    bwfs_inode_t pdir;
    if (bwfs_resolve(parent, &pdir) != BWFS_OK) return -ENOENT;

    uint32_t ino = bwfs_create_inode(&g_bm, false, fs_dir);
    if (ino == UINT32_MAX) return -ENOSPC;

    if (bwfs_dir_add(&g_bm, &pdir, fs_dir, name, ino) != BWFS_OK)
        return -EIO;
    return 0;
}

static int op_open(const char *path, struct fuse_file_info *fi)
{
    bwfs_inode_t tmp;
    return (bwfs_resolve(path, &tmp) == BWFS_OK) ? 0 : -ENOENT;
}

static int op_read(const char *path, char *buf, size_t size, off_t off,
                   struct fuse_file_info *fi)
{
    (void)fi;
    bwfs_inode_t ino;
    if (bwfs_resolve(path, &ino) != BWFS_OK) return -ENOENT;
    if (off >= (off_t)ino.size) return 0;

    size_t want = (off + size > ino.size) ? ino.size - off : size;
    size_t done = 0, block_sz = BWFS_BLOCK_SIZE_BYTES;

    uint8_t *block_buf = malloc(BWFS_BLOCK_SIZE_BYTES);
    while (done < want) {
        uint32_t blk_idx = (off + done) / block_sz;
        uint32_t blk_off = (off + done) % block_sz;
        uint32_t blk     = ino.blocks[blk_idx];
        size_t chunk = MIN(block_sz - blk_off, want - done);

        // Lee bloque completo a buffer temporal
        if (util_read_block(fs_dir, blk, block_buf, block_sz) != 0) {
            free(block_buf); return -EIO;
        }
        
        // Copia solo la porción necesaria
        memcpy(buf + done, block_buf + blk_off, chunk);
        done += chunk;
    }
    free(block_buf);
    return (int)want;

}

static int op_write(const char *path, const char *buf, size_t size, off_t off,
                    struct fuse_file_info *fi)
{
    (void)fi;
    bwfs_inode_t ino;
    if (bwfs_resolve(path, &ino) != BWFS_OK) return -ENOENT;
    if (ino.flags & BWFS_INODE_DIR) return -EISDIR;  // No escribir en directorios

    uint32_t end = off + size;
    if (end > ino.size && bwfs_inode_resize(&g_bm, &ino, end, fs_dir) != BWFS_OK)
        return -ENOSPC;

    size_t done = 0;
    const size_t block_sz = BWFS_BLOCK_SIZE_BYTES;
    
    // Buffer temporal reutilizable
    uint8_t *block_buf = malloc(block_sz);
    if (!block_buf) return -ENOMEM;

    while (done < size) {
        uint32_t blk_idx = (off + done) / block_sz;
        uint32_t blk_off = (off + done) % block_sz;
        
        if (blk_idx >= BWFS_DIRECT_BLOCKS) {
            free(block_buf);
            return -EFBIG;  // Archivo demasiado grande
        }
        
        uint32_t blk = ino.blocks[blk_idx];
        size_t chunk = block_sz - blk_off;
        if (chunk > size - done) chunk = size - done;

        // Solo leer si no escribimos el bloque completo
        if (blk_off > 0 || chunk < block_sz) {
            if (util_read_block(fs_dir, blk, block_buf, block_sz) != 0) {
                free(block_buf);
                return -EIO;
            }
        }

        // Copiar datos al buffer
        memcpy(block_buf + blk_off, buf + done, chunk);

        // Escribir bloque modificado
        if (util_write_block(fs_dir, blk, block_buf, block_sz) != 0) {
            free(block_buf);
            return -EIO;
        }

        done += chunk;
    }

    free(block_buf);
    return (int)size;
}

static int op_flush(const char *path, struct fuse_file_info *fi)
{ (void)path; (void)fi; return 0; }

static int op_fsync(const char *path, int datasync,
                    struct fuse_file_info *fi)
{ (void)path; (void)datasync; (void)fi; return 0; }

static int op_rename(const char *from, const char *to, unsigned int flags)
{
    if (flags) return -EINVAL;  /* no se soporta RENAME_EXCHANGE */

    char p_from[PATH_MAX], n_from[BWFS_NAME_MAX+1];
    char p_to  [PATH_MAX], n_to  [BWFS_NAME_MAX+1];

    if (split_path(from, p_from, n_from) != 0 ||
        split_path(to,   p_to,   n_to)   != 0) return -EINVAL;

    /* solo renames dentro del mismo directorio */
    if (strcmp(p_from, p_to) != 0) return -EXDEV;

    bwfs_inode_t dir;
    if (bwfs_resolve(p_from, &dir) != BWFS_OK) return -ENOENT;

    uint32_t child = bwfs_dir_lookup(&dir, fs_dir, n_from);
    if (child == UINT32_MAX) return -ENOENT;

    /* quitar vieja entrada, añadir nueva */
    if (bwfs_dir_remove(&dir, fs_dir, n_from) != BWFS_OK) return -EIO;
    if (bwfs_dir_add(NULL, &dir, fs_dir, n_to, child) != BWFS_OK) return -EIO;
    return 0;
}

static int op_unlink(const char *path)
{
    char parent[PATH_MAX], name[BWFS_NAME_MAX + 1];
    if (split_path(path, parent, name) != 0) return -EINVAL;

    bwfs_inode_t pdir;
    if (bwfs_resolve(parent, &pdir) != BWFS_OK) return -ENOENT;

    uint32_t ino = bwfs_dir_lookup(&pdir, fs_dir, name);
    if (ino == UINT32_MAX) return -ENOENT;

    bwfs_inode_t file;
    if (bwfs_read_inode(ino, &file, fs_dir) != BWFS_OK) return -EIO;

    bwfs_free_blocks(&g_bm, ino, 1);
    bwfs_free_blocks(&g_bm, file.blocks[0], file.block_count);
    bwfs_write_bitmap(&g_bm, fs_dir);

    if (bwfs_dir_remove(&pdir, fs_dir, name) != BWFS_OK) return -EIO;
    return 0;
}

static off_t op_lseek(const char *path, off_t off, int whence,
                      struct fuse_file_info *fi)
{
    (void)fi;
    bwfs_inode_t ino;
    if (bwfs_resolve(path, &ino) != BWFS_OK)
        return -ENOENT;

    off_t new_off = off;

    switch (whence) {
        case SEEK_SET:
            /* new_off ya es el valor absoluto proporcionado */
            break;
        case SEEK_END:
            new_off = ino.size + off;
            break;
        case SEEK_CUR:        /* no llevamos offset interno → no soportado */
        default:
            return -EINVAL;
    }

    if (new_off < 0) return -EINVAL;
    return new_off;
}


static int op_statfs(const char *path, struct statvfs *st)
{
    (void)path;
    memset(st, 0, sizeof *st);

    uint32_t total = g_sb.total_blocks;
    uint32_t used  = 0;
    for (uint32_t i = 0; i < total; ++i)
        used += bwfs_bm_test(&g_bm, i);

    st->f_bsize   = BWFS_BLOCK_SIZE_BYTES;
    st->f_blocks  = total;
    st->f_bfree   = total - used;
    st->f_bavail  = total - used;
    st->f_namemax = BWFS_NAME_MAX;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* init / destroy                                                            */
/* ------------------------------------------------------------------------- */
static void *op_init(struct fuse_conn_info *c, struct fuse_config *cfg)
{
    (void)c; (void)cfg;
    if (bwfs_read_superblock(&g_sb, fs_dir) != BWFS_OK) return NULL;
    g_bm.total_blocks = g_sb.total_blocks;
    if (bwfs_read_bitmap(&g_bm, fs_dir) != BWFS_OK) { free(g_bm.map); return NULL; }
    return &g_sb;
}
static void op_destroy(void *ud) { (void)ud; free(g_bm.map); }

/* ------------------------------------------------------------------------- */
/* Tabla de operaciones                                                      */
/* ------------------------------------------------------------------------- */
struct fuse_operations bwfs_ops = {
    .init      = op_init,
    .destroy   = op_destroy,
    .access    = op_access,
    .getattr   = op_getattr,
    .opendir   = op_opendir,
    .readdir   = op_readdir,
    .mkdir     = op_mkdir,
    .rmdir     = op_rmdir,
    .create    = op_create,
    .open      = op_open,
    .read      = op_read,
    .write     = op_write,
    .flush     = op_flush,
    .fsync     = op_fsync,
    .lseek     = op_lseek,
    .unlink    = op_unlink,
    .rename    = op_rename,
    .statfs    = op_statfs,
};
