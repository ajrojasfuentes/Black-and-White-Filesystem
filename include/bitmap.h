#ifndef BWFS_BITMAP_H
#define BWFS_BITMAP_H
/**
 * \file bitmap.h
 * \brief Operaciones sobre el mapa de bits de bloques.
 */

#include <stdint.h>
#include "bwfs_common.h"

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

/**
 * \brief Persiste el mapa de bits en el disco (bloque 1).
 *
 * @param bm     Bitmap completo en memoria.
 * @param fs_dir Ruta al directorio que contiene los bloques-PNG.
 * @retval BWFS_OK       Éxito
 * @retval BWFS_ERR_IO   Fallo de escritura
 */
int bwfs_write_bitmap(const bwfs_bitmap_t *bm, const char *fs_dir);

/**
 * \brief Carga el mapa de bits desde disco (bloque 1).
 *
 * @param bm     Estructura destino (debe tener `total_blocks` ya definido).
 * @param fs_dir Directorio del FS.
 * @return       BWFS_OK o código BWFS_ERR_*
 */
int bwfs_read_bitmap(bwfs_bitmap_t *bm, const char *fs_dir);

/* ------------------------------------------------------------------------- */
/* Utilidades inline                                                         */
/* ------------------------------------------------------------------------- */

/** Macro interna para obtener byte y bit dentro del bitmap. */
#define _BWFS_BM_INDEX(bit)   ((bit) / 8)
#define _BWFS_BM_MASK(bit)    (1U << ((bit) % 8))

/** Devuelve 1 si el bloque está ocupado, 0 si libre. */
static inline int bwfs_bm_test(const bwfs_bitmap_t *bm, uint32_t blk) {
    return bm->map[_BWFS_BM_INDEX(blk)] & _BWFS_BM_MASK(blk);
}

/** Marca un bloque como ocupado (val!=0) o libre (val==0). */
static inline void bwfs_bm_set(bwfs_bitmap_t *bm, uint32_t blk, int val) {
    if (val)
        bm->map[_BWFS_BM_INDEX(blk)] |=  _BWFS_BM_MASK(blk);
    else
        bm->map[_BWFS_BM_INDEX(blk)] &= ~_BWFS_BM_MASK(blk);
}

#endif /* BWFS_BITMAP_H */
