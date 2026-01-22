# Table of Contents
- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Build](#build)
  - [Principle](#principle)
  - [Procedure](#procedure)
- [Project Structure](#project-structure)

# Overview

This repository contains the source code for a game / graphics engine that serves to help me learn how to do graphics and game programming. For now, it will be rendered with OpenGL.

# Build

## Principle

The build system is aimed at reproducibility and sandboxing. As such, the build takes place inside a Docker container with all the necessary dependencies installed, see [Dockerfile](Dockerfile). The container is only capable of building for Linux and Windows for now - don't have any plans for Mac since I don't have a Mac.

There are a few thin wrappers around commands executed in the container for convenience:
- [build.py](build.py): Build the engine demo
- [play.py](play.py): Build and execute the engine demo
- [format.py](format.py): Format the source code

## Procedure

**1.** Install system-level dependencies and initialize the repository:
```
python setup.py
```
<br>

**2.** Play demo:
```
python play.py
```
<br>

One can execute arbitrary commands in the container via:
```
python run_in_container.py <commands>
```
i.e.
```
python run_in_container.py make clean
```

# Project Structure

- Entry point: [src/main.cc](src/main.cc)