// -----------------------------------------------------------------------------
// File: src/core/bwfs_common.c
// -----------------------------------------------------------------------------
/**
 * \file bwfs_common.c
 * \brief Rutinas de creación, lectura y escritura del superbloque del
 *        Black & White Filesystem (BWFS).
 *
 * El superbloque es la “cabecera” del disco: contiene la cuenta total de
 * bloques, la ubicación del inodo raíz y banderas globales.  Reside
 * *siempre* en el **bloque lógico 0** (véase BWFS_SUPERBLOCK_BLK).
 *
 * Todas las operaciones de E/S delegan en los helpers genéricos declarados en
 * `util.h` (`util_write_block`, `util_read_block`), los cuales tratan cada
 * bloque como un archivo-PNG contenedor de 1000 × 1000 bits en blanco y negro.
 */

#include "bwfs_common.h"
#include "util.h"

#include <string.h>   /* memset */
#include <stdio.h>    /* NULL */

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

/**
 * \brief Rellena en memoria un \ref bwfs_superblock_t con valores por defecto.
 *
 * Este paso NO escribe nada a disco; únicamente prepara la estructura que
 * luego se pasará a \ref bwfs_write_superblock().
 *
 * \param[out] sb           Puntero a la estructura a inicializar.
 * \param[in]  total_blocks Cantidad de bloques lógicos del nuevo disco.
 */
void bwfs_init_superblock(bwfs_superblock_t *sb, uint32_t total_blocks)
{
    memset(sb, 0, sizeof *sb);

    sb->magic        = BWFS_MAGIC;
    sb->total_blocks = total_blocks;
    sb->root_inode   = 0;                    /* se fijará tras crear el raíz   */
    sb->block_size   = BWFS_BLOCK_SIZE_BITS; /* constante del formato          */
    sb->flags        = 0;                    /* sin cifrado ni resize por def. */
}

/**
 * \brief Escribe el superbloque en el bloque 0 del disco.
 *
 * \param[in] sb     Superb loque ya inicializado / actualizado.
 * \param[in] fs_dir Directorio que contiene los archivos-bloque (*.png).
 * \retval BWFS_OK       Éxito.
 * \retval BWFS_ERR_IO   Fallo de E/S (no se pudo crear o escribir block0.png).
 */
int bwfs_write_superblock(const bwfs_superblock_t *sb, const char *fs_dir)
{
    if (util_write_block(fs_dir,
                         BWFS_SUPERBLOCK_BLK,
                         (const uint8_t *)sb,
                         sizeof *sb) != 0)
    {
        return BWFS_ERR_IO;
    }

    BWFS_LOG_INFO("Superbloque escrito (total_blocks=%u, root_inode=%u)",
                  sb->total_blocks, sb->root_inode);
    return BWFS_OK;
}

/**
 * \brief Carga el superbloque desde disco y valida su consistencia.
 *
 * Se comprueban:
 *  - Número mágico (\c BWFS_MAGIC).
 *  - Tamaño de bloque (\c BWFS_BLOCK_SIZE_BITS).
 *
 * \param[out] sb     Estructura en la que se colocarán los datos leídos.
 * \param[in]  fs_dir Directorio del sistema de archivos.
 * \retval BWFS_OK        Lectura satisfactoria y validación exitosa.
 * \retval BWFS_ERR_IO    Fallo de lectura del bloque 0.
 * \retval BWFS_ERR_FULL  Disco no es BWFS o el tamaño de bloque no coincide.
 */
int bwfs_read_superblock(bwfs_superblock_t *sb, const char *fs_dir)
{
    if (util_read_block(fs_dir,
                        BWFS_SUPERBLOCK_BLK,
                        (uint8_t *)sb,
                        sizeof *sb) != 0)
    {
        return BWFS_ERR_IO;
    }

    if (sb->magic != BWFS_MAGIC ||
        sb->block_size != BWFS_BLOCK_SIZE_BITS)
    {
        BWFS_LOG_ERROR("Superbloque inválido: magic=0x%08x, block_size=%u",
                       sb->magic, sb->block_size);
        return BWFS_ERR_FULL;
    }

    return BWFS_OK;
}
