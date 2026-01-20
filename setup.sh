#!/bin/bash

set -ex

# Initialize submodules.
git submodule update --init

# Detect OS and install the dependencies required by the repository and engine runtime.
source /etc/os-release
if [ "$ID" = debian ]; then
    echo "Detected Debian Linux"

    sudo apt update
    sudo apt install -y clang libglvnd-dev

elif [ "$ID" = arch ]; then
    echo "Detected Arch Linux"

    sudo pacman -Sy clang libglvnd
fi
