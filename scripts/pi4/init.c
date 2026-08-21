/*
 * Minimal init for the qemu-pi4 upstream Linux smoke image.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

static void mount_fs(const char *source, const char *target,
                     const char *filesystem)
{
    if (mount(source, target, filesystem, 0, NULL) && errno != EBUSY) {
        fprintf(stderr, "pi4-lab: cannot mount %s: %s\n",
                target, strerror(errno));
    }
}

static void dump_file(const char *path)
{
    char buffer[4096];
    ssize_t count;
    int fd = open(path, O_RDONLY);

    printf("\n===== %s =====\n", path);
    if (fd < 0) {
        printf("pi4-lab: cannot open %s: %s\n", path, strerror(errno));
        return;
    }

    while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t offset = 0;

        while (offset < count) {
            ssize_t written = write(STDOUT_FILENO, buffer + offset,
                                    count - offset);
            if (written < 0) {
                close(fd);
                return;
            }
            offset += written;
        }
    }
    close(fd);
}

int main(void)
{
    struct utsname uts;

    mkdir("/dev", 0755);
    mkdir("/proc", 0555);
    mkdir("/sys", 0555);
    mkdir("/tmp", 01777);

    mount_fs("devtmpfs", "/dev", "devtmpfs");
    mount_fs("proc", "/proc", "proc");
    mount_fs("sysfs", "/sys", "sysfs");

    if (!uname(&uts)) {
        printf("pi4-lab: Linux %s %s\n", uts.release, uts.machine);
    }

    dump_file("/proc/cmdline");
    dump_file("/proc/cpuinfo");
    dump_file("/proc/iomem");
    dump_file("/proc/interrupts");

    printf("\nPI4-LAB: upstream Linux boot successful\n");
    fflush(NULL);
    sync();
    sleep(1);

    if (reboot(RB_POWER_OFF)) {
        fprintf(stderr, "pi4-lab: poweroff failed: %s\n", strerror(errno));
    }

    for (;;) {
        pause();
    }
}
