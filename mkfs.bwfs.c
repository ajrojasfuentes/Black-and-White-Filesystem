/*
 * mkfs.bwfs - Black & White Filesystem Creator
 * 
 * Este programa crea un nuevo sistema de archivos BWFS almacenado en imágenes BMP
 * de blanco y negro. Los datos se almacenan en los píxeles de las imágenes.
 */

#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "bwfs_common.h"

// ==================== VARIABLES GLOBALES ====================
static Superblock superblock;
static uint8_t *block_bitmap = NULL;
static uint8_t *inode_bitmap = NULL;
static char fs_path[MAX_PATH];
static char passphrase[256];
static uint8_t encryption_key[KEY_SIZE];

// ==================== FUNCIONES DE UTILIDAD LOCALES ====================
void print_usage(const char *program) {
    printf("Uso: %s <directorio>\n", program);
    printf("Crea un nuevo sistema de archivos BWFS en el directorio especificado.\n");
}

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// ==================== FUNCIONES PRINCIPALES ====================
void initialize_superblock(uint32_t total_blocks) {
    memset(&superblock, 0, sizeof(superblock));
    
    superblock.magic = BWFS_MAGIC;
    superblock.version = 1;
    superblock.block_size = BLOCK_SIZE;
    superblock.total_blocks = total_blocks;
    
    // Calcular bloques necesarios para estructuras
    superblock.bitmap_blocks = (total_blocks + BYTES_PER_BLOCK * 8 - 1) / (BYTES_PER_BLOCK * 8);
    superblock.inode_table_blocks = 10;  // Espacio para ~9760 inodos
    superblock.first_data_block = 1 + superblock.bitmap_blocks + superblock.inode_table_blocks;
    
    superblock.total_inodes = superblock.inode_table_blocks * INODES_PER_BLOCK;
    superblock.free_inodes = superblock.total_inodes - 1;  // -1 por el root
    superblock.free_blocks = total_blocks - superblock.first_data_block - 1;  // -1 por root dir
    
    superblock.root_inode = 0;  // Primer inodo es el root
    
    // Tiempo de creación
    time_t now = time(NULL);
    strftime(superblock.mount_time, sizeof(superblock.mount_time), "%Y-%m-%d %H:%M:%S", localtime(&now));
    strcpy(superblock.last_mount, superblock.mount_time);
    
    // Generar salt para cifrado
    if (strlen(passphrase) > 0) {
        superblock.encrypted = 1;
        RAND_bytes(superblock.salt, SALT_SIZE);
        derive_key(passphrase, superblock.salt, encryption_key);
        
        // Hash de la passphrase como firma
        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        unsigned int md_len;
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(mdctx, passphrase, strlen(passphrase));
        EVP_DigestFinal_ex(mdctx, (unsigned char *)superblock.signature, &md_len);
        EVP_MD_CTX_free(mdctx);
    }
}

void create_root_directory() {
    // Crear inodo root
    Inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    
    root_inode.inode_number = 0;
    root_inode.mode = 0755 | 0x4000;  // Directory with rwxr-xr-x
    root_inode.uid = getuid();
    root_inode.gid = getgid();
    root_inode.size = BYTES_PER_BLOCK;  // Un bloque para el directorio
    
    time_t now = time(NULL);
    root_inode.atime = root_inode.mtime = root_inode.ctime = now;
    
    // Asignar primer bloque de datos al root
    root_inode.blocks[0] = superblock.first_data_block;
    root_inode.link_count = 2;  // . y ..
    
    // Escribir inodo root
    uint8_t *inode_block = calloc(1, BYTES_PER_BLOCK);
    memcpy(inode_block, &root_inode, sizeof(root_inode));
    
    if (superblock.encrypted) {
        uint8_t iv[IV_SIZE];
        RAND_bytes(iv, IV_SIZE);
        encrypt_decrypt(inode_block, BYTES_PER_BLOCK, encryption_key, iv, 1);
    }
    
    write_block(1 + superblock.bitmap_blocks, inode_block, fs_path);  // Primer bloque de inodos
    free(inode_block);
    
    // Crear entradas de directorio para . y ..
    uint8_t *dir_block = calloc(1, BYTES_PER_BLOCK);
    DirectoryEntry *dot = (DirectoryEntry *)dir_block;
    DirectoryEntry *dotdot = (DirectoryEntry *)(dir_block + sizeof(DirectoryEntry));
    
    // Entrada "."
    dot->inode = 0;
    dot->rec_len = sizeof(DirectoryEntry);
    dot->name_len = 1;
    dot->file_type = 2;  // Directory
    strcpy(dot->name, ".");
    
    // Entrada ".."
    dotdot->inode = 0;  // En root, .. apunta a sí mismo
    dotdot->rec_len = sizeof(DirectoryEntry);  // Tamaño fijo por ahora
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    strcpy(dotdot->name, "..");
    
    // Marcar el resto del bloque como entrada vacía
    DirectoryEntry *end_marker = (DirectoryEntry *)(dir_block + 2 * sizeof(DirectoryEntry));
    end_marker->inode = 0;
    end_marker->rec_len = 0xFFFF;  // Marcador especial para fin de entradas
    end_marker->name_len = 0;
    end_marker->file_type = 0;    
    write_block(superblock.first_data_block, dir_block, fs_path);
    free(dir_block);
    
    // Marcar bloque como usado
    set_bit(block_bitmap, superblock.first_data_block);
    
    // Marcar inodo como usado
    set_bit(inode_bitmap, 0);
}

