// -----------------------------------------------------------------------------
// File: src/util/png_io.c
// -----------------------------------------------------------------------------
/**
 * \file png_io.c  
 * \brief I/O de bloques usando imágenes PNG reales de 1000x1000 pixels.
 *
 * MAPEO:
 *   - Cada bloque BWFS = 1000×1000 bits = 125,000 bytes
 *   - Cada imagen PNG = 1000×1000 pixels (escala de grises, 1 byte/pixel)
 *   - Conversión: 0 bit → pixel negro (0), 1 bit → pixel blanco (255)
 *
 * DEPENDENCIAS:
 *   - stb_image.h y stb_image_write.h (single-header libraries)
 *   - Descargar de: https://github.com/nothings/stb
 */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image.h"
#include "external/stb_image_write.h"

#include "util.h"
#include "bwfs_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ------------------------------------------------------------------------- */
/* Helpers internos                                                          */
/* ------------------------------------------------------------------------- */

/**
 * \brief Construye la ruta del archivo PNG para un bloque dado.
 */
static void make_png_path(char *out, size_t sz, const char *dir, uint32_t blk)
{
    snprintf(out, sz, "%s/block%u.png", dir, blk);
}

/**
 * \brief Convierte datos binarios (bits) a pixels de escala de grises.
 * 
 * @param bits_data  Buffer de entrada (125,000 bytes = 1,000,000 bits)
 * @param pixels     Buffer de salida (1,000,000 bytes = 1000×1000 pixels)
 */
static void bits_to_pixels(const uint8_t *bits_data, uint8_t *pixels)
{
    for (size_t byte_idx = 0; byte_idx < BWFS_BLOCK_SIZE_BYTES; ++byte_idx) {
        uint8_t byte_val = bits_data[byte_idx];
        
        // Cada byte contiene 8 bits → 8 pixels
        for (int bit_pos = 0; bit_pos < 8; ++bit_pos) {
            size_t pixel_idx = byte_idx * 8 + bit_pos;
            
            // Extraer bit (MSB primero: bit 7, 6, 5... 0)
            int bit = (byte_val >> (7 - bit_pos)) & 1;
            
            // Mapear: 0→negro(0), 1→blanco(255)
            pixels[pixel_idx] = bit ? 255 : 0;
        }
    }
}

/**
 * \brief Convierte pixels de escala de grises a datos binarios (bits).
 * 
 * @param pixels     Buffer de entrada (1,000,000 pixels)
 * @param bits_data  Buffer de salida (125,000 bytes)
 */
static void pixels_to_bits(const uint8_t *pixels, uint8_t *bits_data)
{
    memset(bits_data, 0, BWFS_BLOCK_SIZE_BYTES);  // Inicializar a cero
    
    for (size_t byte_idx = 0; byte_idx < BWFS_BLOCK_SIZE_BYTES; ++byte_idx) {
        uint8_t byte_val = 0;
        
        // Procesar 8 pixels → 1 byte
        for (int bit_pos = 0; bit_pos < 8; ++bit_pos) {
            size_t pixel_idx = byte_idx * 8 + bit_pos;
            
            // Umbral: >127 = blanco (bit 1), ≤127 = negro (bit 0)
            if (pixels[pixel_idx] > 127) {
                byte_val |= (1 << (7 - bit_pos));  // Establecer bit
            }
        }
        
        bits_data[byte_idx] = byte_val;
    }
}

/* ------------------------------------------------------------------------- */
/* API pública                                                               */
/* ------------------------------------------------------------------------- */

