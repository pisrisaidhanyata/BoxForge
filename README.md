# BoxForge

## A Minimal Container Runtime

BoxForge is a minimal Linux container runtime implemented in C.
It demonstrates operating system concepts such as process isolation,
Linux namespaces and filesystem isolation.

## Objectives

- Create an isolated Linux process
- Demonstrate PID namespace isolation
- Demonstrate UTS namespace isolation
- Provide a separate root filesystem
- Run programs inside the isolated environment

## Technologies

- Ubuntu / WSL
- C
- GCC
- Linux system calls
- Linux namespaces
- BusyBox

## Compile

```bash
gcc -std=c11 src/main.c -o boxforge
