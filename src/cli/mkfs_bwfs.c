// -----------------------------------------------------------------------------
// File: src/cli/mkfs_bwfs.c
// -----------------------------------------------------------------------------
/**
 * \file mkfs_bwfs.c
 * \brief Formatea un directorio como Black & White Filesystem.
 *
 * Uso:
 *     mkfs_bwfs [-b <bloques>] <directorio_FS>
 *
 *  - Crea los archivos block<N>.bin (uno por bloque lógico).
 *  - Inicializa superbloque (bloque 0), bitmap (bloque 1) e inodo raíz.
 *  - El tamaño de cada bloque es BWFS_BLOCK_SIZE_BYTES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>     /* getopt(), realpath()            */
#include <getopt.h>     /* en algunos toolchains C++       */

#include "bwfs_common.h"
#include "bitmap.h"
#include "inode.h"
#include "util.h"

#define DEFAULT_BLOCKS 1024U   /* valor por defecto si no se usa -b */

/* ------------------------------------------------------------------------- */
/* Crea todos los archivos-bloque vacíos                                     */
/* ------------------------------------------------------------------------- */
static int create_all_blocks(const char *dir, uint32_t total)
{
    for (uint32_t i = 0; i < total; ++i)
        if (util_create_empty_block(dir, i) != 0)
            return -1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    uint32_t total_blocks = DEFAULT_BLOCKS;

    /* -------------------- Parsear argumentos --------------------------- */
    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        switch (opt) {
            case 'b':
                total_blocks = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            default:
                fprintf(stderr,
                    "Uso: %s [-b bloques] <directorio_FS>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
    if (optind != argc - 1) {
        fprintf(stderr, "Falta <directorio_FS>\n");
        return EXIT_FAILURE;
    }
    const char *fs_dir = argv[optind];

    /* -------------------- Crear directorio destino --------------------- */
    if (mkdir(fs_dir, 0755) && errno != EEXIST) {
        BWFS_LOG_ERROR("mkdir %s", fs_dir);
        return EXIT_FAILURE;
    }

    /* -------------------- Inicializar superbloque ---------------------- */
    bwfs_superblock_t sb;
    bwfs_init_superblock(&sb, total_blocks);

    /* -------------------- Preparar bitmap en RAM ----------------------- */
    bwfs_bitmap_t bm = {
        .bits_per_block = BWFS_BLOCK_SIZE_BITS,
        .total_blocks   = total_blocks
    };
    size_t bm_bytes = (total_blocks + 7) / 8;
    bm.map = calloc(1, bm_bytes);
    if (!bm.map) { perror("calloc"); return EXIT_FAILURE; }

    /* Reservar bloques 0 (super) y 1 (bitmap)                             */
    bwfs_bm_set(&bm, BWFS_SUPERBLOCK_BLK, 1);
    bwfs_bm_set(&bm, BWFS_BITMAP_BLK,     1);

    /* -------------------- Crear inodo raíz ----------------------------- */
    uint32_t root_blk = bwfs_create_inode(&bm, /*is_dir=*/true, fs_dir);
    if (root_blk == UINT32_MAX) {
        fprintf(stderr, "Error: sin espacio para inodo raíz\n");
        free(bm.map); return EXIT_FAILURE;
    }

    /* -------------------- Persistir superbloque y bitmap --------------- */
    sb.root_inode = root_blk;
    if (bwfs_write_superblock(&sb, fs_dir) != BWFS_OK ||
        bwfs_write_bitmap(&bm,    fs_dir) != BWFS_OK) {
        free(bm.map); return EXIT_FAILURE;
    }

    /* -------------------- Generar archivos-bloque vacíos --------------- */
    if (create_all_blocks(fs_dir, total_blocks) != 0) {
        fprintf(stderr, "Error creando bloques de datos\n");
        free(bm.map); return EXIT_FAILURE;
    }

    printf("BWFS formateado en \"%s\" con %u bloques (inodo raíz %u)\n",
           fs_dir, total_blocks, root_blk);

    free(bm.map);
    return EXIT_SUCCESS;
}