void write_metadata() {
    uint8_t *buffer = calloc(1, BYTES_PER_BLOCK);
    
    // Escribir superblock (bloque 0)
    memcpy(buffer, &superblock, sizeof(superblock));
    
    if (superblock.encrypted) {
        uint8_t iv[IV_SIZE];
        RAND_bytes(iv, IV_SIZE);
        // Solo cifrar la parte del superblock después de los campos de cifrado
        size_t offset = offsetof(Superblock, mount_time);
        encrypt_decrypt(buffer + offset, sizeof(superblock) - offset, encryption_key, iv, 1);
    }
    
    write_block(0, buffer, fs_path);
    
    // Escribir bitmap de bloques
    for (uint32_t i = 0; i < superblock.bitmap_blocks; i++) {
        memset(buffer, 0, BYTES_PER_BLOCK);
        size_t bytes_to_copy = (superblock.total_blocks / 8) - (i * BYTES_PER_BLOCK);
        if (bytes_to_copy > BYTES_PER_BLOCK) bytes_to_copy = BYTES_PER_BLOCK;
        
        memcpy(buffer, block_bitmap + (i * BYTES_PER_BLOCK), bytes_to_copy);
        
        if (superblock.encrypted) {
            uint8_t iv[IV_SIZE];
            RAND_bytes(iv, IV_SIZE);
            encrypt_decrypt(buffer, BYTES_PER_BLOCK, encryption_key, iv, 1);
        }
        
        write_block(1 + i, buffer, fs_path);
    }
    
    // Escribir bitmap de inodos (como parte del primer bloque de la tabla de inodos)
    memset(buffer, 0, BYTES_PER_BLOCK);
    memcpy(buffer + sizeof(Inode), inode_bitmap, superblock.total_inodes / 8);
    
    if (superblock.encrypted) {
        uint8_t iv[IV_SIZE];
        RAND_bytes(iv, IV_SIZE);
        encrypt_decrypt(buffer + sizeof(Inode), BYTES_PER_BLOCK - sizeof(Inode), encryption_key, iv, 1);
    }
    
    // El inodo root ya fue escrito, así que actualizamos el bloque
    uint8_t *inode_block = malloc(BYTES_PER_BLOCK);
    read_block(1 + superblock.bitmap_blocks, inode_block, fs_path);
    memcpy(inode_block + sizeof(Inode), buffer + sizeof(Inode), BYTES_PER_BLOCK - sizeof(Inode));
    write_block(1 + superblock.bitmap_blocks, inode_block, fs_path);
    
    free(inode_block);
    free(buffer);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    strcpy(fs_path, argv[1]);
    
    // Crear directorio si no existe
    struct stat st;
    if (stat(fs_path, &st) == -1) {
        if (mkdir(fs_path, 0755) == -1) {
            die("Error creando directorio");
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s no es un directorio\n", fs_path);
        return EXIT_FAILURE;
    }
    
    // Solicitar passphrase
    printf("Introduce una passphrase para el filesystem (vacío para sin cifrado): ");
    fflush(stdout);
    
    // Leer passphrase de forma segura
    struct termios old, new;
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    
    fgets(passphrase, sizeof(passphrase), stdin);
    passphrase[strcspn(passphrase, "\n")] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");
    
    // Configurar sistema de archivos
    uint32_t total_blocks = 100;  // 100 bloques = ~12.5 MB
    
    printf("Creando BWFS con las siguientes características:\n");
    printf("- Bloques totales: %u\n", total_blocks);
    printf("- Tamaño de bloque: %dx%d píxeles\n", BLOCK_SIZE, BLOCK_SIZE);
    printf("- Bytes por bloque: %d\n", BYTES_PER_BLOCK);
    printf("- Cifrado: %s\n", strlen(passphrase) > 0 ? "Sí" : "No");
    
    // Inicializar estructuras
    initialize_superblock(total_blocks);
    
    // Allocar bitmaps
    block_bitmap = calloc(1, (superblock.total_blocks + 7) / 8);
    inode_bitmap = calloc(1, (superblock.total_inodes + 7) / 8);
    
    // Marcar bloques del sistema como usados
    for (uint32_t i = 0; i < superblock.first_data_block; i++) {
        set_bit(block_bitmap, i);
    }
    
    // Crear directorio root
    create_root_directory();
    
    // Escribir metadatos
    write_metadata();
    
    // Limpiar
    free(block_bitmap);
    free(inode_bitmap);
    
    printf("\nSistema de archivos BWFS creado exitosamente en %s\n", fs_path);
    printf("Total de espacio: %.2f MB\n", (double)(total_blocks * BYTES_PER_BLOCK) / (1024 * 1024));
    printf("Espacio disponible: %.2f MB\n", (double)(superblock.free_blocks * BYTES_PER_BLOCK) / (1024 * 1024));
    
    return EXIT_SUCCESS;
}