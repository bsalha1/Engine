#!/usr/bin/env python3
import platform
import subprocess
from run_in_container import run, run_in_container

platform = platform.system()

print(f">>> Playing for platform: {platform}")

# Build the engine.
run_in_container(["make", "all"])

# Run the engine demo.

if platform == "Linux":
    run(["build/engine"])

elif platform == "Windows":
    run(["build\\engine.exe"])

else:
    print("Unsupported platform")
    exit(1)