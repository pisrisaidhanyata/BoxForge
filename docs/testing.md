
# BoxForge Testing

## 1. Build Test

Command:

    make clean
    make

Expected result: The C program compiles into the boxforge executable
without compiler errors.

## 2. Invalid Input Tests

Command:

    bash tests/test_boxforge.sh

Tests:
- Reject missing command-line arguments.
- Reject a relative command path.
- Reject a nonexistent root filesystem.

Expected result: All three tests pass.

## 3. Manual Container Test

Command:

    sudo ./boxforge /bin/bash

Inside the container, run:

    echo $$
    hostname
    pwd
    ls /
    ps
    exit

Expected observations:
- The shell reports PID 1.
- The hostname is boxforge.
- The working directory is /.
- The container root directories are visible.
- The process list shows the container's process view.
- Exiting the shell returns control to the host and prints
  the container-stopped message.

## 4. Test Scope

These tests demonstrate basic execution and input validation.
They do not prove complete security isolation or CPU and memory
resource limiting.
