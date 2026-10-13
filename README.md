
# BoxForge — A Minimal Container Runtime

BoxForge is an educational container runtime written in C for Linux.
It demonstrates process isolation using Linux namespaces and a
minimal root filesystem.

## Features

- PID, UTS, mount, and IPC namespaces
- Minimal container root filesystem
- BusyBox utilities
- Container command execution
- Process waiting and shutdown reporting
- Automated invalid-input tests

## Requirements

- Linux or a suitable Ubuntu/WSL environment
- GCC
- GNU Make
- BusyBox and required runtime libraries inside rootfs
- Permissions to create the required namespaces and mounts

## Build

    make

## Run

    sudo ./boxforge /bin/bash

Inside the container, try:

    echo $$
    hostname
    pwd
    ls /
    ps
    exit

## Run Automated Tests

    bash tests/test_boxforge.sh

## Project Structure

- src/main.c: main container runtime
- include/boxforge.h: shared declarations
- rootfs/: container filesystem
- tests/: automated tests
- docs/: architecture and testing documentation
- Makefile: build instructions

## Limitations

This is an educational prototype, not a production replacement
for Docker. Additional security hardening, cgroups v2 resource
limits, and networking require separate implementation and testing.

