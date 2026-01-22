#!/usr/bin/env python3

from run_in_container import run_in_container

print(">>> Building engine demo")

# Build the engine demo.
run_in_container(["make", "all"])
