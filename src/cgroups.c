
#define _GNU_SOURCE

#include "cgroups.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MEMORY_LIMIT "536870912\n"
#define CPU_LIMIT    "50000 100000\n"

#define CGROUP_ROOT "/sys/fs/cgroup"

static char cgroup_path[512];

/* Write a value into a cgroup control file. */
static int write_setting(const char *filename, const char *value)
{
    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        fprintf(stderr, "BoxForge: Cannot open %s: %s\n",
                filename, strerror(errno));
        return -1;
    }

    if (fputs(value, file) == EOF) {
        int saved_errno = errno;
        fclose(file);
        fprintf(stderr, "BoxForge: Cannot write %s: %s\n",
                filename, strerror(saved_errno));
        return -1;
    }

    if (fclose(file) == EOF) {
        fprintf(stderr, "BoxForge: Cannot close %s: %s\n",
                filename, strerror(errno));
        return -1;
    }

    return 0;
}

/* Resolve the original user when BoxForge is launched using sudo. */
static int get_original_uid(unsigned long *uid)
{
    const char *value = getenv("SUDO_UID");
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || *value == '\0') {
        *uid = (unsigned long)getuid();
        return 0;
    }

    errno = 0;
    parsed = strtoul(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0') {
        fprintf(stderr, "BoxForge: Invalid SUDO_UID\n");
        return -1;
    }

    *uid = parsed;
    return 0;
}

/* Create a resource-limited cgroup and move the container process into it. */
int cgroup_create(pid_t pid)
{
    char parent[512];
    char setting[600];
    char pid_text[32];
    char controllers[256];
    FILE *file;
    unsigned long uid;
    struct stat st;

    cgroup_path[0] = '\0';

    if (get_original_uid(&uid) == -1)
        return -1;

    if (snprintf(parent, sizeof(parent),
                 CGROUP_ROOT "/user.slice/user-%lu.slice/"
                 "user@%lu.service/app.slice",
                 uid, uid) >= (int)sizeof(parent)) {
        fprintf(stderr, "BoxForge: Parent cgroup path is too long\n");
        return -1;
    }

    if (stat(parent, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr,
                "BoxForge: User cgroup directory is unavailable: %s\n",
                parent);
        fprintf(stderr,
                "BoxForge: Check WSL2 cgroup delegation and permissions.\n");
        return -1;
    }

    /* Confirm CPU and memory controllers are available. */
    snprintf(setting, sizeof(setting), "%s/cgroup.controllers", parent);
    file = fopen(setting, "r");

    if (file == NULL) {
        fprintf(stderr, "BoxForge: Cannot read %s: %s\n",
                setting, strerror(errno));
        return -1;
    }

    if (fgets(controllers, sizeof(controllers), file) == NULL) {
        fclose(file);
        fprintf(stderr, "BoxForge: Cannot read cgroup controllers\n");
        return -1;
    }

    fclose(file);

    if (strstr(controllers, "cpu") == NULL ||
        strstr(controllers, "memory") == NULL) {
        fprintf(stderr,
                "BoxForge: CPU or memory controller is unavailable\n");
        return -1;
    }

    /* Enable controllers for child cgroups if not already enabled. */
    snprintf(setting, sizeof(setting),
             "%s/cgroup.subtree_control", parent);

    file = fopen(setting, "r");
    if (file == NULL) {
        fprintf(stderr, "BoxForge: Cannot read %s: %s\n",
                setting, strerror(errno));
        return -1;
    }

    if (fgets(controllers, sizeof(controllers), file) == NULL) {
        fclose(file);
        fprintf(stderr, "BoxForge: Cannot read subtree controllers\n");
        return -1;
    }

    fclose(file);

    if (strstr(controllers, "cpu") == NULL ||
        strstr(controllers, "memory") == NULL) {
        if (write_setting(setting, "+cpu +memory") == -1) {
            fprintf(stderr,
                    "BoxForge: Cannot enable CPU/memory controllers.\n"
                    "The parent cgroup may not be delegated to BoxForge.\n");
            return -1;
        }
    }

    if (snprintf(cgroup_path, sizeof(cgroup_path),
                 "%s/boxforge-%ld", parent, (long)pid)
        >= (int)sizeof(cgroup_path)) {
        fprintf(stderr, "BoxForge: Cgroup path is too long\n");
        cgroup_path[0] = '\0';
        return -1;
    }

    if (mkdir(cgroup_path, 0755) == -1) {
        fprintf(stderr, "BoxForge: Cannot create %s: %s\n",
                cgroup_path, strerror(errno));
        cgroup_path[0] = '\0';
        return -1;
    }

    snprintf(setting, sizeof(setting), "%s/memory.max", cgroup_path);
    if (write_setting(setting, MEMORY_LIMIT) == -1)
        goto fail;

    snprintf(setting, sizeof(setting), "%s/cpu.max", cgroup_path);
    if (write_setting(setting, CPU_LIMIT) == -1)
        goto fail;

    snprintf(setting, sizeof(setting), "%s/cgroup.procs", cgroup_path);
    snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)pid);

    if (write_setting(setting, pid_text) == -1)
        goto fail;

    printf("BoxForge: Memory limit set to 512 MiB\n");
    printf("BoxForge: CPU quota set to 50%%\n");

    return 0;

fail:
    /*
     * The child may already have been moved into the cgroup.
     * Do not claim cleanup succeeded if the directory is still populated.
     */
    if (rmdir(cgroup_path) == -1 && errno != ENOENT) {
        fprintf(stderr,
                "BoxForge: Could not remove incomplete cgroup %s: %s\n",
                cgroup_path, strerror(errno));
    }

    cgroup_path[0] = '\0';
    return -1;
}

/* Remove the cgroup after the container process has exited. */
void cgroup_cleanup(void)
{
    if (cgroup_path[0] == '\0')
        return;

    if (rmdir(cgroup_path) == -1 && errno != ENOENT) {
        fprintf(stderr,
                "BoxForge: Could not remove cgroup %s: %s\n",
                cgroup_path, strerror(errno));
        return;
    }

    cgroup_path[0] = '\0';
}

