#!/bin/bash

set -e

base_dir="$(dirname $(realpath $0))"

git submodule update --init

source /etc/os-release
if [ "$ID" = debian ]; then
    echo "Detected Debian Linux"
    sudo apt update
    #sudo apt install -y clang

    # GLFW dependencies.
    sudo apt install xorg-dev python3 python-is-python3

    # GLEW dependencies.
    sudo apt install libtool

elif [ "$ID" = "arch" ]; then
    echo "Detected Arch Linux"
    sudo pacman -Sy clang
fi

# Build GLFW library and headers.
cd glfw
cmake -S . -B build -D GLFW_BUILD_WAYLAND=OFF
cd build
make
cd "$base_dir"

# Build GLEW library and headers.
cd glew/auto
make
cd ..
make
cd "$base_dir"

# Build libglvnd library and headers.
cd libglvnd
./autogen.sh
./configure
sudo make install
cd "$base_dir"

# Build glm library and headers.
cd glm
cmake \
    -DGLM_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -B build .
cmake --build build -- all
