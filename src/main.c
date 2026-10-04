#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define STACK_SIZE (1024 * 1024)

char child_stack[STACK_SIZE];

char **child_args;

int child_main(void *arg)
{
    printf("BoxForge: Container started\n");
    printf("Container PID: %d\n", getpid());

    /* Set container hostname */
    if (sethostname("boxforge", 8) == -1)
    {
        perror("sethostname");
    }

    /* Make mount namespace private */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1)
    {
        perror("mount");
    }

    /* Change root filesystem */
    if (chroot("./rootfs") == -1)
    {
        perror("chroot");
        return 1;
    }

    /* Move into new root */
    if (chdir("/") == -1)
    {
        perror("chdir");
        return 1;
    }
if (mount("proc", "/proc", "proc", 0, NULL) == -1)
{
    perror("mount /proc");
}

    printf("Filesystem isolation enabled\n");

    /* Run requested command */
    execvp(child_args[0], child_args);

    perror("execvp");

    return 1;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: sudo ./boxforge <command> [args...]\n");
        return 1;
    }

    child_args = &argv[1];

    int flags =
        CLONE_NEWPID |
        CLONE_NEWUTS |
        CLONE_NEWNS |
        CLONE_NEWIPC |
        CLONE_NEWNET |
        SIGCHLD;

    pid_t pid = clone(
        child_main,
        child_stack + STACK_SIZE,
        flags,
        NULL
    );

    if (pid == -1)
    {
        perror("clone");
        return 1;
    }

    printf("BoxForge: Created container process PID %d\n", pid);

    waitpid(pid, NULL, 0);

    printf("BoxForge: Container stopped\n");

    return 0;
}
