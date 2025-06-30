// -----------------------------------------------------------------------------
// File: src/cli/mount_bwfs.c
// -----------------------------------------------------------------------------
/**
 * \file mount_bwfs.c
 * \brief Punto de entrada para montar BWFS con FUSE 3.
 *
 * Ejemplo:
 *     mount_bwfs <directorio_FS> <punto_montaje> -f -o allow_other
 */

#define _GNU_SOURCE     /* Para realpath() y otras funciones GNU */
#include <limits.h>     /* Para PATH_MAX */
#include <time.h>       /* Para struct timespec - DEBE IR ANTES DE FUSE */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>

extern struct fuse_operations bwfs_ops; /* declarado en src/fuse/bwfs.c */
const char *fs_dir;                      /* visible en bwfs.c via 'extern' */

/* Si por alguna razón PATH_MAX no vino del encabezado anterior */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern struct fuse_operations bwfs_ops; /* declarado en src/fuse/bwfs.c */
const char *fs_dir;                      /* visible en bwfs.c via 'extern' */

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
            "Uso: %s <directorio_FS> <punto_montaje> [opciones FUSE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Usar buffer estático para evitar problemas con realpath */
    static char fs_dir_buf[PATH_MAX];
    if (realpath(argv[1], fs_dir_buf) == NULL) {
        perror("realpath");
        return EXIT_FAILURE;
    }
    fs_dir = fs_dir_buf;

    /* Desplazar argumentos para FUSE:  argv[2…] son los suyos */
    return fuse_main(argc - 1, &argv[1], &bwfs_ops, NULL);
}