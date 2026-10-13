#define _GNU_SOURCE

#include "boxforge.h"
#include "cgroups.h"

#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 * 1024)

struct launch_config {
    struct container_config container;
    int start_read_fd;
    int start_write_fd;
};

/* Pivot root helper function */
static int perform_pivot_root(const char *new_root) {
    char old_root[PATH_MAX];
    snprintf(old_root, sizeof(old_root), "%s/.old_root", new_root);

    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("BoxForge: bind mount new_root");
        return -1;
    }

    if (mkdir(old_root, 0700) == -1 && errno != EEXIST) {
        perror("BoxForge: mkdir .old_root");
        return -1;
    }

    if (syscall(SYS_pivot_root, new_root, old_root) == -1) {
        perror("BoxForge: pivot_root");
        return -1;
    }

    if (chdir("/") == -1) {
        perror("BoxForge: chdir /");
        return -1;
    }

    if (umount2("/.old_root", MNT_DETACH) == -1) {
        perror("BoxForge: umount old root");
        return -1;
    }

    rmdir("/.old_root");
    return 0;
}

/* Signal handler for PID 1 to reap zombie children */
static void sigchld_reaper(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* Runs inside the isolated container namespaces */
int child_main(void *arg) {
    struct launch_config *launch = arg;
    struct container_config *config = &launch->container;
    char start_signal;

    close(launch->start_write_fd);

    ssize_t bytes_read;
    do {
        bytes_read = read(launch->start_read_fd, &start_signal, 1);
    } while (bytes_read == -1 && errno == EINTR);

    close(launch->start_read_fd);

    if (bytes_read != 1) {
        fprintf(stderr, "BoxForge: Failed to receive startup signal\n");
        return 1;
    }

    // Set reap handler for child processes
    signal(SIGCHLD, sigchld_reaper);

    if (sethostname("boxforge", 8) == -1) {
        perror("BoxForge: sethostname");
        return 1;
    }

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("BoxForge: mount private");
        return 1;
    }

    // Mount OverlayFS using lower, upper, and work directories
    char mount_opts[1024];
    snprintf(mount_opts, sizeof(mount_opts),
             "lowerdir=%s,upperdir=%s,workdir=%s",
             config->rootfs, config->upper_dir, config->work_dir);

    if (mount("overlay", config->merged_dir, "overlay", 0, mount_opts) == -1) {
        perror("BoxForge: mount overlay");
        return 1;
    }

    // Perform pivot_root into merged OverlayFS mount
    if (perform_pivot_root(config->merged_dir) == -1) {
        fprintf(stderr, "BoxForge: Failed pivot_root\n");
        return 1;
    }

    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        perror("BoxForge: mkdir /proc");
        return 1;
    }

    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1) {
        perror("BoxForge: mount /proc");
        return 1;
    }

    printf("BoxForge: Starting container\n");
    fflush(stdout);

    execv(config->command[0], config->command);

    perror("BoxForge: execv");
    return 127;
}

/* Attach process to active container namespaces */
static int handle_exec(pid_t target_pid, char **cmd) {
    const char *namespaces[] = { "ipc", "uts", "net", "pid", "mnt" };
    char ns_path[256];

    for (int i = 0; i < 5; i++) {
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", target_pid, namespaces[i]);
        int fd = open(ns_path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "BoxForge: Cannot open namespace %s for PID %d: %s\n",
                    namespaces[i], target_pid, strerror(errno));
            return 1;
        }
        if (setns(fd, 0) == -1) {
            fprintf(stderr, "BoxForge: Failed setns for %s: %s\n",
                    namespaces[i], strerror(errno));
            close(fd);
            return 1;
        }
        close(fd);
    }

    execv(cmd[0], cmd);
    perror("BoxForge: execv failed");
    return 127;
}

