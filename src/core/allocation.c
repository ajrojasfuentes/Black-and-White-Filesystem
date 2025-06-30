// -----------------------------------------------------------------------------
// File: src/core/allocation.c
// -----------------------------------------------------------------------------
/**
 * \file allocation.c
 * \brief Reservas y liberaciones de bloques usando estrategia *Worst-Fit*.
 *
 * El algoritmo recorre el bitmap para localizar la región libre contigua más
 * grande (`best_len`).  Si existen varias del mismo tamaño se queda con la
 * primera encontrada.  Devuelve el índice del bloque inicial o UINT32_MAX si
 * no hay hueco suficiente.
 */

#include "allocation.h"
#include <limits.h>   /* UINT32_MAX */
#include "bitmap.h"

/* ------------------------------------------------------------------------- */
/* Funciones internas auxiliares                                             */
/* ------------------------------------------------------------------------- */

/**
 * \brief Encuentra la región contigua libre más larga.
 *
 * \param bm        Bitmap.
 * \param min_len   Longitud mínima deseada.
 * \param[out] out_start  Primer bloque de la región encontrada.
 * \param[out] out_len    Longitud de la región encontrada.
 */
static void find_worst_fit(const bwfs_bitmap_t *bm,
                           uint32_t             min_len,
                           uint32_t            *out_start,
                           uint32_t            *out_len)
{
    uint32_t best_start = UINT32_MAX, best_len = 0;
    uint32_t cur_start  = 0,          cur_len  = 0;

    for (uint32_t i = 0; i < bm->total_blocks; ++i) {
        if (!bwfs_bm_test(bm, i)) {              /* bloque libre */
            if (cur_len == 0) cur_start = i;
            ++cur_len;
        } else {                                 /* bloque ocupado */
            if (cur_len >= min_len && cur_len > best_len) {
                best_start = cur_start;
                best_len   = cur_len;
            }
            cur_len = 0;
        }
    }

    /* Comprobar región al final del bitmap */
    if (cur_len >= min_len && cur_len > best_len) {
        best_start = cur_start;
        best_len   = cur_len;
    }

    *out_start = best_start;
    *out_len   = best_len;
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

uint32_t bwfs_alloc_blocks(bwfs_bitmap_t *bm, uint32_t count)
{
    uint32_t start, len;
    find_worst_fit(bm, count, &start, &len);

    if (start == UINT32_MAX)  /* sin hueco grande suficiente */
        return UINT32_MAX;

    for (uint32_t i = 0; i < count; ++i)
        bwfs_bm_set(bm, start + i, 1);

    return start;
}

void bwfs_free_blocks(bwfs_bitmap_t *bm, uint32_t start, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        bwfs_bm_set(bm, start + i, 0);
}
