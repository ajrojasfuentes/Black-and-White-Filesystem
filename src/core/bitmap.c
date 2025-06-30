// -----------------------------------------------------------------------------
// File: src/core/bitmap.c
// -----------------------------------------------------------------------------
/**
 * \file bitmap.c
 * \brief Persistencia y carga del mapa de bits de bloques BWFS.
 *
 * El mapa de bits se guarda íntegro en el **bloque lógico 1**.  Cada bit
 * representa el estado (0 = libre, 1 = ocupado) de un bloque lógico de datos.
 */

#include "bitmap.h"
#include "util.h"

#include <stdlib.h>   /* malloc, free */

/* ------------------------------------------------------------------------- */
/* Operaciones públicas                                                      */
/* ------------------------------------------------------------------------- */

/**
 * \brief Escribe a disco el mapa de bits completo.
 *
 * \param[in] bm     Bitmap en memoria (con `total_blocks` y `map` válidos).
 * \param[in] fs_dir Directorio raíz del sistema de archivos.
 * \retval BWFS_OK       Éxito.
 * \retval BWFS_ERR_IO   No se pudo escribir el bloque 1.
 */
int bwfs_write_bitmap(const bwfs_bitmap_t *bm, const char *fs_dir)
{
    size_t bytes = (bm->total_blocks + 7) / 8;

    if (util_write_block(fs_dir, BWFS_BITMAP_BLK, bm->map, bytes) != 0)
        return BWFS_ERR_IO;

    BWFS_LOG_INFO("Bitmap escrito (%u bloques gestionados)", bm->total_blocks);
    return BWFS_OK;
}

/**
 * \brief Carga el mapa de bits desde disco.
 *
 * La función reserva memoria para `bm->map`; el llamante debe liberarla con
 * `free()` cuando ya no sea necesaria.
 *
 * \param[in,out] bm  Estructura a rellenar (debe traer `total_blocks`).
 * \param[in]     fs_dir Directorio raíz del FS.
 * \retval BWFS_OK        Éxito.
 * \retval BWFS_ERR_NOMEM Memoria insuficiente para el buffer.
 * \retval BWFS_ERR_IO    Fallo de lectura del bloque 1.
 */
int bwfs_read_bitmap(bwfs_bitmap_t *bm, const char *fs_dir)
{
    size_t bytes = (bm->total_blocks + 7) / 8;

    bm->map = (uint8_t *)malloc(bytes);
    if (!bm->map)
        return BWFS_ERR_NOMEM;

    if (util_read_block(fs_dir, BWFS_BITMAP_BLK, bm->map, bytes) != 0) {
        free(bm->map);
        return BWFS_ERR_IO;
    }

    bm->bits_per_block = BWFS_BLOCK_SIZE_BITS;
    return BWFS_OK;
}
