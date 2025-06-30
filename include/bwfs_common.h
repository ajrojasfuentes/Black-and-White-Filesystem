#ifndef BWFS_COMMON_H
#define BWFS_COMMON_H
/**
 * \file bwfs_common.h
 * \brief Tipos y constantes públicas del *Black & White Filesystem (BWFS)*.
 *
 * Este archivo **define el formato on-disk**.  Cualquier cambio aquí rompe la
 * compatibilidad con discos ya formateados; recuerda incrementar el campo
 * `magic` del superbloque si modificas algo.
 */

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* Parámetros globales                                                       */
/* ------------------------------------------------------------------------- */

/** Secuencia ASCII «B W F S» → 0x42 0x57 0x46 0x53. */
#define BWFS_MAGIC              0x42465753U

/** Dimensión (px) del bloque-imagen lógico: 1000 × 1000 PX en blanco/negro. */
#define BWFS_BLOCK_PX           1000U

/** Tamaño lógico de un bloque en bits y bytes. */
#define BWFS_BLOCK_SIZE_BITS    (BWFS_BLOCK_PX * BWFS_BLOCK_PX)
#define BWFS_BLOCK_SIZE_BYTES   (BWFS_BLOCK_SIZE_BITS / 8)

/** Bloques reservados para metadatos. */
#define BWFS_SUPERBLOCK_BLK     0U  /**< Superbloque                        */
#define BWFS_BITMAP_BLK         1U  /**< Mapa de bits de bloques            */

/* ------------------------------------------------------------------------- */
/* Superbloque                                                               */
/* ------------------------------------------------------------------------- */

/** Flags del superbloque (`flags`). */
enum {
    BWFS_SB_ENCRYPTED  = 0x01,  /**< Metadatos cifrados con passphrase   */
    BWFS_SB_RESIZABLE  = 0x02,  /**< El disco admite *resize* dinámico   */
};

/**
 * \struct bwfs_superblock_t
 * \brief Metadatos globales (reside en el bloque 0).
 *
 * Está marcado *packed* para que su representación *in-memory* coincida con la
 * serialización en el bloque 0 **sin relleno interno**.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;          /**< Valor fijo #BWFS_MAGIC                 */
    uint32_t total_blocks;   /**< Cantidad total de bloques lógicos      */
    uint32_t root_inode;     /**< Número de inodo del directorio raíz    */
    uint32_t block_size;     /**< Copia de #BWFS_BLOCK_SIZE_BITS         */
    uint32_t flags;          /**< Véase enum BWFS_SB_*                   */
    uint32_t reserved[11];   /**< Futuras extensiones (deja a 0)         */
} bwfs_superblock_t;

/**
 * \brief Inicializa un superbloque con valores por defecto.
 */
void bwfs_init_superblock(bwfs_superblock_t *sb, uint32_t total_blocks);

/**
 * \brief Escribe el superbloque al disco.
 */
int bwfs_write_superblock(const bwfs_superblock_t *sb, const char *fs_dir);

/**
 * \brief Lee y valida el superbloque desde disco.
 */
int bwfs_read_superblock(bwfs_superblock_t *sb, const char *fs_dir);

/* ------------------------------------------------------------------------- */
/* Inodos                                                                    */
/* ------------------------------------------------------------------------- */

/** Bloques directos por inodo (mantén potencia de 2 para facilitar resize). */
#define BWFS_DIRECT_BLOCKS      10U

/** El bit 0 del campo `flags` indica que el inodo es un directorio. */
#define BWFS_INODE_DIR          0x01U

/**
 * \struct bwfs_inode_t  (128 bytes)
 * \brief Inodo de tamaño fijo — lectura/escritura directa de disco.
 *
 * Para la versión mínima se usan **solo los bloques directos**; `indirect`
 * está reservado para crecer en el futuro.  Si `size` > `BWFS_DIRECT_BLOCKS`
 * ⋅ `BWFS_BLOCK_SIZE_BYTES` deberás gestionar la indirección.
 */
typedef struct __attribute__((packed)) {
    /* Campo  Offset  Tamaño */
    uint32_t ino;                            /**<  0   4  — Nº de inodo   */
    uint32_t size;                           /**<  4   4  — Bytes reales  */
    uint32_t block_count;                    /**<  8   4  — Bloques usados*/
    uint8_t  flags;                          /**< 12   1  — BWFS_INODE_*  */
    uint8_t  _pad[3];                        /**< 13   3  — Alineación    */
    uint32_t blocks[BWFS_DIRECT_BLOCKS];     /**< 16  40  — Directos      */
    uint32_t indirect;                       /**< 56   4  — Indirecto 1°  */
    uint32_t reserved[14];                   /**< 60  56  — Relleno       */
} bwfs_inode_t;

/* ------------------------------------------------------------------------- */
/* Directorios                                                               */
/* ------------------------------------------------------------------------- */

#define BWFS_NAME_MAX 255U  /**< Longitud máxima de nombre sin NUL. */

/**
 * \struct bwfs_dir_entry_t
 * \brief Asociación nombre → inodo (registro fijo).
 */
typedef struct __attribute__((packed)) {
    uint32_t ino;                    /**< Inodo destino               */
    char     name[BWFS_NAME_MAX + 1];/**< UTF-8 + NUL final           */
} bwfs_dir_entry_t;

/* ------------------------------------------------------------------------- */
/* Bitmap (solo en RAM)                                                      */
/* ------------------------------------------------------------------------- */

/**
 * \struct bwfs_bitmap_t
 * \brief Representación temporal del mapa de bits de bloques libres/ocupados.
 */
typedef struct {
    uint32_t bits_per_block;   /**< Constante: #BWFS_BLOCK_SIZE_BITS     */
    uint32_t total_blocks;     /**< Igual al valor del superbloque       */
    uint8_t *map;              /**< Buffer ⌈total_blocks/8⌉ bytes        */
} bwfs_bitmap_t;

/* ------------------------------------------------------------------------- */
/* Códigos de retorno genéricos                                              */
/* ------------------------------------------------------------------------- */

#define BWFS_OK         0   /**< Operación exitosa                     */
#define BWFS_ERR_IO    -5   /**< Error de entrada/salida               */
#define BWFS_ERR_NOMEM -6   /**< Sin memoria disponible                */
#define BWFS_ERR_FULL  -7   /**< No hay espacio libre                  */

#endif /* BWFS_COMMON_H */
