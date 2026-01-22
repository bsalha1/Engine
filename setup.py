#!/usr/bin/env python3

import platform
import subprocess
from run_in_container import run

platform = platform.system()

print(f">>> Setting up repository for platform: {platform}")

run(["git", "submodule", "update", "--init"])

if platform == "Linux":
    # Build docker container.
    run(["docker", "build", "-t", "build-container", "--build-arg", "PLATFORM=linux", "."])

    # Get the distro.
    os_id = None
    with open("/etc/os-release") as f:
        for line in f:
            if line.startswith("ID="):
                os_id = line.strip().split("=")[1]
                break
    if os_id == None:
        print("Failed to detect Linux distribution.")

    print(f">>> Detected Linux distribution: {os_id}")

    if os_id == "debian":
        run(["sudo", "apt", "update"])
        run(["sudo", "apt", "install", "-y", "clang", "libglvnd-dev"])

    elif os_id == "arch":
        run(["sudo", "pacman", "-Sy", "clang", "libglvnd"])

    else:
        print(">>> Unsupported Linux distribution")
        exit(1)

elif platform == "Windows":
    # Build docker container.
    run(["docker", "build", "-t", "build-container", "--build-arg", "PLATFORM=windows", "."])

else:
    print(">>> Unsupported platform")
    exit(1)
