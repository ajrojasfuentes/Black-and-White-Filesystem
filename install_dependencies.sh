#!/usr/bin/env bash
# Instalador de dependencias para BWFS (FUSE 3)

echo "=== Instalador de dependencias para BWFS ==="
echo

# ---------- Funciones auxiliares ---------- #
msg()   { printf '\e[1;36m%s\e[0m\n' "$*"; }
die()   { printf '\e[1;31mERROR:\e[0m %s\n' "$*" ; exit 1; }

# ---------- Detección de plataforma ---------- #
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if   command -v apt-get  &>/dev/null; then  distro="debian"  # Ubuntu / Debian
    elif command -v dnf      &>/dev/null; then  distro="dnf"     # Fedora / RHEL≥8
    elif command -v yum      &>/dev/null; then  distro="yum"     # CentOS 7
    elif command -v pacman   &>/dev/null; then  distro="arch"    # Arch / Manjaro
    else
        die "Distribución Linux no reconocida. Instala manualmente:\n  - build-essential / base-devel\n  - pkg-config\n  - openssl-dev\n  - fuse3 + libfuse3-dev\n  - libfuse2"
    fi

    case "$distro" in
    debian)
        msg "Detectado sistema Debian/Ubuntu"
        sudo apt-get update
        # En Ubuntu 22.04+ la libfuse 2 lleva sufijo t64
        libfuse2_pkg=$(apt-cache policy libfuse2t64 &>/dev/null && echo libfuse2t64 || echo libfuse2)
        sudo apt-get install -y \
            build-essential pkg-config libssl-dev \
            fuse3 libfuse3-dev "$libfuse2_pkg"
        ;;
    dnf)
        msg "Detectado sistema Fedora / RHEL (dnf)"
        sudo dnf -y install \
            @development-tools pkgconf-pkg-config openssl-devel \
            fuse3 fuse3-devel fuse-libs          # fuse-libs = libfuse2
        ;;
    yum)
        msg "Detectado sistema CentOS 7 (yum)"
        sudo yum -y install \
            gcc gcc-c++ make pkgconfig openssl-devel \
            fuse3 fuse3-devel fuse-libs
        ;;
    arch)
        msg "Detectado sistema Arch Linux / Manjaro"
        sudo pacman -Sy --needed --noconfirm \
            base-devel pkgconf openssl \
            fuse3 fuse2                          # fuse2 = libfuse2
        ;;
    esac

elif [[ "$OSTYPE" == "darwin"* ]]; then
    msg "Detectado sistema macOS"
    if command -v brew &>/dev/null; then
        brew install pkg-config openssl@3 macfuse
    else
        die "Homebrew no encontrado. Instálalo primero:\n  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    fi
else
    die "Sistema operativo no soportado: $OSTYPE"
fi

echo
msg "=== Instalación completada ==="
echo "Ahora puedes compilar BWFS con:  make"
