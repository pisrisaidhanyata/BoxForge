#ifndef BOXFORGE_CGROUPS_H
#define BOXFORGE_CGROUPS_H

#include <sys/types.h>

int cgroup_create(pid_t pid);
void cgroup_cleanup(void);

#endif
