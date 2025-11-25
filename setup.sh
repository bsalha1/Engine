#!/bin/bash

set -e

base_dir="$(dirname $(realpath $0))"
sudo pacman -Sy clang

# Build GLFW library and headers.
cd glfw
cmake -S . -B build
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
