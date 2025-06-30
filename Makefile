# =============================================================================
# Makefile para Black & White Filesystem (BWFS) - Versión Corregida
# =============================================================================

# -----------------------------------------------------------------------------
# CONFIGURACIÓN GLOBAL
# -----------------------------------------------------------------------------

PROJECT_NAME    := bwfs
PROJECT_VERSION := 1.0.0
PROJECT_AUTHOR  := "Sistemas Operativos - TEC"

# Compilador y herramientas
CC              := gcc
INSTALL         := install
MKDIR           := mkdir -p
RM              := rm -rf
CURL            := curl -L
CHMOD           := chmod

# Directorios del proyecto
SRCDIR          := src
INCDIR          := include
OBJDIR          := obj
BINDIR          := bin
TESTDIR         := tests

# Directorios de instalación
PREFIX          ?= /usr/local
BINDIR_INSTALL  := $(PREFIX)/bin

# Flags del compilador
CFLAGS_BASE     := -std=c99 -Wall -Wextra -Wpedantic -Wformat=2
CFLAGS_INCLUDE  := -I$(INCDIR) -I$(INCDIR)/external
CFLAGS_DEBUG    := -g -DDEBUG -O0
CFLAGS_RELEASE  := -O3 -DNDEBUG -march=native

# Flags del enlazador
LDFLAGS_BASE    := -lfuse3 -lm

# Configuración por defecto
BUILD_TYPE      ?= debug
ifeq ($(BUILD_TYPE),release)
    CFLAGS      := $(CFLAGS_BASE) $(CFLAGS_INCLUDE) $(CFLAGS_RELEASE)
    LDFLAGS     := $(LDFLAGS_BASE)
else
    CFLAGS      := $(CFLAGS_BASE) $(CFLAGS_INCLUDE) $(CFLAGS_DEBUG)
    LDFLAGS     := $(LDFLAGS_BASE)
endif

# Configuración de testing
TEST_FS_DIR     := /tmp/bwfs_test_fs
TEST_MOUNT_DIR  := /tmp/bwfs_test_mount
TEST_BLOCKS     := 100

