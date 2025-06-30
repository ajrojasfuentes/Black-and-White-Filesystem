// -----------------------------------------------------------------------------
// File: src/core/dir.c
// -----------------------------------------------------------------------------
/**
 * \file dir.c
 * \brief Manipulación de directorios para BWFS
 *
 *  - Cada directorio se almacena inicialmente en **un solo bloque** de datos
 *    que contiene un arreglo fijo de registros \ref bwfs_dir_entry_t.
 *  - El bloque se asigna bajo demanda cuando se inserta la primera entrada.
 *  - Para la versión mínima NO se soporta la expansión a múltiples bloques;
 *    si el bloque se llena, la función devuelve \c BWFS_ERR_FULL.
 */

#include "dir.h"
#include "bitmap.h"
#include "util.h"
#include "allocation.h"

#include <string.h>   /* strncpy, strcmp */
#include <stdlib.h>   /* malloc, calloc, free */

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

/** Número máximo de entradas por bloque-directorio. */
static inline size_t max_entries_per_block(void)
{
    return BWFS_BLOCK_SIZE_BYTES / sizeof(bwfs_dir_entry_t);
}

/**
 * \brief Lee todas las entradas del bloque-directorio a un buffer reservado.
 *
 * \param dir_inode  Inodo del directorio (debe tener \c block_count>0).
 * \param fs_dir     Ruta al disco BWFS.
 * \param[out] out   Buffer destino (tamaño \c max_entries_per_block()).
 * \retval BWFS_OK o código BWFS_ERR_*
 */
static int load_entries(const bwfs_inode_t *dir_inode,
                        const char         *fs_dir,
                        bwfs_dir_entry_t   *out)
{
    if (util_read_block(fs_dir,
                        dir_inode->blocks[0],
                        (uint8_t *)out,
                        BWFS_BLOCK_SIZE_BYTES) != 0)
        return BWFS_ERR_IO;

    return BWFS_OK;
}

/**
 * \brief Graba el arreglo completo de entradas al bloque-directorio.
 */
