#!/bin/bash

set -ex

base_dir="$(dirname $(realpath $0))"

# Clean source tree. Sudo used for submodules for libglvnd.
git clean -fxd
git submodule foreach --recursive sudo git clean -fxd

# Initialize submodules.
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

elif [ "$ID" = arch ]; then
    echo "Detected Arch Linux"
    sudo pacman -Sy clang
fi

# Set build environment.
export CMAKE_BUILD_PARALLEL_LEVEL=$(nproc)
export MAKEFLAGS="-j$(nproc)"

# Build GLFW library.
cd "$base_dir/glfw"
cmake -S . -B build -D GLFW_BUILD_WAYLAND=OFF
cd build
make

# Build GLEW library.
cd "$base_dir/glew/auto"
make
cd ..
make

# Build libglvnd library.
cd "$base_dir/libglvnd"
./autogen.sh
./configure
sudo make install

# Build glm library.
cd "$base_dir/glm"
cmake \
    -DGLM_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -B build .
cmake --build build -- all

# Build stb_image library.
cd "$base_dir/stb"
make clean
make

# Build imgui library.
cd "$base_dir/imgui"
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

# Build assimp library.
cd "$base_dir/assimp"
cmake CMakeLists.txt -DBUILD_SHARED_LIBS=OFF
cmake --build .