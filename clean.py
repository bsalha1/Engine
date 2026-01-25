#!/usr/bin/env python3

from run_in_container import run_in_container

print(">>> Cleaning")

run_in_container(["make", "clean"])
