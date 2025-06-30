// -----------------------------------------------------------------------------
// File: src/cli/fsck_bwfs.c
// -----------------------------------------------------------------------------
/**
 * \file fsck_bwfs.c
 * \brief Verificador de consistencia para Black & White Filesystem.
 *
 * Uso:
 *     fsck_bwfs [-f] [-y] <directorio_FS>
 *
 * Opciones:
 *   -f  Force: verificar incluso si el FS parece limpio
 *   -y  Yes: reparar automáticamente sin preguntar
 *
 * Códigos de salida:
 *   0 - Filesystem limpio
 *   1 - Errores encontrados y corregidos
 *   4 - Errores encontrados pero no corregidos
 *   8 - Error operacional (no se pudo acceder al FS)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdarg.h>     /* Para va_list en fsck_log */

#include "bwfs_common.h"
#include "bitmap.h"
#include "inode.h"
#include "dir.h"
#include "util.h"

/* ------------------------------------------------------------------------- */
/* Estructuras de estado del fsck                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    const char *fs_dir;
    bool force_check;
    bool auto_repair;
    bool verbose;
    
    /* Estadísticas */
    uint32_t errors_found;
    uint32_t errors_fixed;
    uint32_t warnings;
    
    /* Estado cargado */
    bwfs_superblock_t sb;
    bwfs_bitmap_t bitmap;
    uint8_t *inode_used;     /* Bitmap de inodos encontrados */
    uint8_t *block_used;     /* Bitmap de bloques realmente referenciados */
} fsck_context_t;

#define FSCK_ERROR   0x01
#define FSCK_WARNING 0x02
#define FSCK_INFO    0x04

/* ------------------------------------------------------------------------- */
/* Utilidades de logging                                                     */
/* ------------------------------------------------------------------------- */

