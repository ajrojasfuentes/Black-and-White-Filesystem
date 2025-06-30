#!/bin/bash
# setup_deps.sh - Configurar dependencias para BWFS

set -e

echo "=== Configurando dependencias para BWFS ==="

# Crear directorio para headers externos
mkdir -p include/external

# Descargar stb headers
echo "Descargando stb_image.h..."
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
     -o include/external/stb_image.h

echo "Descargando stb_image_write.h..."
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h \
     -o include/external/stb_image_write.h

echo "✅ Dependencias instaladas correctamente"
echo ""
echo "ESTRUCTURA ACTUALIZADA:"
echo "include/"
echo "├── external/"
echo "│   ├── stb_image.h"
echo "│   └── stb_image_write.h"
echo "└── ..."
echo ""
echo "SIGUIENTE PASO:"
echo "1. Actualizar #include en src/util/png_io.c:"
echo "   #include \"external/stb_image.h\""
echo "   #include \"external/stb_image_write.h\""
echo ""
echo "2. Actualizar Makefile para incluir ruta:"
echo "   CFLAGS += -Iinclude/external"