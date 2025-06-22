# Makefile para Black & White Filesystem
#
# Uso básico:
#   make              - Compila mkfs.bwfs y fsck.bwfs (no requiere FUSE)
#   make all-with-fuse - Compila todo incluyendo mount.bwfs (requiere FUSE)
#   make test         - Crea un filesystem de prueba
#   make clean        - Limpia archivos compilados
#
# Si no tienes pkg-config instalado, ejecuta primero:
#   bash install_dependencies.sh

CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -D_DEFAULT_SOURCE -D_GNU_SOURCE
LDFLAGS = -lcrypto -lssl -lm

# Detectar si pkg-config está disponible
HAS_PKGCONFIG := $(shell which pkg-config 2>/dev/null)

# Detectar si estamos en Linux o macOS para FUSE
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    ifdef HAS_PKGCONFIG
        FUSE_CFLAGS := $(shell pkg-config fuse --cflags 2>/dev/null)
        FUSE_LIBS := $(shell pkg-config fuse --libs 2>/dev/null)
    endif
    # Fallback si no hay pkg-config
    ifeq ($(FUSE_LIBS),)
        FUSE_LIBS = -lfuse -lpthread
    endif
else ifeq ($(UNAME_S),Darwin)
    FUSE_CFLAGS = -I/usr/local/include/osxfuse
    FUSE_LIBS = -L/usr/local/lib -losxfuse
endif

# Objetivos
TARGETS = mkfs.bwfs fsck.bwfs
FUSE_TARGETS = mount.bwfs
OBJS = bwfs_common.o

all: $(TARGETS)

all-with-fuse: $(TARGETS) $(FUSE_TARGETS)

# Archivo común con funciones compartidas
bwfs_common.o: bwfs_common.c bwfs_common.h
	$(CC) $(CFLAGS) -c bwfs_common.c -o bwfs_common.o

# mkfs.bwfs
mkfs.bwfs: mkfs.bwfs.c bwfs_common.o
	$(CC) $(CFLAGS) mkfs.bwfs.c bwfs_common.o -o mkfs.bwfs $(LDFLAGS)

# fsck.bwfs
fsck.bwfs: fsck.bwfs.c bwfs_common.o
	$(CC) $(CFLAGS) fsck.bwfs.c bwfs_common.o -o fsck.bwfs $(LDFLAGS)

# mount.bwfs (incluirá FUSE)
mount.bwfs: mount.bwfs.c bwfs_common.o bwfs_ops.o
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) mount.bwfs.c bwfs_common.o bwfs_ops.o -o mount.bwfs $(LDFLAGS) $(FUSE_LIBS)

# Operaciones FUSE
bwfs_ops.o: bwfs_ops.c bwfs_ops.h bwfs_common.h
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c bwfs_ops.c -o bwfs_ops.o

# Limpiar
clean:
	rm -f $(TARGETS) $(FUSE_TARGETS) *.o
	rm -rf test_fs/

# Prueba rápida
test: mkfs.bwfs
	mkdir -p test_fs
	./mkfs.bwfs test_fs

# Instalar (requiere sudo)
install: $(TARGETS)
	sudo cp $(TARGETS) /usr/local/bin/
	sudo chmod +x /usr/local/bin/mkfs.bwfs
	sudo chmod +x /usr/local/bin/fsck.bwfs

install-all: $(TARGETS) $(FUSE_TARGETS)
	sudo cp $(TARGETS) $(FUSE_TARGETS) /usr/local/bin/
	sudo chmod +x /usr/local/bin/mkfs.bwfs
	sudo chmod +x /usr/local/bin/fsck.bwfs
	sudo chmod +x /usr/local/bin/mount.bwfs

uninstall:
	sudo rm -f /usr/local/bin/mkfs.bwfs
	sudo rm -f /usr/local/bin/fsck.bwfs
	sudo rm -f /usr/local/bin/mount.bwfs

.PHONY: all all-with-fuse clean test install install-all uninstall