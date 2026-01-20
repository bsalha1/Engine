# Overview

This repository contains the source code for a game / graphics engine that serves to help me learn how to do graphics and game programming. For now, it will be rendered with OpenGL.

# Build

Currently, the engine only builds for Linux.

The build takes place inside a Docker container so the build is reproducible. However, the engine is executed outside of the container, so one must still install some system-level dependencies that are linked to in runtime. It is also possible to not use Docker to build by invoking `make` outside of the container, but you are responsible for making your build work.

**1.** Install system-level dependencies and initialize the repository:
```
./setup.sh
```
<br>

**2.** Build Docker container:
```
./build_container.sh
```
<br>

**3.** Build inside Docker container:
```
./build_in_docker.sh
```
or equivalently:
```
./execute_in_container.sh make all
```
<br>

**4.** A binary called `engine` will be produced in the `build/` directory. To play the demo, execute:
```
./build/engine
```
<br>

To view other targets:
```
./execute_in_container.sh make help
```
<br>

To build and play from the current terminal (helpful for quick iteration):
```
./play.sh
```
