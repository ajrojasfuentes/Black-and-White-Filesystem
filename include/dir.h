#ifndef BWFS_DIR_H
#define BWFS_DIR_H
/**
 * \file dir.h
 * \brief Manipulación de directorios (niveles arbitrarios).
 */

#include "bwfs_common.h"
#include "inode.h"
#include "bitmap.h"

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

/**
 * \brief Inserta una nueva entrada en un directorio.
 *
 * Si el directorio aún no posee bloques de datos se reserva el primero.
 *
 * @param bm         Bitmap (puede ser NULL si `block_count>0`).
 * @param dir_inode  Inodo del directorio (en memoria; será re-escrito).
 * @param fs_dir     Directorio del FS.
 * @param name       Nombre UTF-8 sin «/».
 * @param child_ino  Inodo que se enlazará.
 * @return           BWFS_OK o código BWFS_ERR_*
 */
int bwfs_dir_add(bwfs_bitmap_t *bm,
                 bwfs_inode_t  *dir_inode,
                 const char    *fs_dir,
                 const char    *name,
                 uint32_t       child_ino);

/**
 * \brief Elimina una entrada por nombre.
 *
 * @return BWFS_OK si la entrada existía, BWFS_ERR_IO/-1 en error,
 *         UINT32_MAX si no se halló.
 */
int bwfs_dir_remove(bwfs_inode_t *dir_inode,
                    const char   *fs_dir,
                    const char   *name);

/**
 * \brief Busca un nombre dentro de un directorio.
 *
 * @param dir_inode  Inodo del directorio.
 * @param fs_dir     Directorio del FS.
 * @param name       Nombre a buscar.
 * @return           Inodo encontrado o UINT32_MAX.
 */
uint32_t bwfs_dir_lookup(const bwfs_inode_t *dir_inode,
                         const char         *fs_dir,
                         const char         *name);

#endif /* BWFS_DIR_H */
