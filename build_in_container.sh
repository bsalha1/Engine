#!/bin/sh

# Build the engine inside the container.
./execute_in_container.sh make all && echo "Created executable build/engine"