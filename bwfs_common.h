/*
 * bwfs_common.h - Definiciones comunes para Black & White Filesystem
 * 
 * Este archivo contiene todas las estructuras, constantes y funciones
 * compartidas entre los diferentes componentes del BWFS.
 */

#ifndef BWFS_COMMON_H
#define BWFS_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// ==================== CONSTANTES DEL SISTEMA ====================
#define BWFS_MAGIC          0x42574653  // "BWFS" en hex
#define BLOCK_SIZE          1000        // Tamaño del bloque en píxeles (1000x1000)
#define BITS_PER_PIXEL      1          // 1 bit por píxel (blanco/negro)
#define BYTES_PER_BLOCK     (BLOCK_SIZE * BLOCK_SIZE / 8)  // 125,000 bytes por bloque
#define MAX_FILENAME        255
#define MAX_PATH            4096
#define INODE_SIZE          128         // Tamaño de un inodo en bytes
#define INODES_PER_BLOCK    (BYTES_PER_BLOCK / INODE_SIZE)  // 976 inodos por bloque
#define DIRECT_BLOCKS       12          // Bloques directos en un inodo
#define INDIRECT_BLOCK_PTRS (BYTES_PER_BLOCK / sizeof(uint32_t))  // 31,250 punteros
#define SALT_SIZE           16
#define KEY_SIZE            32
#define IV_SIZE             16

// Tipos de archivo
#define BWFS_FILE_TYPE_UNKNOWN  0
#define BWFS_FILE_TYPE_REG      1  // Archivo regular
#define BWFS_FILE_TYPE_DIR      2  // Directorio
#define BWFS_FILE_TYPE_LINK     3  // Enlace simbólico

// Modos de archivo (compatible con POSIX)
#define BWFS_S_IFMT    0170000  // Máscara de tipo de archivo
#define BWFS_S_IFREG   0100000  // Archivo regular
#define BWFS_S_IFDIR   0040000  // Directorio
#define BWFS_S_IFLNK   0120000  // Enlace simbólico

// ==================== ESTRUCTURAS BMP ====================
#pragma pack(push, 1)
typedef struct {
    uint16_t type;              // "BM"
    uint32_t size;              // Tamaño del archivo
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;            // Offset a los datos de píxeles
} BMPHeader;

typedef struct {
    uint32_t size;              // Tamaño de este header
    int32_t width;              // Ancho en píxeles
    int32_t height;             // Alto en píxeles
    uint16_t planes;            // Siempre 1
    uint16_t bits_per_pixel;    // 1 para blanco y negro
    uint32_t compression;       // 0 = sin compresión
    uint32_t image_size;        // Tamaño de los datos de imagen
    int32_t x_pixels_per_m;     // Resolución horizontal
    int32_t y_pixels_per_m;     // Resolución vertical
    uint32_t colors_used;       // 2 para blanco y negro
    uint32_t important_colors;  // 2
} BMPInfoHeader;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} RGBQuad;
#pragma pack(pop)

// ==================== ESTRUCTURAS DEL FILESYSTEM ====================
typedef struct {
    uint32_t magic;             // Número mágico BWFS
    uint32_t version;           // Versión del FS
    uint32_t block_size;        // Tamaño del bloque en píxeles
    uint32_t total_blocks;      // Total de bloques en el FS
    uint32_t free_blocks;       // Bloques libres
    uint32_t total_inodes;      // Total de inodos
    uint32_t free_inodes;       // Inodos libres
    uint32_t first_data_block;  // Primer bloque de datos
    uint32_t inode_table_blocks;// Bloques usados para tabla de inodos
    uint32_t bitmap_blocks;     // Bloques usados para bitmap
    uint32_t root_inode;        // Inodo del directorio raíz
    uint8_t encrypted;          // Si los metadatos están cifrados
    uint8_t salt[SALT_SIZE];    // Salt para derivar clave de cifrado
    char mount_time[64];        // Tiempo de creación
    char last_mount[64];        // Último montaje
    char signature[256];        // Firma/passphrase hasheada
} Superblock;

typedef struct {
    uint32_t inode_number;      // Número de inodo
    uint32_t mode;              // Permisos y tipo de archivo
    uint32_t uid;               // User ID
    uint32_t gid;               // Group ID
    uint64_t size;              // Tamaño del archivo
    uint64_t atime;             // Tiempo de acceso
    uint64_t mtime;             // Tiempo de modificación
    uint64_t ctime;             // Tiempo de creación
    uint32_t blocks[DIRECT_BLOCKS];    // Bloques directos
    uint32_t indirect_block;    // Bloque indirecto simple
    uint32_t double_indirect;   // Bloque indirecto doble
    uint32_t triple_indirect;   // Bloque indirecto triple
    uint32_t link_count;        // Número de hard links
    uint8_t reserved[20];       // Espacio reservado para futuras extensiones
} Inode;

typedef struct {
    uint32_t inode;             // Número de inodo
    uint16_t rec_len;           // Longitud total de este registro
    uint8_t name_len;           // Longitud del nombre
    uint8_t file_type;          // Tipo de archivo
    char name[MAX_FILENAME];    // Nombre del archivo
} DirectoryEntry;

// ==================== FUNCIONES DE UTILIDAD ====================

// Funciones de manejo de bloques
void write_block(uint32_t block_num, const uint8_t *data, const char *fs_path);
void read_block(uint32_t block_num, uint8_t *data, const char *fs_path);

// Funciones de bitmap
void set_bit(uint8_t *bitmap, uint32_t bit);
void clear_bit(uint8_t *bitmap, uint32_t bit);
int get_bit(const uint8_t *bitmap, uint32_t bit);
uint32_t find_free_bit(const uint8_t *bitmap, uint32_t size);

// Funciones de cifrado
void derive_key(const char *pass, const uint8_t *salt, uint8_t *key);
void encrypt_decrypt(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, int encrypt);

// Funciones de conversión
uint32_t get_file_type_from_mode(uint32_t mode);
uint8_t get_dir_entry_type_from_mode(uint32_t mode);

// Funciones de debugging
void print_superblock(const Superblock *sb);
void print_inode(const Inode *inode);
void print_directory_entry(const DirectoryEntry *entry);

#endif // BWFS_COMMON_H