static void print_help(const char *program) {
    printf("BoxForge - Minimal Container Runtime\n\n");
    printf("Usage:\n");
    printf("  %s <command> [args...]\n", program);
    printf("  %s run <rootfs> <command> [args...]\n", program);
    printf("  %s exec <container_pid> <command> [args...]\n", program);
    printf("  %s --help\n\n", program);
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_help(argv[0]);
        return 0;
    }

    // Support: ./boxforge exec <PID> <command>
    if (argc >= 4 && strcmp(argv[1], "exec") == 0) {
        pid_t target_pid = atoi(argv[2]);
        if (target_pid <= 0) {
            fprintf(stderr, "Invalid container PID: %s\n", argv[2]);
            return 1;
        }
        return handle_exec(target_pid, &argv[3]);
    }

    char *rootfs_input = "rootfs";
    int command_index = 1;

    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s run <rootfs> <command> [args...]\n", argv[0]);
            return 1;
        }
        rootfs_input = argv[2];
        command_index = 3;
    } else if (argc > 1 && strcmp(argv[1], "--rootfs") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s --rootfs <rootfs> <command> [args...]\n", argv[0]);
            return 1;
        }
        rootfs_input = argv[2];
        command_index = 3;
    }

    if (argc <= command_index) {
        print_help(argv[0]);
        return 1;
    }

    if (argv[command_index][0] != '/') {
        fprintf(stderr, "Error: command must be an absolute path inside rootfs\n");
        return 1;
    }

    char *rootfs = realpath(rootfs_input, NULL);
    if (rootfs == NULL) {
        perror("BoxForge: root filesystem");
        return 1;
    }

    // Set up workspace directories for OverlayFS
    char upper_dir[] = "/tmp/boxforge_upper_XXXXXX";
    char work_dir[]  = "/tmp/boxforge_work_XXXXXX";
    char merged_dir[] = "/tmp/boxforge_merged_XXXXXX";

    if (!mkdtemp(upper_dir) || !mkdtemp(work_dir) || !mkdtemp(merged_dir)) {
        perror("BoxForge: mkdtemp failed");
        free(rootfs);
        return 1;
    }

    struct launch_config launch = {
        .container = {
            .rootfs = rootfs,
            .command = &argv[command_index],
            .upper_dir = upper_dir,
            .work_dir = work_dir,
            .merged_dir = merged_dir
        },
        .start_read_fd = -1,
        .start_write_fd = -1
    };

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("BoxForge: pipe");
        free(rootfs);
        return 1;
    }

    launch.start_read_fd = pipe_fds[0];
    launch.start_write_fd = pipe_fds[1];

    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("BoxForge: malloc stack");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        free(rootfs);
        return 1;
    }

    printf("BoxForge: Creating isolated environment...\n");

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC | SIGCHLD;

    pid_t child_pid = clone(child_main, stack + STACK_SIZE, flags, &launch);

    if (child_pid == -1) {
        perror("BoxForge: clone");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        free(stack);
        free(rootfs);
        return 1;
    }

    close(pipe_fds[0]);
    printf("BoxForge: Container process started (host PID %d)\n", child_pid);

    if (cgroup_create(child_pid) == -1) {
        fprintf(stderr, "BoxForge: Could not apply cgroup limits; stopping container.\n");
        kill(child_pid, SIGKILL);
        close(pipe_fds[1]);
        while (waitpid(child_pid, NULL, 0) == -1 && errno == EINTR);
        cgroup_cleanup();
        free(stack);
        free(rootfs);
        return 1;
    }

    char start_signal = '1';
    ssize_t bytes_written;
    do {
        bytes_written = write(pipe_fds[1], &start_signal, 1);
    } while (bytes_written == -1 && errno == EINTR);

    close(pipe_fds[1]);

    if (bytes_written != 1) {
        perror("BoxForge: startup signal");
        kill(child_pid, SIGKILL);
        while (waitpid(child_pid, NULL, 0) == -1 && errno == EINTR);
        cgroup_cleanup();
        free(stack);
        free(rootfs);
        return 1;
    }

    int status;
    pid_t result;
    do {
        result = waitpid(child_pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    cgroup_cleanup();

    // Clean up temporary OverlayFS folders on container termination
    umount2(merged_dir, MNT_DETACH);
    rmdir(merged_dir);
    rmdir(work_dir);
    rmdir(upper_dir);

    if (result == -1) {
        perror("BoxForge: waitpid");
        free(stack);
        free(rootfs);
        return 1;
    }

    printf("BoxForge: Container stopped\n");

    free(stack);
    free(rootfs);

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);

    return 1;
}
