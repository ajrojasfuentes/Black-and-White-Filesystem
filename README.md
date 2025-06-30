# Black-and-White-Filesystem
Proyecto 2 - Sistemas Operativos - S1 2025

# Guía de Uso del Makefile BWFS

## 🚀 **Inicio Rápido**

```bash
# 1. Configurar dependencias
make setup-deps

# 2. Verificar dependencias del sistema
make check-deps

# 3. Compilar todo
make

# 4. Ejecutar pruebas
make test
```

## 🔧 **Configuración de Build**

### **Modos de Compilación:**

```bash
# Modo Debug (predeterminado) - Con símbolos y sanitizers
make BUILD_TYPE=debug

# Modo Release - Optimizado para producción
make BUILD_TYPE=release

# Modo Coverage - Para análisis de cobertura
make BUILD_TYPE=coverage
```

### **Variables de Configuración:**

```bash
# Cambiar directorio de instalación
make install PREFIX=/opt/bwfs

# Compilación verbose
make VERBOSE=1

# Usar clang en lugar de gcc
make CC=clang
```

## 🧪 **Testing Completo**

### **Pruebas Individuales:**

```bash
# Prueba de formateo
make format-test

# Prueba de montaje y operaciones básicas
make mount-test

# Prueba de integridad
make integrity-test

# Prueba de reparación automática
make repair-test

# Prueba de estrés (muchos archivos)
make stress-test
```

### **Suite Completa:**

```bash
# Ejecutar todas las pruebas
make test

# Ejecutar pruebas con limpieza automática
make test cleanup-test
```

## 🔍 **Análisis de Calidad**

### **Análisis Estático:**

```bash
# Requiere cppcheck instalado
sudo apt-get install cppcheck
make static-analysis
```

### **Análisis de Memoria:**

```bash
# Requiere valgrind instalado
sudo apt-get install valgrind
make memcheck
```

### **Cobertura de Código:**

```bash
# Requiere gcov instalado
sudo apt-get install gcov
make coverage BUILD_TYPE=coverage
```

## 📦 **Instalación y Distribución**

### **Instalar en el Sistema:**

```bash
# Instalación estándar
sudo make install

# Instalación personalizada
sudo make install PREFIX=/usr/local
```

### **Crear Paquete de Distribución:**

```bash
make dist
# Genera: dist/bwfs-1.0.0.tar.gz
```

### **Desinstalar:**

```bash
sudo make uninstall
```

## 🛠️ **Desarrollo**

### **Limpieza:**

```bash
# Limpieza básica
make clean

# Limpieza completa (incluye dependencias)
make distclean
```

### **Información del Proyecto:**

```bash
# Ver información detallada
make info

# Ver ayuda completa
make help
```

## 📊 **Outputs y Resultados**

### **Archivos Generados:**

```
bin/
├── mkfs_bwfs     # Formateador
├── fsck_bwfs     # Verificador
└── mount_bwfs    # Montador

obj/              # Archivos objeto
├── cli/
├── core/
├── util/
└── fuse/

coverage/         # Reportes de cobertura
*.log            # Logs de análisis
```

### **Códigos de Salida:**

- `0`: Éxito
- `1`: Error de compilación
- `2`: Error de dependencias
- `3`: Error en pruebas

## 🎯 **Casos de Uso Comunes**

### **Desarrollo Diario:**

```bash
# Compilar y probar rápidamente
make clean && make && make test
```

### **Preparar Release:**

```bash
# Compilación optimizada con todas las verificaciones
make clean
make BUILD_TYPE=release
make static-analysis
make test
make dist
```

### **Debugging:**

```bash
# Compilar con debug y verificar memoria
make BUILD_TYPE=debug
make memcheck
```

### **Análisis de Cobertura:**

```bash
# Generar reporte completo
make clean
make test BUILD_TYPE=coverage
make coverage
```

## ⚠️ **Solución de Problemas**

### **Errores Comunes:**

1. **FUSE3 no encontrado:**
   ```bash
   sudo apt-get install libfuse3-dev
   ```

2. **stb_image no encontrado:**
   ```bash
   make setup-deps
   ```

3. **Permisos insuficientes:**
   ```bash
   sudo make install
   ```

4. **Filesystem ocupado:**
   ```bash
   make cleanup-test
   ```

### **Verificación de Configuración:**

```bash
# Verificar todo está correcto
make check-deps
make info
```

## 📈 **Métricas de Calidad**

El Makefile genera automáticamente:

- **Cobertura de código**: `coverage/`
- **Análisis estático**: `static-analysis.log`
- **Verificación de memoria**: `memcheck.log`
- **Tiempo de compilación**: Mostrado en pantalla
- **Éxito de pruebas**: Código de salida

## 🎨 **Personalización**

### **Modificar Flags de Compilación:**

```bash
# Agregar flags personalizados
make CFLAGS="-Wall -Wextra -Werror"
```

### **Cambiar Directorios de Prueba:**

```bash
# Usar directorios personalizados
make test TEST_FS_DIR=/tmp/mi_fs TEST_MOUNT_DIR=/tmp/mi_mount
```

Este Makefile está diseñado para ser **robusto, flexible y fácil de usar** tanto para desarrollo como para producción.