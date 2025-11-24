#!/bin/bash

set -e

sudo pacman -Sy clang-format

# Build GLFW library and headers.
cd glfw
cmake -S . -B build
cd build
make

# Build GLEW library and headers.
cd glew/auto
make
cd ..
make

# Build libglvnd library and headers.
cd libglvnd
./autogen.sh
./configure
sudo make install
