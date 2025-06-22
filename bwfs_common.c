/*
 * bwfs_common.c - Implementación de funciones comunes para BWFS
 * 
 * Este archivo contiene las implementaciones de las funciones
 * compartidas entre todos los componentes del BWFS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "bwfs_common.h"

// ==================== FUNCIONES BMP ====================

static void create_bmp_file(const char *filename, int width, int height, const uint8_t *data) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Error creando archivo BMP");
        return;
    }
    
    // Calcular padding para cada fila (debe ser múltiplo de 4 bytes)
    int row_size = (width + 7) / 8;  // Bytes necesarios para los píxeles
    int padding = (4 - (row_size % 4)) % 4;
    int padded_row_size = row_size + padding;
    
    // Headers BMP
    BMPHeader header = {
        .type = 0x4D42,  // "BM"
        .size = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + 2 * sizeof(RGBQuad) + padded_row_size * height,
        .reserved1 = 0,
        .reserved2 = 0,
        .offset = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + 2 * sizeof(RGBQuad)
    };
    
    BMPInfoHeader info = {
        .size = sizeof(BMPInfoHeader),
        .width = width,
        .height = height,
        .planes = 1,
        .bits_per_pixel = 1,
        .compression = 0,
        .image_size = padded_row_size * height,
        .x_pixels_per_m = 2835,  // 72 DPI
        .y_pixels_per_m = 2835,
        .colors_used = 2,
        .important_colors = 2
    };
    
    // Paleta de colores (blanco y negro)
    RGBQuad palette[2] = {
        {0, 0, 0, 0},        // Negro (0)
        {255, 255, 255, 0}   // Blanco (1)
    };
    
    // Escribir headers
    fwrite(&header, sizeof(header), 1, fp);
    fwrite(&info, sizeof(info), 1, fp);
    fwrite(palette, sizeof(palette), 1, fp);
    
    // Escribir datos de píxeles (con padding)
    uint8_t padding_bytes[4] = {0};
    for (int y = 0; y < height; y++) {
        fwrite(data + y * row_size, row_size, 1, fp);
        if (padding > 0) {
            fwrite(padding_bytes, padding, 1, fp);
        }
    }
    
    fclose(fp);
}

// ==================== FUNCIONES DE BLOQUES ====================

void write_block(uint32_t block_num, const uint8_t *data, const char *fs_path) {
    char filename[MAX_PATH];
    snprintf(filename, MAX_PATH, "%s/block_%06u.bmp", fs_path, block_num);
    create_bmp_file(filename, BLOCK_SIZE, BLOCK_SIZE, data);
}

void read_block(uint32_t block_num, uint8_t *data, const char *fs_path) {
    char filename[MAX_PATH];
    snprintf(filename, MAX_PATH, "%s/block_%06u.bmp", fs_path, block_num);
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        memset(data, 0, BYTES_PER_BLOCK);
        return;
    }
    
    // Saltar headers BMP
    fseek(fp, sizeof(BMPHeader) + sizeof(BMPInfoHeader) + 2 * sizeof(RGBQuad), SEEK_SET);
    
    // Leer datos considerando padding
    int row_size = BLOCK_SIZE / 8;
    int padding = (4 - (row_size % 4)) % 4;
    
    for (int y = 0; y < BLOCK_SIZE; y++) {
        fread(data + y * row_size, row_size, 1, fp);
        if (padding > 0) {
            fseek(fp, padding, SEEK_CUR);
        }
    }
    
    fclose(fp);
}

// ==================== FUNCIONES DE BITMAP ====================

void set_bit(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

void clear_bit(uint8_t *bitmap, uint32_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

int get_bit(const uint8_t *bitmap, uint32_t bit) {
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}

uint32_t find_free_bit(const uint8_t *bitmap, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (!get_bit(bitmap, i)) {
            return i;
        }
    }
    return (uint32_t)-1;
}

// ==================== FUNCIONES DE CIFRADO ====================

void derive_key(const char *pass, const uint8_t *salt, uint8_t *key) {
    PKCS5_PBKDF2_HMAC(pass, strlen(pass), salt, SALT_SIZE, 10000, EVP_sha256(), KEY_SIZE, key);
}

void encrypt_decrypt(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, int encrypt) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return;
    
    int outlen;
    
    if (encrypt) {
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        EVP_EncryptUpdate(ctx, data, &outlen, data, len);
        EVP_EncryptFinal_ex(ctx, data + outlen, &outlen);
    } else {
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        EVP_DecryptUpdate(ctx, data, &outlen, data, len);
        EVP_DecryptFinal_ex(ctx, data + outlen, &outlen);
    }
    
    EVP_CIPHER_CTX_free(ctx);
}

// ==================== FUNCIONES DE CONVERSIÓN ====================

uint32_t get_file_type_from_mode(uint32_t mode) {
    switch (mode & BWFS_S_IFMT) {
        case BWFS_S_IFREG: return BWFS_FILE_TYPE_REG;
        case BWFS_S_IFDIR: return BWFS_FILE_TYPE_DIR;
        case BWFS_S_IFLNK: return BWFS_FILE_TYPE_LINK;
        default: return BWFS_FILE_TYPE_UNKNOWN;
    }
}

uint8_t get_dir_entry_type_from_mode(uint32_t mode) {
    return get_file_type_from_mode(mode);
}

// ==================== FUNCIONES DE DEBUGGING ====================

void print_superblock(const Superblock *sb) {
    printf("\n=== SUPERBLOCK ===\n");
    printf("Magic: 0x%08X (%s)\n", sb->magic, sb->magic == BWFS_MAGIC ? "VÁLIDO" : "INVÁLIDO");
    printf("Versión: %u\n", sb->version);
    printf("Tamaño de bloque: %u píxeles\n", sb->block_size);
    printf("Bloques totales: %u\n", sb->total_blocks);
    printf("Bloques libres: %u\n", sb->free_blocks);
    printf("Inodos totales: %u\n", sb->total_inodes);
    printf("Inodos libres: %u\n", sb->free_inodes);
    printf("Primer bloque de datos: %u\n", sb->first_data_block);
    printf("Bloques de tabla de inodos: %u\n", sb->inode_table_blocks);
    printf("Bloques de bitmap: %u\n", sb->bitmap_blocks);
    printf("Inodo raíz: %u\n", sb->root_inode);
    printf("Cifrado: %s\n", sb->encrypted ? "Sí" : "No");
    printf("Tiempo de creación: %s\n", sb->mount_time);
    printf("Último montaje: %s\n", sb->last_mount);
    printf("==================\n\n");
}

void print_inode(const Inode *inode) {
    printf("\n=== INODO %u ===\n", inode->inode_number);
    printf("Modo: %o", inode->mode & 0777);
    
    if (inode->mode & BWFS_S_IFDIR) printf(" (directorio)");
    else if (inode->mode & BWFS_S_IFREG) printf(" (archivo regular)");
    else if (inode->mode & BWFS_S_IFLNK) printf(" (enlace simbólico)");
    
    printf("\nUID: %u, GID: %u\n", inode->uid, inode->gid);
    printf("Tamaño: %lu bytes\n", inode->size);
    printf("Enlaces: %u\n", inode->link_count);
    
    printf("Bloques directos: ");
    int blocks_used = 0;
    for (int i = 0; i < DIRECT_BLOCKS; i++) {
        if (inode->blocks[i] != 0) {
            printf("%u ", inode->blocks[i]);
            blocks_used++;
        }
    }
    if (blocks_used == 0) printf("(ninguno)");
    
    printf("\nBloque indirecto: %u\n", inode->indirect_block);
    printf("Bloque doble indirecto: %u\n", inode->double_indirect);
    printf("Bloque triple indirecto: %u\n", inode->triple_indirect);
    printf("================\n\n");
}

void print_directory_entry(const DirectoryEntry *entry) {
    printf("Inodo: %u, Tipo: %u, Nombre: '%.*s' (len: %u)\n", 
           entry->inode, 
           entry->file_type,
           entry->name_len,
           entry->name,
           entry->name_len);
}