static int store_entries(const bwfs_inode_t *dir_inode,
                         const char         *fs_dir,
                         const bwfs_dir_entry_t *entries)
{
    return util_write_block(fs_dir,
                            dir_inode->blocks[0],
                            (const uint8_t *)entries,
                            BWFS_BLOCK_SIZE_BYTES) ? BWFS_ERR_IO : BWFS_OK;
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

int bwfs_dir_add(bwfs_bitmap_t *bm,
                 bwfs_inode_t  *dir_inode,
                 const char    *fs_dir,
                 const char    *name,
                 uint32_t       child_ino)
{
    /* ------------------------------------------------------------------ */
    /* 1) Verificar/crear el primer bloque de datos del directorio        */
    /* ------------------------------------------------------------------ */
    if (dir_inode->block_count == 0) {
        if (!bm) return BWFS_ERR_FULL;          /* imposible asignar */

        uint32_t blk = bwfs_alloc_blocks(bm, 1);
        if (blk == UINT32_MAX) return BWFS_ERR_FULL;

        /* Inicializar bloque a ceros */
        uint8_t *zero = (uint8_t *)calloc(1, BWFS_BLOCK_SIZE_BYTES);
        if (!zero) { bwfs_free_blocks(bm, blk, 1); return BWFS_ERR_NOMEM; }
        util_write_block(fs_dir, blk, zero, BWFS_BLOCK_SIZE_BYTES);
        free(zero);

        dir_inode->blocks[0]   = blk;
        dir_inode->block_count = 1;
        dir_inode->size        = 0;

        /* Persistir metadatos */
        if (bwfs_write_bitmap(bm, fs_dir) != BWFS_OK ||
            bwfs_write_inode(dir_inode, fs_dir) != BWFS_OK)
            return BWFS_ERR_IO;
    }

    /* ------------------------------------------------------------------ */
    /* 2) Cargar las entradas existentes                                  */
    /* ------------------------------------------------------------------ */
    const size_t max = max_entries_per_block();
    bwfs_dir_entry_t *entries =
        (bwfs_dir_entry_t *)malloc(max * sizeof *entries);
    if (!entries) return BWFS_ERR_NOMEM;

    if (load_entries(dir_inode, fs_dir, entries) != BWFS_OK) {
        free(entries);
        return BWFS_ERR_IO;
    }

    /* ------------------------------------------------------------------ */
    /* 3) Buscar espacio libre y evitar duplicados                        */
    /* ------------------------------------------------------------------ */
    size_t free_idx = max;
    for (size_t i = 0; i < max; ++i) {
        if (entries[i].ino == 0 && free_idx == max)
            free_idx = i;                       /* primera ranura libre */
        if (entries[i].ino != 0 &&
            strncmp(entries[i].name, name, BWFS_NAME_MAX) == 0) {
            free(entries);
            return BWFS_ERR_FULL;               /* nombre ya existe    */
        }
    }

    if (free_idx == max) {                      /* bloque lleno */
        free(entries);
        return BWFS_ERR_FULL;
    }

    /* ------------------------------------------------------------------ */
    /* 4) Rellenar nueva entrada                                           */
    /* ------------------------------------------------------------------ */
    entries[free_idx].ino = child_ino;
    strncpy(entries[free_idx].name, name, BWFS_NAME_MAX);
    entries[free_idx].name[BWFS_NAME_MAX] = '\0';

    dir_inode->size += sizeof(bwfs_dir_entry_t);

    /* ------------------------------------------------------------------ */
    /* 5) Persistir bloque e inodo                                         */
    /* ------------------------------------------------------------------ */
    int rc = store_entries(dir_inode, fs_dir, entries);
    free(entries);
    if (rc != BWFS_OK) return rc;

    return bwfs_write_inode(dir_inode, fs_dir);
}

int bwfs_dir_remove(bwfs_inode_t *dir_inode,
                    const char   *fs_dir,
                    const char   *name)
{
    if (dir_inode->block_count == 0) return UINT32_MAX; /* directorio vacío */

    const size_t max = max_entries_per_block();
    bwfs_dir_entry_t *entries =
        (bwfs_dir_entry_t *)malloc(max * sizeof *entries);
    if (!entries) return BWFS_ERR_NOMEM;

    if (load_entries(dir_inode, fs_dir, entries) != BWFS_OK) {
        free(entries);
        return BWFS_ERR_IO;
    }

    for (size_t i = 0; i < max; ++i) {
        if (entries[i].ino != 0 &&
            strncmp(entries[i].name, name, BWFS_NAME_MAX) == 0)
        {
            /* Marcar como libre */
            entries[i].ino      = 0;
            entries[i].name[0]  = '\0';
            dir_inode->size    -= sizeof(bwfs_dir_entry_t);

            int rc = store_entries(dir_inode, fs_dir, entries);
            free(entries);
            if (rc != BWFS_OK) return rc;

            return bwfs_write_inode(dir_inode, fs_dir);
        }
    }

    free(entries);
    return UINT32_MAX;                           /* no encontrado */
}

uint32_t bwfs_dir_lookup(const bwfs_inode_t *dir_inode,
                         const char         *fs_dir,
                         const char         *name)
{
    if (dir_inode->block_count == 0) return UINT32_MAX;

    const size_t max = max_entries_per_block();
    bwfs_dir_entry_t *entries =
        (bwfs_dir_entry_t *)malloc(max * sizeof *entries);
    if (!entries) return UINT32_MAX;

    if (load_entries(dir_inode, fs_dir, entries) != BWFS_OK) {
        free(entries);
        return UINT32_MAX;
    }

    for (size_t i = 0; i < max; ++i) {
        if (entries[i].ino != 0 &&
            strncmp(entries[i].name, name, BWFS_NAME_MAX) == 0) {
            uint32_t found = entries[i].ino;
            free(entries);
            return found;
        }
    }

    free(entries);
    return UINT32_MAX;                           /* no encontrado */
}