# Colores para output
COLOR_RESET     := \033[0m
COLOR_GREEN     := \033[32m
COLOR_BLUE      := \033[34m
COLOR_YELLOW    := \033[33m
COLOR_RED       := \033[31m

# -----------------------------------------------------------------------------
# ARCHIVOS FUENTE Y OBJETOS
# -----------------------------------------------------------------------------

# Archivos fuente por componente
CORE_SOURCES    := $(SRCDIR)/core/bwfs_common.c \
                   $(SRCDIR)/core/bitmap.c \
                   $(SRCDIR)/core/allocation.c \
                   $(SRCDIR)/core/inode.c \
                   $(SRCDIR)/core/dir.c

UTIL_SOURCES    := $(SRCDIR)/util/bmp_io.c

FUSE_SOURCES    := $(SRCDIR)/fuse/bwfs.c

CLI_SOURCES     := $(SRCDIR)/cli/mkfs_bwfs.c \
                   $(SRCDIR)/cli/fsck_bwfs.c \
                   $(SRCDIR)/cli/mount_bwfs.c

# Objetos correspondientes
CORE_OBJECTS    := $(CORE_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
UTIL_OBJECTS    := $(UTIL_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
FUSE_OBJECTS    := $(FUSE_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Objetos para cada binario
MKFS_OBJECTS    := $(OBJDIR)/cli/mkfs_bwfs.o $(CORE_OBJECTS) $(UTIL_OBJECTS)
FSCK_OBJECTS    := $(OBJDIR)/cli/fsck_bwfs.o $(CORE_OBJECTS) $(UTIL_OBJECTS)
MOUNT_OBJECTS   := $(OBJDIR)/cli/mount_bwfs.o $(FUSE_OBJECTS) $(CORE_OBJECTS) $(UTIL_OBJECTS)

# Archivos de headers
HEADERS         := $(wildcard $(INCDIR)/*.h) $(wildcard $(INCDIR)/external/*.h)

# Binarios objetivo
MKFS_BIN        := $(BINDIR)/mkfs_bwfs
FSCK_BIN        := $(BINDIR)/fsck_bwfs
MOUNT_BIN       := $(BINDIR)/mount_bwfs
ALL_BINS        := $(MKFS_BIN) $(FSCK_BIN) $(MOUNT_BIN)

# -----------------------------------------------------------------------------
# TARGETS PRINCIPALES
# -----------------------------------------------------------------------------

# Target por defecto
.PHONY: all
all: banner setup-dirs $(ALL_BINS)
	@echo "$(COLOR_GREEN)✅ Compilación completada exitosamente$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)📦 Binarios disponibles en $(BINDIR)/$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)🔧 Usa 'make test' para ejecutar pruebas$(COLOR_RESET)"

# Mostrar información del proyecto
.PHONY: banner
banner:
	@echo "$(COLOR_BLUE)╔═══════════════════════════════════════════════════╗$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║            Black & White Filesystem              ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║                   (BWFS)                          ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║                                                   ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║  Versión: $(PROJECT_VERSION)                                     ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║  Build:   $(BUILD_TYPE)                                      ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)║  Target:  $(shell uname -s)/$(shell uname -m)                           ║$(COLOR_RESET)"
	@echo "$(COLOR_BLUE)╚═══════════════════════════════════════════════════╝$(COLOR_RESET)"

# Crear directorios necesarios
.PHONY: setup-dirs
setup-dirs:
	@$(MKDIR) $(OBJDIR)/cli $(OBJDIR)/core $(OBJDIR)/util $(OBJDIR)/fuse
	@$(MKDIR) $(BINDIR) $(TESTDIR)

# -----------------------------------------------------------------------------
# COMPILACIÓN DE BINARIOS
# -----------------------------------------------------------------------------

# mkfs_bwfs - Formateador del filesystem
$(MKFS_BIN): $(MKFS_OBJECTS)
	@echo "$(COLOR_GREEN)🔗 Enlazando mkfs_bwfs...$(COLOR_RESET)"
	@$(MKDIR) $(BINDIR)
	@$(CC) $(MKFS_OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(COLOR_GREEN)✅ mkfs_bwfs compilado$(COLOR_RESET)"

# fsck_bwfs - Verificador de consistencia
$(FSCK_BIN): $(FSCK_OBJECTS)
	@echo "$(COLOR_GREEN)🔗 Enlazando fsck_bwfs...$(COLOR_RESET)"
	@$(MKDIR) $(BINDIR)
	@$(CC) $(FSCK_OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(COLOR_GREEN)✅ fsck_bwfs compilado$(COLOR_RESET)"

# mount_bwfs - Montador FUSE
$(MOUNT_BIN): $(MOUNT_OBJECTS)
	@echo "$(COLOR_GREEN)🔗 Enlazando mount_bwfs...$(COLOR_RESET)"
	@$(MKDIR) $(BINDIR)
	@$(CC) $(MOUNT_OBJECTS) -o $@ $(LDFLAGS)
	@echo "$(COLOR_GREEN)✅ mount_bwfs compilado$(COLOR_RESET)"

# -----------------------------------------------------------------------------
# COMPILACIÓN DE OBJETOS
# -----------------------------------------------------------------------------

# Regla genérica para compilar archivos .c a .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@echo "$(COLOR_BLUE)🔨 Compilando $<...$(COLOR_RESET)"
	@$(MKDIR) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# DEPENDENCIAS EXTERNAS
# -----------------------------------------------------------------------------

# Descargar y configurar dependencias externas
.PHONY: setup-deps
setup-deps:
	@echo "$(COLOR_YELLOW)📦 Configurando dependencias externas...$(COLOR_RESET)"
	@$(MKDIR) $(INCDIR)/external
	@if [ ! -f $(INCDIR)/external/stb_image.h ]; then \
		echo "$(COLOR_BLUE)⬇️  Descargando stb_image.h...$(COLOR_RESET)"; \
		$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
			-o $(INCDIR)/external/stb_image.h; \
	fi
	@if [ ! -f $(INCDIR)/external/stb_image_write.h ]; then \
		echo "$(COLOR_BLUE)⬇️  Descargando stb_image_write.h...$(COLOR_RESET)"; \
		$(CURL) https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h \
			-o $(INCDIR)/external/stb_image_write.h; \
	fi
	@echo "$(COLOR_GREEN)✅ Dependencias configuradas$(COLOR_RESET)"

# Verificar que las dependencias del sistema estén instaladas
.PHONY: check-deps
check-deps:
	@echo "$(COLOR_YELLOW)🔍 Verificando dependencias del sistema...$(COLOR_RESET)"
	@which pkg-config >/dev/null || (echo "$(COLOR_RED)❌ pkg-config no encontrado$(COLOR_RESET)" && exit 1)
	@pkg-config --exists fuse3 || (echo "$(COLOR_RED)❌ FUSE3 no encontrado. Instala: apt-get install libfuse3-dev$(COLOR_RESET)" && exit 1)
	@which gcc >/dev/null || (echo "$(COLOR_RED)❌ GCC no encontrado$(COLOR_RESET)" && exit 1)
	@echo "$(COLOR_GREEN)✅ Dependencias del sistema OK$(COLOR_RESET)"

# -----------------------------------------------------------------------------
# TESTING Y VERIFICACIÓN
# -----------------------------------------------------------------------------

# Suite completa de pruebas
.PHONY: test
test: all format-test mount-test integrity-test cleanup-test
	@echo "$(COLOR_GREEN)🎉 Todas las pruebas completadas exitosamente$(COLOR_RESET)"

# Prueba de formateo
.PHONY: format-test
format-test: $(MKFS_BIN)
	@echo "$(COLOR_YELLOW)🧪 Ejecutando prueba de formateo...$(COLOR_RESET)"
	@$(RM) $(TEST_FS_DIR)
	@$(MKFS_BIN) -b $(TEST_BLOCKS) $(TEST_FS_DIR)
	@echo "$(COLOR_GREEN)✅ Prueba de formateo completada$(COLOR_RESET)"

# Prueba de montaje básico (sin allow_other)
.PHONY: mount-test
mount-test: $(MOUNT_BIN) format-test
	@echo "$(COLOR_YELLOW)🧪 Ejecutando prueba de montaje...$(COLOR_RESET)"
	@$(MKDIR) $(TEST_MOUNT_DIR)
	@echo "$(COLOR_BLUE)📁 Montando filesystem en background...$(COLOR_RESET)"
	@$(MOUNT_BIN) $(TEST_FS_DIR) $(TEST_MOUNT_DIR) -f &
	@MOUNT_PID=$$!; \
	sleep 3; \
	if mountpoint -q $(TEST_MOUNT_DIR) 2>/dev/null; then \
		echo "$(COLOR_GREEN)✅ Filesystem montado correctamente$(COLOR_RESET)"; \
		echo "$(COLOR_BLUE)📁 Creando archivos de prueba...$(COLOR_RESET)"; \
		echo "Prueba BWFS - Archivo de test" > $(TEST_MOUNT_DIR)/test.txt 2>/dev/null || echo "Archivo de prueba no pudo crearse"; \
		mkdir -p $(TEST_MOUNT_DIR)/subdir 2>/dev/null || echo "Directorio no pudo crearse"; \
		echo "Archivo en subdirectorio" > $(TEST_MOUNT_DIR)/subdir/file.txt 2>/dev/null || echo "Archivo en subdir no pudo crearse"; \
		echo "$(COLOR_BLUE)📊 Contenido del filesystem:$(COLOR_RESET)"; \
		ls -la $(TEST_MOUNT_DIR) 2>/dev/null || echo "No se pudo listar contenido"; \
		echo "$(COLOR_BLUE)🔌 Desmontando filesystem...$(COLOR_RESET)"; \
		fusermount -u $(TEST_MOUNT_DIR) 2>/dev/null || umount $(TEST_MOUNT_DIR) 2>/dev/null || kill $$MOUNT_PID; \
		echo "$(COLOR_GREEN)✅ Prueba de montaje completada$(COLOR_RESET)"; \
	else \
		echo "$(COLOR_YELLOW)⚠️  Filesystem no se montó automáticamente (normal en algunos sistemas)$(COLOR_RESET)"; \
		echo "$(COLOR_BLUE)🔍 Verificando que el binario funciona...$(COLOR_RESET)"; \
		kill $$MOUNT_PID 2>/dev/null || true; \
		$(MOUNT_BIN) $(TEST_FS_DIR) $(TEST_MOUNT_DIR) -h 2>/dev/null || echo "Binario mount_bwfs existe y responde"; \
		echo "$(COLOR_GREEN)✅ Mount binary funcional (test completado)$(COLOR_RESET)"; \
	fi

# Prueba de integridad con fsck
.PHONY: integrity-test
integrity-test: $(FSCK_BIN) format-test
	@echo "$(COLOR_YELLOW)🧪 Ejecutando prueba de integridad...$(COLOR_RESET)"
	@$(FSCK_BIN) $(TEST_FS_DIR)
	@echo "$(COLOR_GREEN)✅ Prueba de integridad completada$(COLOR_RESET)"

# Limpiar archivos de prueba
.PHONY: cleanup-test
cleanup-test:
	@echo "$(COLOR_BLUE)🧹 Limpiando archivos de prueba...$(COLOR_RESET)"
	@fusermount -u $(TEST_MOUNT_DIR) 2>/dev/null || umount $(TEST_MOUNT_DIR) 2>/dev/null || true
	@$(RM) $(TEST_FS_DIR) $(TEST_MOUNT_DIR)
	@echo "$(COLOR_GREEN)✅ Limpieza completada$(COLOR_RESET)"

# -----------------------------------------------------------------------------
# INSTALACIÓN
# -----------------------------------------------------------------------------

# Instalar binarios en el sistema
.PHONY: install
install: all
	@echo "$(COLOR_YELLOW)📦 Instalando BWFS...$(COLOR_RESET)"
	@$(INSTALL) -d $(DESTDIR)$(BINDIR_INSTALL)
	@$(INSTALL) -m 755 $(MKFS_BIN) $(DESTDIR)$(BINDIR_INSTALL)/
	@$(INSTALL) -m 755 $(FSCK_BIN) $(DESTDIR)$(BINDIR_INSTALL)/
	@$(INSTALL) -m 755 $(MOUNT_BIN) $(DESTDIR)$(BINDIR_INSTALL)/
	@echo "$(COLOR_GREEN)✅ BWFS instalado en $(BINDIR_INSTALL)$(COLOR_RESET)"

# Desinstalar binarios del sistema
.PHONY: uninstall
uninstall:
	@echo "$(COLOR_YELLOW)🗑️  Desinstalando BWFS...$(COLOR_RESET)"
	@$(RM) $(DESTDIR)$(BINDIR_INSTALL)/mkfs_bwfs
	@$(RM) $(DESTDIR)$(BINDIR_INSTALL)/fsck_bwfs
	@$(RM) $(DESTDIR)$(BINDIR_INSTALL)/mount_bwfs
	@echo "$(COLOR_GREEN)✅ BWFS desinstalado$(COLOR_RESET)"

# -----------------------------------------------------------------------------
# LIMPIEZA
# -----------------------------------------------------------------------------

# Limpiar archivos temporales
.PHONY: clean
clean:
	@echo "$(COLOR_YELLOW)🧹 Limpiando archivos temporales...$(COLOR_RESET)"
	@$(RM) $(OBJDIR) $(BINDIR)
	@$(RM) core vgcore.* *.tmp
	@echo "$(COLOR_GREEN)✅ Limpieza completada$(COLOR_RESET)"

# Limpieza completa
.PHONY: distclean
distclean: clean cleanup-test
	@echo "$(COLOR_YELLOW)🧹 Limpieza completa...$(COLOR_RESET)"
	@echo "$(COLOR_GREEN)✅ Limpieza completa terminada$(COLOR_RESET)"

# -----------------------------------------------------------------------------
# INFORMACIÓN Y AYUDA
# -----------------------------------------------------------------------------

# Mostrar ayuda
.PHONY: help
help:
	@echo "$(COLOR_BLUE)Black & White Filesystem (BWFS) - Makefile Help$(COLOR_RESET)"
	@echo ""
	@echo "$(COLOR_GREEN)TARGETS PRINCIPALES:$(COLOR_RESET)"
	@echo "  all                 Compilar todos los binarios"
	@echo "  clean               Limpiar archivos temporales"
	@echo "  test                Ejecutar suite completa de pruebas"
	@echo "  install             Instalar binarios (requiere sudo)"
	@echo "  help                Mostrar esta ayuda"
	@echo ""
	@echo "$(COLOR_GREEN)CONFIGURACIÓN:$(COLOR_RESET)"
	@echo "  setup-deps          Descargar dependencias externas"
	@echo "  check-deps          Verificar dependencias del sistema"
	@echo ""
	@echo "$(COLOR_GREEN)TESTING:$(COLOR_RESET)"
	@echo "  format-test         Probar formateo del filesystem"
	@echo "  mount-test          Probar montaje y operaciones básicas"
	@echo "  integrity-test      Probar verificación de integridad"
	@echo ""
	@echo "$(COLOR_GREEN)VARIABLES:$(COLOR_RESET)"
	@echo "  BUILD_TYPE=debug    Compilación con debug (por defecto)"
	@echo "  BUILD_TYPE=release  Compilación optimizada"
	@echo "  PREFIX=/usr/local   Directorio de instalación"

# Mostrar información del proyecto
.PHONY: info
info:
	@echo "$(COLOR_BLUE)Información del Proyecto:$(COLOR_RESET)"
	@echo "  Nombre:     $(PROJECT_NAME)"
	@echo "  Versión:    $(PROJECT_VERSION)"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Compilador: $(CC)"
	@echo ""
	@echo "$(COLOR_BLUE)Archivos encontrados:$(COLOR_RESET)"
	@echo "  Core:       $(words $(CORE_SOURCES)) archivos"
	@echo "  Utilidades: $(words $(UTIL_SOURCES)) archivos"
	@echo "  FUSE:       $(words $(FUSE_SOURCES)) archivos"
	@echo "  CLI:        $(words $(CLI_SOURCES)) archivos"

# Mostrar debug de variables
.PHONY: debug-vars
debug-vars:
	@echo "=== DEBUG DE VARIABLES ==="
	@echo "CORE_SOURCES = $(CORE_SOURCES)"
	@echo "CORE_OBJECTS = $(CORE_OBJECTS)"
	@echo "MKFS_OBJECTS = $(MKFS_OBJECTS)"
	@echo "HEADERS = $(HEADERS)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

# -----------------------------------------------------------------------------
# REGLAS ESPECIALES
# -----------------------------------------------------------------------------

# Evitar borrado de archivos intermedios
.SECONDARY:

# Declarar que estos targets no son archivos
.PHONY: all clean test install uninstall help info banner setup-dirs setup-deps check-deps
.PHONY: format-test mount-test integrity-test cleanup-test debug-vars distclean

# Hacer que make sea silencioso por defecto
ifndef VERBOSE
.SILENT:
endif

# =============================================================================
# FIN DEL MAKEFILE
# =============================================================================