static void fsck_log(fsck_context_t *ctx, int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    
    const char *prefix;
    switch (level) {
        case FSCK_ERROR:   prefix = "[ERROR] "; ctx->errors_found++; break;
        case FSCK_WARNING: prefix = "[WARN]  "; ctx->warnings++; break;
        case FSCK_INFO:    prefix = "[INFO]  "; break;
        default:           prefix = "        "; break;
    }
    
    printf("%s", prefix);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static bool fsck_ask_repair(fsck_context_t *ctx, const char *question)
{
    if (ctx->auto_repair) {
        printf("        Auto-reparando: %s\n", question);
        return true;
    }
    
    printf("        %s (y/n)? ", question);
    fflush(stdout);
    
    char response;
    if (scanf(" %c", &response) == 1 && (response == 'y' || response == 'Y')) {
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Verificaciones individuales                                               */
/* ------------------------------------------------------------------------- */

/**
 * \brief Verifica la integridad del superbloque.
 */
static int check_superblock(fsck_context_t *ctx)
{
    printf("Verificando superbloque...\n");
    
    if (bwfs_read_superblock(&ctx->sb, ctx->fs_dir) != BWFS_OK) {
        fsck_log(ctx, FSCK_ERROR, "No se pudo leer el superbloque");
        return -1;
    }
    
    /* Verificar magic number */
    if (ctx->sb.magic != BWFS_MAGIC) {
        fsck_log(ctx, FSCK_ERROR, "Magic number inválido: 0x%08x (esperado 0x%08x)",
                 ctx->sb.magic, BWFS_MAGIC);
        return -1;
    }
    
    /* Verificar tamaño de bloque */
    if (ctx->sb.block_size != BWFS_BLOCK_SIZE_BITS) {
        fsck_log(ctx, FSCK_ERROR, "Tamaño de bloque inválido: %u (esperado %u)",
                 ctx->sb.block_size, BWFS_BLOCK_SIZE_BITS);
        return -1;
    }
    
    /* Verificar cantidad mínima de bloques */
    if (ctx->sb.total_blocks < 3) {
        fsck_log(ctx, FSCK_ERROR, "Muy pocos bloques: %u (mínimo 3)",
                 ctx->sb.total_blocks);
        return -1;
    }
    
    /* Verificar que root_inode esté en rango válido */
    if (ctx->sb.root_inode >= ctx->sb.total_blocks) {
        fsck_log(ctx, FSCK_ERROR, "Inodo raíz fuera de rango: %u >= %u",
                 ctx->sb.root_inode, ctx->sb.total_blocks);
        return -1;
    }
    
    fsck_log(ctx, FSCK_INFO, "Superbloque OK (%u bloques, raíz=%u)",
             ctx->sb.total_blocks, ctx->sb.root_inode);
    return 0;
}

/**
 * \brief Carga y verifica el bitmap de bloques.
 */
static int check_bitmap(fsck_context_t *ctx)
{
    printf("Verificando bitmap de bloques...\n");
    
    ctx->bitmap.total_blocks = ctx->sb.total_blocks;
    if (bwfs_read_bitmap(&ctx->bitmap, ctx->fs_dir) != BWFS_OK) {
        fsck_log(ctx, FSCK_ERROR, "No se pudo leer el bitmap");
        return -1;
    }
    
    /* Verificar que bloques críticos estén marcados como ocupados */
    if (!bwfs_bm_test(&ctx->bitmap, BWFS_SUPERBLOCK_BLK)) {
        fsck_log(ctx, FSCK_ERROR, "Bloque del superbloque marcado como libre");
        if (fsck_ask_repair(ctx, "Marcar bloque del superbloque como ocupado")) {
            bwfs_bm_set(&ctx->bitmap, BWFS_SUPERBLOCK_BLK, 1);
            ctx->errors_fixed++;
        }
    }
    
    if (!bwfs_bm_test(&ctx->bitmap, BWFS_BITMAP_BLK)) {
        fsck_log(ctx, FSCK_ERROR, "Bloque del bitmap marcado como libre");
        if (fsck_ask_repair(ctx, "Marcar bloque del bitmap como ocupado")) {
            bwfs_bm_set(&ctx->bitmap, BWFS_BITMAP_BLK, 1);
            ctx->errors_fixed++;
        }
    }
    
    if (!bwfs_bm_test(&ctx->bitmap, ctx->sb.root_inode)) {
        fsck_log(ctx, FSCK_ERROR, "Inodo raíz marcado como libre");
        if (fsck_ask_repair(ctx, "Marcar inodo raíz como ocupado")) {
            bwfs_bm_set(&ctx->bitmap, ctx->sb.root_inode, 1);
            ctx->errors_fixed++;
        }
    }
    
    /* Preparar bitmap de uso real para comparación posterior */
    size_t block_bytes = (ctx->sb.total_blocks + 7) / 8;
    ctx->block_used = (uint8_t *)calloc(1, block_bytes);
    if (!ctx->block_used) {
        fsck_log(ctx, FSCK_ERROR, "Sin memoria para bitmap de verificación");
        return -1;
    }
    
    /* Marcar bloques críticos como usados */
    ctx->block_used[BWFS_SUPERBLOCK_BLK / 8] |= (1 << (BWFS_SUPERBLOCK_BLK % 8));
    ctx->block_used[BWFS_BITMAP_BLK / 8]     |= (1 << (BWFS_BITMAP_BLK % 8));
    ctx->block_used[ctx->sb.root_inode / 8]  |= (1 << (ctx->sb.root_inode % 8));
    
    fsck_log(ctx, FSCK_INFO, "Bitmap cargado correctamente");
    return 0;
}

/**
 * \brief Verifica la validez de un inodo individual.
 */
static int check_single_inode(fsck_context_t *ctx, uint32_t ino)
{
    bwfs_inode_t inode;
    if (bwfs_read_inode(ino, &inode, ctx->fs_dir) != BWFS_OK) {
        fsck_log(ctx, FSCK_ERROR, "No se pudo leer inodo %u", ino);
        return -1;
    }
    
    /* Verificar consistencia básica */
    if (inode.ino != ino) {
        fsck_log(ctx, FSCK_ERROR, "Inodo %u: número incorrecto en metadatos (%u)",
                 ino, inode.ino);
        if (fsck_ask_repair(ctx, "Corregir número de inodo")) {
            inode.ino = ino;
            if (bwfs_write_inode(&inode, ctx->fs_dir) == BWFS_OK) {
                ctx->errors_fixed++;
            }
        }
    }
    
    /* Verificar que block_count coincida con bloques asignados */
    uint32_t real_blocks = 0;
    for (uint32_t i = 0; i < BWFS_DIRECT_BLOCKS && inode.blocks[i] != 0; ++i) {
        real_blocks++;
        
        /* Verificar que el bloque esté en rango */
        if (inode.blocks[i] >= ctx->sb.total_blocks) {
            fsck_log(ctx, FSCK_ERROR, "Inodo %u: bloque %u fuera de rango",
                     ino, inode.blocks[i]);
            return -1;
        }
        
        /* Marcar bloque como usado realmente */
        uint32_t blk = inode.blocks[i];
        ctx->block_used[blk / 8] |= (1 << (blk % 8));
    }
    
    if (inode.block_count != real_blocks) {
        fsck_log(ctx, FSCK_ERROR, "Inodo %u: block_count=%u pero tiene %u bloques",
                 ino, inode.block_count, real_blocks);
        if (fsck_ask_repair(ctx, "Corregir block_count")) {
            inode.block_count = real_blocks;
            if (bwfs_write_inode(&inode, ctx->fs_dir) == BWFS_OK) {
                ctx->errors_fixed++;
            }
        }
    }
    
    /* Verificar tamaño vs bloques para archivos */
    if (!(inode.flags & BWFS_INODE_DIR)) {
        uint32_t max_size = inode.block_count * BWFS_BLOCK_SIZE_BYTES;
        if (inode.size > max_size) {
            fsck_log(ctx, FSCK_ERROR, "Inodo %u: tamaño %u excede capacidad %u",
                     ino, inode.size, max_size);
            if (fsck_ask_repair(ctx, "Truncar archivo al tamaño máximo")) {
                inode.size = max_size;
                if (bwfs_write_inode(&inode, ctx->fs_dir) == BWFS_OK) {
                    ctx->errors_fixed++;
                }
            }
        }
    }
    
    return 0;
}

/**
 * \brief Verifica recursivamente la estructura de directorios.
 */
static int check_directory_recursive(fsck_context_t *ctx, uint32_t dir_ino, int depth)
{
    if (depth > 100) {  /* Prevenir bucles infinitos */
        fsck_log(ctx, FSCK_ERROR, "Directorio %u: profundidad excesiva (posible bucle)", dir_ino);
        return -1;
    }
    
    bwfs_inode_t dir_inode;
    if (bwfs_read_inode(dir_ino, &dir_inode, ctx->fs_dir) != BWFS_OK) {
        fsck_log(ctx, FSCK_ERROR, "No se pudo leer directorio %u", dir_ino);
        return -1;
    }
    
    if (!(dir_inode.flags & BWFS_INODE_DIR)) {
        fsck_log(ctx, FSCK_ERROR, "Inodo %u no es un directorio", dir_ino);
        return -1;
    }
    
    /* Marcar inodo como encontrado */
    ctx->inode_used[dir_ino / 8] |= (1 << (dir_ino % 8));
    
    if (dir_inode.block_count == 0) {
        /* Directorio vacío es válido */
        return 0;
    }
    
    /* Leer entradas del directorio */
    size_t max_entries = BWFS_BLOCK_SIZE_BYTES / sizeof(bwfs_dir_entry_t);
    bwfs_dir_entry_t *entries = malloc(max_entries * sizeof(bwfs_dir_entry_t));
    if (!entries) {
        fsck_log(ctx, FSCK_ERROR, "Sin memoria para leer directorio %u", dir_ino);
        return -1;
    }
    
    if (util_read_block(ctx->fs_dir, dir_inode.blocks[0],
                        (uint8_t *)entries, BWFS_BLOCK_SIZE_BYTES) != 0) {
        fsck_log(ctx, FSCK_ERROR, "No se pudo leer bloque de directorio %u", dir_ino);
        free(entries);
        return -1;
    }
    
    uint32_t entry_count = 0;
    for (size_t i = 0; i < max_entries; ++i) {
        if (entries[i].ino == 0) continue;
        
        entry_count++;
        uint32_t child_ino = entries[i].ino;
        
        /* Verificar que el inodo hijo exista */
        if (child_ino >= ctx->sb.total_blocks) {
            fsck_log(ctx, FSCK_ERROR, "Directorio %u: entrada '%s' apunta a inodo inválido %u",
                     dir_ino, entries[i].name, child_ino);
            continue;
        }
        
        /* Verificar inodo hijo */
        if (check_single_inode(ctx, child_ino) != 0) {
            continue;
        }
        
        /* Si es directorio, verificar recursivamente */
        bwfs_inode_t child;
        if (bwfs_read_inode(child_ino, &child, ctx->fs_dir) == BWFS_OK) {
            if (child.flags & BWFS_INODE_DIR) {
                check_directory_recursive(ctx, child_ino, depth + 1);
            }
            /* Marcar como encontrado */
            ctx->inode_used[child_ino / 8] |= (1 << (child_ino % 8));
        }
    }
    
    /* Verificar consistencia del tamaño del directorio */
    uint32_t expected_size = entry_count * sizeof(bwfs_dir_entry_t);
    if (dir_inode.size != expected_size) {
        fsck_log(ctx, FSCK_WARNING, "Directorio %u: tamaño inconsistente (%u vs %u esperado)",
                 dir_ino, dir_inode.size, expected_size);
    }
    
    free(entries);
    return 0;
}

/**
 * \brief Compara bitmap del disco vs uso real y reporta inconsistencias.
 */
static int check_bitmap_consistency(fsck_context_t *ctx)
{
    printf("Verificando consistencia del bitmap...\n");
    
    bool bitmap_dirty = false;
    uint32_t leaked_blocks = 0;
    uint32_t false_used = 0;
    
    for (uint32_t i = 0; i < ctx->sb.total_blocks; ++i) {
        bool bitmap_says_used = bwfs_bm_test(&ctx->bitmap, i);
        bool really_used = (ctx->block_used[i / 8] & (1 << (i % 8))) != 0;
        
        if (bitmap_says_used && !really_used) {
            fsck_log(ctx, FSCK_WARNING, "Bloque %u marcado como usado pero no referenciado", i);
            leaked_blocks++;
            if (fsck_ask_repair(ctx, "Marcar bloque como libre")) {
                bwfs_bm_set(&ctx->bitmap, i, 0);
                bitmap_dirty = true;
                ctx->errors_fixed++;
            }
        }
        else if (!bitmap_says_used && really_used) {
            fsck_log(ctx, FSCK_ERROR, "Bloque %u usado pero marcado como libre", i);
            false_used++;
            if (fsck_ask_repair(ctx, "Marcar bloque como usado")) {
                bwfs_bm_set(&ctx->bitmap, i, 1);
                bitmap_dirty = true;
                ctx->errors_fixed++;
            }
        }
    }
    
    if (bitmap_dirty) {
        if (bwfs_write_bitmap(&ctx->bitmap, ctx->fs_dir) != BWFS_OK) {
            fsck_log(ctx, FSCK_ERROR, "No se pudo escribir bitmap corregido");
            return -1;
        }
    }
    
    if (leaked_blocks == 0 && false_used == 0) {
        fsck_log(ctx, FSCK_INFO, "Bitmap consistente");
    } else {
        fsck_log(ctx, FSCK_INFO, "Inconsistencias: %u bloques filtrados, %u mal marcados",
                 leaked_blocks, false_used);
    }
    
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Función principal de verificación                                         */
/* ------------------------------------------------------------------------- */

static int run_fsck(fsck_context_t *ctx)
{
    printf("=== FSCK.BWFS - Verificando %s ===\n", ctx->fs_dir);
    
    /* 1. Verificar superbloque */
    if (check_superblock(ctx) != 0) {
        return -1;
    }
    
    /* 2. Cargar y verificar bitmap */
    if (check_bitmap(ctx) != 0) {
        return -1;
    }
    
    /* 3. Preparar bitmap de inodos encontrados */
    size_t inode_bytes = (ctx->sb.total_blocks + 7) / 8;
    ctx->inode_used = (uint8_t *)calloc(1, inode_bytes);
    if (!ctx->inode_used) {
        fsck_log(ctx, FSCK_ERROR, "Sin memoria para bitmap de inodos");
        return -1;
    }
    
    /* 4. Verificar estructura de directorios desde la raíz */
    printf("Verificando estructura de directorios...\n");
    if (check_directory_recursive(ctx, ctx->sb.root_inode, 0) != 0) {
        return -1;
    }
    
    /* 5. Verificar consistencia del bitmap */
    if (check_bitmap_consistency(ctx) != 0) {
        return -1;
    }
    
    /* 6. Buscar inodos huérfanos */
    printf("Buscando inodos huérfanos...\n");
    uint32_t orphans = 0;
    for (uint32_t i = 2; i < ctx->sb.total_blocks; ++i) {  /* Empezar después del bitmap */
        if (bwfs_bm_test(&ctx->bitmap, i) && !(ctx->inode_used[i / 8] & (1 << (i % 8)))) {
            /* Verificar si realmente es un inodo válido */
            bwfs_inode_t test;
            if (bwfs_read_inode(i, &test, ctx->fs_dir) == BWFS_OK && test.ino == i) {
                fsck_log(ctx, FSCK_WARNING, "Inodo huérfano encontrado: %u", i);
                orphans++;
                /* Nota: en un fsck real, aquí se movería a lost+found */
            }
        }
    }
    
    if (orphans == 0) {
        fsck_log(ctx, FSCK_INFO, "No se encontraron inodos huérfanos");
    }
    
    return 0;
}

static void print_summary(fsck_context_t *ctx)
{
    printf("\n=== RESUMEN ===\n");
    printf("Errores encontrados:  %u\n", ctx->errors_found);
    printf("Errores corregidos:   %u\n", ctx->errors_fixed);
    printf("Advertencias:         %u\n", ctx->warnings);
    
    if (ctx->errors_found == 0) {
        printf("Filesystem LIMPIO\n");
    } else if (ctx->errors_fixed == ctx->errors_found) {
        printf("Filesystem REPARADO\n");
    } else {
        printf("Filesystem CON ERRORES\n");
    }
}

static void cleanup(fsck_context_t *ctx)
{
    if (ctx->bitmap.map) free(ctx->bitmap.map);
    if (ctx->block_used) free(ctx->block_used);
    if (ctx->inode_used) free(ctx->inode_used);
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    fsck_context_t ctx = {0};
    
    /* Parsear argumentos */
    int opt;
    while ((opt = getopt(argc, argv, "fyv")) != -1) {
        switch (opt) {
            case 'f': ctx.force_check = true; break;
            case 'y': ctx.auto_repair = true; break;
            case 'v': ctx.verbose = true; break;
            default:
                fprintf(stderr, "Uso: %s [-f] [-y] [-v] <directorio_FS>\n", argv[0]);
                return 8;
        }
    }
    
    if (optind != argc - 1) {
        fprintf(stderr, "Falta especificar directorio del filesystem\n");
        return 8;
    }
    
    ctx.fs_dir = argv[optind];
    
    /* Verificar que el directorio existe */
    struct stat st;
    if (stat(ctx.fs_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' no es un directorio accesible\n", ctx.fs_dir);
        return 8;
    }
    
    /* Ejecutar verificación */
    int result = run_fsck(&ctx);
    print_summary(&ctx);
    cleanup(&ctx);
    
    /* Código de salida estándar fsck */
    if (result != 0) {
        return 8;  /* Error operacional */
    } else if (ctx.errors_found == 0) {
        return 0;  /* Filesystem limpio */
    } else if (ctx.errors_fixed == ctx.errors_found) {
        return 1;  /* Errores corregidos */
    } else {
        return 4;  /* Errores sin corregir */
    }
}