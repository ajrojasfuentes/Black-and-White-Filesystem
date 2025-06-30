// -----------------------------------------------------------------------------
// File: src/core/inode.c
// -----------------------------------------------------------------------------
/**
 * \file inode.c
 * \brief Gestión completa de inodos (crear, leer, escribir, redimensionar).
 *
 * - Cada inodo ocupa exactamente **un bloque lógico** del disco.
 * - Para la versión mínima se admiten solo 10 bloques directos
 *   (no se implementa aún el bloque indirecto).
 * - Todas las escrituras de metadatos actualizan también el bitmap.
 */

#include "inode.h"
#include "allocation.h"
#include "bitmap.h"
#include "util.h"

#include <string.h>   /* memset, strncpy */
#include <stdlib.h>   /* calloc, free   */
#include <limits.h>   /* UINT32_MAX     */

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares privadas                                             */
/* ------------------------------------------------------------------------- */

/**
 * \brief Cero-inicializa un rango de punteros de bloque en un inodo.
 */
static void zero_blocks(bwfs_inode_t *inode, uint32_t from, uint32_t to)
{
    for (uint32_t i = from; i < to && i < BWFS_DIRECT_BLOCKS; ++i)
        inode->blocks[i] = 0;
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

uint32_t bwfs_create_inode(bwfs_bitmap_t *bm,
                           bool           is_dir,
                           const char    *fs_dir)
{
    /* 1. Reservar UN bloque para el inodo (sus metadatos). */
    uint32_t ino_blk = bwfs_alloc_blocks(bm, 1);
    if (ino_blk == UINT32_MAX)
        return UINT32_MAX;

    /* 2. Construir la estructura en memoria. */
    bwfs_inode_t inode;
    memset(&inode, 0, sizeof inode);

    inode.ino         = ino_blk;
    inode.size        = 0;
    inode.block_count = 0;
    inode.flags       = is_dir ? BWFS_INODE_DIR : 0;
    /* blocks[] e indirect ya quedaron en cero vía memset */

    /* 3. Persistir (inodo + bitmap). */
    if (bwfs_write_inode(&inode, fs_dir) != BWFS_OK ||
        bwfs_write_bitmap(bm, fs_dir)   != BWFS_OK)
    {
        /* Roll-back: liberar el bloque si algo falla. */
        bwfs_free_blocks(bm, ino_blk, 1);
        bwfs_write_bitmap(bm, fs_dir);      /* mejor esfuerzo */
        return UINT32_MAX;
    }

    return ino_blk;
}

int bwfs_write_inode(const bwfs_inode_t *inode, const char *fs_dir)
{
    return util_write_block(fs_dir,
                            inode->ino,
                            (const uint8_t *)inode,
                            sizeof *inode) ? BWFS_ERR_IO : BWFS_OK;
}

int bwfs_read_inode(uint32_t ino, bwfs_inode_t *inode, const char *fs_dir)
{
    return util_read_block(fs_dir,
                           ino,
                           (uint8_t *)inode,
                           sizeof *inode) ? BWFS_ERR_IO : BWFS_OK;
}

int bwfs_inode_resize(bwfs_bitmap_t *bm,
                      bwfs_inode_t   *inode,
                      uint32_t        new_size,
                      const char     *fs_dir)
{
    /* Cálculo de bloques actuales y requeridos */
    uint32_t cur_blocks = inode->block_count;
    uint32_t req_blocks = (new_size + BWFS_BLOCK_SIZE_BYTES - 1)
                          / BWFS_BLOCK_SIZE_BYTES;

    if (req_blocks > BWFS_DIRECT_BLOCKS)
        return BWFS_ERR_FULL; /* indirectos aún no soportados */

    /* ------------------------------------------------------------------ */
    /* Expansión                                                          */
    /* ------------------------------------------------------------------ */
    if (req_blocks > cur_blocks) {
        uint32_t add = req_blocks - cur_blocks;
        for (uint32_t i = 0; i < add; ++i) {
            uint32_t blk = bwfs_alloc_blocks(bm, 1);
            if (blk == UINT32_MAX) {
                /* Rollback parcial si falta espacio */
                bwfs_free_blocks(bm, inode->blocks[cur_blocks], i);
                bwfs_write_bitmap(bm, fs_dir);
                return BWFS_ERR_FULL;
            }
            inode->blocks[cur_blocks + i] = blk;
        }
        inode->block_count = req_blocks;
    }
    /* ------------------------------------------------------------------ */
    /* Contracción                                                        */
    /* ------------------------------------------------------------------ */
    else if (req_blocks < cur_blocks) {
        for (uint32_t i = req_blocks; i < cur_blocks; ++i)
            bwfs_free_blocks(bm, inode->blocks[i], 1);

        zero_blocks(inode, req_blocks, cur_blocks);
        inode->block_count = req_blocks;
    }

    inode->size = new_size;

    /* Persistir cambios (bitmap + inodo). */
    if (bwfs_write_bitmap(bm, fs_dir) != BWFS_OK ||
        bwfs_write_inode(inode, fs_dir) != BWFS_OK)
        return BWFS_ERR_IO;

    return BWFS_OK;
}
