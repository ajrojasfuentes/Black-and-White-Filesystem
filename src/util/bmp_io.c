// -----------------------------------------------------------------------------
// File: src/util/bmp_io.c
// -----------------------------------------------------------------------------
/**
 * \file bmp_io.c
 * \brief I/O de bloques usando archivos binarios simples con extensión .bmp
 *
 * FILOSOFÍA:
 *   - Mantiene el concepto de "imágenes" (archivos .bmp)
 *   - Pero usa almacenamiento binario directo sin conversión
 *   - Simple, confiable y eficiente
 *   - Cada bloque = archivo block<N>.bmp de exactamente 125,000 bytes
 *
 * MAPEO:
 *   - Cada bloque BWFS = 125,000 bytes directos
 *   - Sin conversión bits ↔ pixels
 *   - Archivos .bmp contienen datos binarios raw
 */

#include "util.h"
#include "bwfs_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>   /* ← define off_t */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ------------------------------------------------------------------------- */
/* Helpers internos                                                          */
/* ------------------------------------------------------------------------- */

/**
 * \brief Construye la ruta del archivo BMP para un bloque dado.
 */
static void make_bmp_path(char *out, size_t sz, const char *dir, uint32_t blk)
{
    snprintf(out, sz, "%s/block%u.bmp", dir, blk);
}

/**
 * \brief Verifica que un archivo tenga el tamaño esperado.
 */
static int verify_file_size(const char *path, size_t expected_size)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;  // Archivo no existe
    }
    return (st.st_size == (off_t)expected_size) ? 0 : -1;
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

int util_create_empty_block(const char *fs_dir, uint32_t block_id)
{
    char path[PATH_MAX];
    make_bmp_path(path, sizeof path, fs_dir, block_id);

    FILE *f = fopen(path, "wb");
    if (!f) {
        BWFS_LOG_ERROR("Cannot create file %s", path);
        return -1;
    }

    // Escribir exactamente BWFS_BLOCK_SIZE_BYTES de ceros
    uint8_t zero_buffer[4096];  // Buffer de 4KB para eficiencia
    memset(zero_buffer, 0, sizeof(zero_buffer));
    
    size_t remaining = BWFS_BLOCK_SIZE_BYTES;
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(zero_buffer)) ? sizeof(zero_buffer) : remaining;
        size_t written = fwrite(zero_buffer, 1, chunk, f);
        
        if (written != chunk) {
            BWFS_LOG_ERROR("Write failed for %s", path);
            fclose(f);
            return -1;
        }
        remaining -= written;
    }

    fclose(f);

    // Verificar que el archivo tiene el tamaño correcto
    if (verify_file_size(path, BWFS_BLOCK_SIZE_BYTES) != 0) {
        BWFS_LOG_ERROR("Created file %s has wrong size", path);
        return -1;
    }

    return 0;
}

int util_write_block(const char *fs_dir, uint32_t block_id,
                     const uint8_t *data, size_t len)
{
    if (len > BWFS_BLOCK_SIZE_BYTES) {
        BWFS_LOG_ERROR("Data too large: %zu > %u bytes", len, BWFS_BLOCK_SIZE_BYTES);
        return -1;
    }

    char path[PATH_MAX];
    make_bmp_path(path, sizeof path, fs_dir, block_id);

    FILE *f = fopen(path, "wb");
    if (!f) {
        BWFS_LOG_ERROR("Cannot open %s for writing", path);
        return -1;
    }

    // Escribir los datos proporcionados
    if (len > 0) {
        size_t written = fwrite(data, 1, len, f);
        if (written != len) {
            BWFS_LOG_ERROR("Failed to write data to %s", path);
            fclose(f);
            return -1;
        }
    }

    // Rellenar el resto con ceros hasta BWFS_BLOCK_SIZE_BYTES
    size_t padding = BWFS_BLOCK_SIZE_BYTES - len;
    if (padding > 0) {
        uint8_t zero_buffer[4096];
        memset(zero_buffer, 0, sizeof(zero_buffer));
        
        while (padding > 0) {
            size_t chunk = (padding > sizeof(zero_buffer)) ? sizeof(zero_buffer) : padding;
            size_t written = fwrite(zero_buffer, 1, chunk, f);
            
            if (written != chunk) {
                BWFS_LOG_ERROR("Failed to write padding to %s", path);
                fclose(f);
                return -1;
            }
            padding -= written;
        }
    }

    fclose(f);

    // Verificar tamaño final
    if (verify_file_size(path, BWFS_BLOCK_SIZE_BYTES) != 0) {
        BWFS_LOG_ERROR("Written file %s has wrong size", path);
        return -1;
    }

    return 0;
}

int util_read_block(const char *fs_dir, uint32_t block_id,
                    uint8_t *out, size_t len)
{
    if (len > BWFS_BLOCK_SIZE_BYTES) {
        BWFS_LOG_ERROR("Request too large: %zu > %u bytes", len, BWFS_BLOCK_SIZE_BYTES);
        return -1;
    }

    char path[PATH_MAX];
    make_bmp_path(path, sizeof path, fs_dir, block_id);

    // Verificar que el archivo existe y tiene el tamaño correcto
    if (verify_file_size(path, BWFS_BLOCK_SIZE_BYTES) != 0) {
        BWFS_LOG_ERROR("File %s does not exist or has wrong size", path);
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        BWFS_LOG_ERROR("Cannot open %s for reading", path);
        return -1;
    }

    // Leer exactamente len bytes desde el inicio del archivo
    size_t read_bytes = fread(out, 1, len, f);
    fclose(f);

    if (read_bytes != len) {
        BWFS_LOG_ERROR("Failed to read %zu bytes from %s (got %zu)", len, path, read_bytes);
        return -1;
    }

    return 0;
}