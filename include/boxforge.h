#ifndef BOXFORGE_H
#define BOXFORGE_H

#define _GNU_SOURCE

#include <sys/types.h>

#define BOXFORGE_DEFAULT_ROOTFS "rootfs"
#define BOXFORGE_HOSTNAME "boxforge"
#define BOXFORGE_STACK_SIZE (1024 * 1024)

struct container_config {
    char *rootfs;
    char **command;
    char *upper_dir;
    char *work_dir;
    char *merged_dir;
};

int child_main(void *arg);

#endif
