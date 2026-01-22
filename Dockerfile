# Base off of Debian Bookworm release which uses GCC 12.
FROM debian:bookworm

# Set the platform.
ARG PLATFORM
ENV PLATFORM="$PLATFORM"

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
        make \
        cmake \
        python3 \
        python-is-python3 \
        g++ \
        git && \
    if [ "$PLATFORM" = linux ]; then \
        apt-get install -y \
            mesa-common-dev \
            zlib1g-dev \
            libminizip-dev \
            libxrandr-dev \
            libxinerama-dev \
            libxcursor-dev \
            libxi-dev ; \
    elif [ "$PLATFORM" = windows ]; then \
        apt-get install -y \
            mingw-w64 \
            lld ; \
    fi

CMD exec bash

WORKDIR /mnt/host