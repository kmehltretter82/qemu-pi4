/*
 * Minimal init for the qemu-pi4 physical Pi 400 smoke test.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#define BOOT_DEVICE "/dev/mmcblk0p1"
#define RESULT_PATH "/result/qemu-pi4-hardware-result.txt"

static int result_fd = -1;

static void write_all(int fd, const void *buffer, size_t length)
{
    const char *bytes = buffer;

    while (length) {
        ssize_t written = write(fd, bytes, length);

        if (written <= 0) {
            return;
        }
        bytes += written;
        length -= written;
    }
}

static void emit(const void *buffer, size_t length)
{
    write_all(STDOUT_FILENO, buffer, length);
    if (result_fd >= 0) {
        write_all(result_fd, buffer, length);
    }
}

static void emitf(const char *format, ...)
{
    char buffer[4096];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length > 0) {
        emit(buffer, (size_t)length < sizeof(buffer) ? (size_t)length
                                                    : sizeof(buffer) - 1);
    }
}

static void mount_fs(const char *source, const char *target,
                     const char *filesystem)
{
    if (mount(source, target, filesystem, 0, NULL) && errno != EBUSY) {
        emitf("pi4-lab: cannot mount %s: %s\n", target, strerror(errno));
    }
}

static void dump_file(const char *path)
{
    char buffer[4096];
    ssize_t count;
    int fd;

    emitf("\n===== %s =====\n", path);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        emitf("pi4-lab: cannot open %s: %s\n", path, strerror(errno));
        return;
    }
    while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
        emit(buffer, count);
    }
    close(fd);
}

static void open_result(void)
{
    int attempt;

    for (attempt = 0; attempt < 30; attempt++) {
        if (!access(BOOT_DEVICE, F_OK)) {
            break;
        }
        sleep(1);
    }

    if (access(BOOT_DEVICE, F_OK)) {
        emitf("pi4-lab: boot device %s did not appear: %s\n",
              BOOT_DEVICE, strerror(errno));
        return;
    }
    if (mount(BOOT_DEVICE, "/result", "vfat", MS_SYNCHRONOUS, NULL)) {
        emitf("pi4-lab: cannot mount %s: %s\n",
              BOOT_DEVICE, strerror(errno));
        return;
    }

    result_fd = open(RESULT_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (result_fd < 0) {
        emitf("pi4-lab: cannot create %s: %s\n",
              RESULT_PATH, strerror(errno));
    }
}

int main(void)
{
    struct utsname uts;

    mkdir("/dev", 0755);
    mkdir("/proc", 0555);
    mkdir("/sys", 0555);
    mkdir("/result", 0755);
    mount_fs("devtmpfs", "/dev", "devtmpfs");
    mount_fs("proc", "/proc", "proc");
    mount_fs("sysfs", "/sys", "sysfs");
    open_result();

    if (!uname(&uts)) {
        emitf("pi4-lab: Linux %s %s\n", uts.release, uts.machine);
    }
    emitf("pi4-lab: physical boot device %s\n", BOOT_DEVICE);
    dump_file("/proc/device-tree/model");
    dump_file("/proc/cmdline");
    dump_file("/proc/cpuinfo");
    dump_file("/proc/iomem");
    dump_file("/proc/interrupts");
    emitf("\nPI4-LAB: physical upstream Linux boot successful\n");

    if (result_fd >= 0) {
        fsync(result_fd);
        close(result_fd);
        result_fd = -1;
    }
    sync();
    umount("/result");
    sleep(1);

    if (reboot(RB_AUTOBOOT)) {
        emitf("pi4-lab: reboot failed: %s\n", strerror(errno));
    }
    for (;;) {
        pause();
    }
}
