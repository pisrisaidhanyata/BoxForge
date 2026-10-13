
# BoxForge Architecture

## 1. Project Overview

BoxForge is a minimal container runtime written in C for Linux.
It demonstrates how operating-system features can isolate a process
and provide a separate view of the filesystem.

## 2. Architecture

The main execution flow is:

User Command
    |
    v
BoxForge Main Program
    |
    v
Create Child Process with clone()
    |
    v
PID, UTS, Mount and IPC Namespaces
    |
    v
Set up Container Root Filesystem
    |
    v
Mount /proc
    |
    v
Execute Requested Command
    |
    v
Wait for Process and Report Shutdown

## 3. Main Components

### src/main.c
Parses command-line arguments, creates the child process, configures
namespaces and the root filesystem, executes the requested command,
waits for the child, and releases allocated memory.

### include/boxforge.h
Contains shared configuration structures and constants used by
the runtime.

### rootfs/
Contains the minimal filesystem used by the container, including
BusyBox, required libraries, and directories such as /proc and /tmp.

### tests/
Contains automated tests for invalid command-line inputs.

### docs/
Contains architecture, testing, and project documentation.

## 4. Operating-System Concepts

- PID namespace: gives the container its own process ID view.
- UTS namespace: allows an isolated hostname.
- Mount namespace: provides a separate mount view.
- IPC namespace: isolates System V IPC resources.
- chroot(): changes the apparent root directory.
- execv(): runs the requested command.
- waitpid(): waits for the container process to finish.

## 5. Current Limitations and Future Work

The current prototype demonstrates basic process and filesystem
isolation. Stronger security, cgroups v2 CPU and memory limits,
network configuration, and additional automated isolation tests
remain future work until implemented and verified.
