# Base off of Debian Bookworm release which uses GCC 12.
FROM debian:bookworm

# Install dependencies
RUN apt-get update && apt-get install -y \
    make \
    cmake \
    python3 \
    python-is-python3 \
    g++ \
    git \
    mesa-common-dev \
    zlib1g-dev \
    libminizip-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev

CMD exec bash

WORKDIR /mnt/host