int util_create_empty_block(const char *fs_dir, uint32_t block_id)
{
    char path[PATH_MAX];
    make_png_path(path, sizeof path, fs_dir, block_id);

    // Crear imagen completamente negra (todos los bits a 0)
    uint8_t *pixels = (uint8_t *)calloc(BWFS_BLOCK_PX * BWFS_BLOCK_PX, 1);
    if (!pixels) {
        BWFS_LOG_ERROR("calloc failed para block %u", block_id);
        return -1;
    }

    // Escribir PNG en escala de grises
    int result = stbi_write_png(path, 
                               BWFS_BLOCK_PX, BWFS_BLOCK_PX, 
                               1,  // 1 canal (escala de grises)
                               pixels, 
                               BWFS_BLOCK_PX);  // stride
    
    free(pixels);
    
    if (!result) {
        BWFS_LOG_ERROR("stbi_write_png failed: %s", path);
        return -1;
    }
    
    return 0;
}

int util_write_block(const char *fs_dir, uint32_t block_id,
                     const uint8_t *data, size_t len)
{
    if (len > BWFS_BLOCK_SIZE_BYTES) {
        BWFS_LOG_ERROR("Datos demasiado grandes: %zu > %u bytes", 
                       len, BWFS_BLOCK_SIZE_BYTES);
        return -1;
    }

    char path[PATH_MAX];
    make_png_path(path, sizeof path, fs_dir, block_id);

    // Buffer temporal para los datos completos del bloque
    uint8_t *full_data = (uint8_t *)calloc(BWFS_BLOCK_SIZE_BYTES, 1);
    if (!full_data) {
        BWFS_LOG_ERROR("calloc failed for block %u", block_id);
        return -1;
    }

    // Copiar datos de entrada (rellenar con ceros si len < BWFS_BLOCK_SIZE_BYTES)
    memcpy(full_data, data, len);

    // Convertir bits → pixels
    uint8_t *pixels = (uint8_t *)malloc(BWFS_BLOCK_PX * BWFS_BLOCK_PX);
    if (!pixels) {
        BWFS_LOG_ERROR("malloc failed for pixels");
        free(full_data);
        return -1;
    }

    bits_to_pixels(full_data, pixels);

    // Escribir PNG
    int result = stbi_write_png(path, 
                               BWFS_BLOCK_PX, BWFS_BLOCK_PX,
                               1,  // escala de grises
                               pixels,
                               BWFS_BLOCK_PX);

    free(full_data);
    free(pixels);

    if (!result) {
        BWFS_LOG_ERROR("stbi_write_png failed: %s", path);
        return -1;
    }

    return 0;
}

int util_read_block(const char *fs_dir, uint32_t block_id,
                    uint8_t *out, size_t len)
{
    if (len > BWFS_BLOCK_SIZE_BYTES) {
        BWFS_LOG_ERROR("Buffer demasiado pequeño: %zu bytes solicitados", len);
        return -1;
    }

    char path[PATH_MAX];
    make_png_path(path, sizeof path, fs_dir, block_id);

    // Cargar PNG
    int width, height, channels;
    uint8_t *pixels = stbi_load(path, &width, &height, &channels, 1);  // Forzar escala de grises
    
    if (!pixels) {
        BWFS_LOG_ERROR("stbi_load failed: %s", path);
        return -1;
    }

    // Validar dimensiones
    if (width != BWFS_BLOCK_PX || height != BWFS_BLOCK_PX) {
        BWFS_LOG_ERROR("Dimensiones incorrectas en %s: %dx%d (esperado %dx%d)",
                       path, width, height, BWFS_BLOCK_PX, BWFS_BLOCK_PX);
        stbi_image_free(pixels);
        return -1;
    }

    // Convertir pixels → bits
    uint8_t *bits_data = (uint8_t *)malloc(BWFS_BLOCK_SIZE_BYTES);
    if (!bits_data) {
        BWFS_LOG_ERROR("malloc failed for bits_data");
        stbi_image_free(pixels);
        return -1;
    }

    pixels_to_bits(pixels, bits_data);

    // Copiar al buffer de salida (solo lo que se pidió)
    memcpy(out, bits_data, len);

    // Limpiar
    stbi_image_free(pixels);
    free(bits_data);

    return 0;
}