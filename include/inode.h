#ifndef BWFS_INODE_H
#define BWFS_INODE_H
/**
 * \file inode.h
 * \brief Creación y gestión de inodos.
 */

#include <stdbool.h>
#include "bwfs_common.h"
#include "bitmap.h"

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

/**
 * \brief Crea un nuevo inodo (archivo o directorio).
 *
 * Asigna un bloque para el propio inodo y lo inicializa en disco.
 *
 * @param bm      Bitmap cargado en RAM (será actualizado).
 * @param is_dir  true → directorio, false → archivo.
 * @return        Número de inodo o UINT32_MAX en error.
 */
uint32_t bwfs_create_inode(bwfs_bitmap_t *bm, bool is_dir, const char *fs_dir);

/**
 * \brief Persiste un inodo ya inicializado.
 *
 * @param inode   Puntero a la copia in-memory.
 * @param fs_dir  Directorio del FS.
 * @retval BWFS_OK      OK
 * @retval BWFS_ERR_IO  Falla de I/O
 */
int bwfs_write_inode(const bwfs_inode_t *inode, const char *fs_dir);

/**
 * \brief Carga un inodo desde disco.
 *
 * @param ino     Número de inodo a leer.
 * @param inode   Destino (struct ya reservado).
 * @param fs_dir  Directorio del FS.
 * @return        BWFS_OK o BWFS_ERR_IO
 */
int bwfs_read_inode(uint32_t ino, bwfs_inode_t *inode, const char *fs_dir);

/**
 * \brief Ajusta el tamaño de un archivo, asignando o liberando bloques.
 *
 * Solo usa bloques directos.  Para valores grandes devolverá BWFS_ERR_FULL.
 *
 * @param bm        Bitmap (actualizado).
 * @param inode     Inodo (actualizado y re-escrito).
 * @param new_size  Tamaño objetivo en bytes.
 * @param fs_dir    Directorio del FS.
 * @return          BWFS_OK o código BWFS_ERR_*
 */
int bwfs_inode_resize(bwfs_bitmap_t *bm,
                      bwfs_inode_t   *inode,
                      uint32_t        new_size,
                      const char     *fs_dir);

#endif /* BWFS_INODE_H */
