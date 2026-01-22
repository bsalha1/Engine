#!/usr/bin/env python3
import platform
import subprocess
import sys

def run(cmd):
    print(">>>", " ".join(cmd))
    subprocess.check_call(cmd)

def run_in_container(cmd):
    git_commit = subprocess.check_output(["git", "describe", "--dirty", "--always"]).decode().strip()
    run(["docker", "run",
        "-it",
        "-v", ".:/mnt/host",
        "-e", f"GIT_COMMIT={git_commit}",
        "build-container:latest"] + cmd)

# If this script is being executed, make it run the arguments in the docker container.
if __name__ == "__main__":
    if len(sys.argv) == 1:
        prog_name = sys.argv[0]
        print(f"{prog_name} <command to execute>")
        exit(1)

    run_in_container(sys.argv[1:])