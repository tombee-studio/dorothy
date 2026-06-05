#!/bin/sh
set -e

OS="$(uname -s)"
echo "[install-deps] OS: $OS"

install_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "[install-deps] Homebrew is required: https://brew.sh"
        exit 1
    fi
    echo "[install-deps] Installing via Homebrew..."
    brew install googletest llvm pkg-config mingw-w64
}

install_debian() {
    echo "[install-deps] Installing via apt-get..."
    sudo apt-get update -qq
    sudo apt-get install -y build-essential pkg-config libgtest-dev clang llvm cmake mingw-w64

    # libgtest-dev on older Ubuntu installs sources only; build them if needed
    if ! pkg-config --exists gtest 2>/dev/null; then
        echo "[install-deps] Building gtest from source..."
        GTEST_SRC=""
        for d in /usr/src/googletest /usr/src/gtest; do
            [ -d "$d" ] && GTEST_SRC="$d" && break
        done
        if [ -n "$GTEST_SRC" ]; then
            cmake -S "$GTEST_SRC" -B "$GTEST_SRC/build" -DCMAKE_BUILD_TYPE=Release
            sudo cmake --build "$GTEST_SRC/build" --target install
        else
            echo "[install-deps] gtest source not found; pkg-config may not work"
        fi
    fi
}

install_fedora() {
    echo "[install-deps] Installing via dnf..."
    sudo dnf install -y gcc-c++ make pkgconf-pkg-config gtest-devel clang llvm mingw64-gcc
}

install_rhel() {
    echo "[install-deps] Installing via yum..."
    sudo yum install -y gcc-c++ make pkgconfig gtest-devel clang llvm mingw64-gcc
}

install_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        install_debian
    elif command -v dnf >/dev/null 2>&1; then
        install_fedora
    elif command -v yum >/dev/null 2>&1; then
        install_rhel
    else
        echo "[install-deps] Unknown Linux distribution."
        echo "Please install: g++, make, libgtest-dev, pkg-config, clang, llvm"
        exit 1
    fi
}

case "$OS" in
    Darwin) install_macos ;;
    Linux)  install_linux ;;
    *)
        echo "[install-deps] Unsupported OS: $OS"
        exit 1
        ;;
esac

echo "[install-deps] Done."
