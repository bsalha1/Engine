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

# Build GLFW library.
cd glfw
cmake -S . -B build -D GLFW_BUILD_WAYLAND=OFF
cd build
make
cd "$base_dir"

# Build GLEW library.
cd glew/auto
make
cd ..
make
cd "$base_dir"

# Build libglvnd library.
cd libglvnd
./autogen.sh
./configure
sudo make install
cd "$base_dir"

# Build glm library.
cd glm
cmake \
    -DGLM_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -B build .
cmake --build build -- all
cd "$base_dir"

# Build stb_image library.
cd stb
make
cd "$base_dir"

# Build imgui library.
cd imgui
g++ \
    -c \
    -I. \
    -I../glfw/include \
    backends/imgui_impl_opengl3.cpp \
    backends/imgui_impl_glfw.cpp \
    imgui_draw.cpp \
    imgui_tables.cpp \
    imgui_widgets.cpp \
    imgui.cpp \
    imgui_demo.cpp

ar rcs libimgui.a *.o
