#!/bin/sh

# Forward arguments to this script to a docker run.
docker run \
    -it \
    -v .:/mnt/host \
    -e GIT_COMMIT=$(git describe --dirty --always) \
    build-container:latest "$@"