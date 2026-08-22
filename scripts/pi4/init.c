/*
 * Minimal init for the qemu-pi4 upstream Linux smoke image.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/socket.h>
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

static int bring_up_interface(const char *name)
{
    struct ifreq ifr = { 0 };
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        return -1;
    }

    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr)) {
        close(fd);
        return -1;
    }

    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr)) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int wait_for_carrier(const char *path)
{
    for (int attempt = 0; attempt < 5; attempt++) {
        char value;
        int fd = open(path, O_RDONLY);

        if (fd >= 0) {
            ssize_t count = read(fd, &value, 1);

            close(fd);
            if (count == 1 && value == '1') {
                return 0;
            }
        }
        sleep(1);
    }

    return -1;
}

static void dump_ipv4_address(const char *name)
{
    struct ifreq ifr = { 0 };
    char address[INET_ADDRSTRLEN];
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        return;
    }

    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", name);
    if (!ioctl(fd, SIOCGIFADDR, &ifr) &&
        inet_ntop(AF_INET,
                  &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,
                  address, sizeof(address))) {
        printf("PI4-LAB: %s IPv4 address %s\n", name, address);
    } else {
        printf("PI4-LAB: %s has no IPv4 address\n", name);
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

    if (bring_up_interface("eth0")) {
        printf("\nPI4-LAB: eth0 unavailable: %s\n", strerror(errno));
    } else {
        printf("\nPI4-LAB: eth0 brought up\n");
        if (!wait_for_carrier("/sys/class/net/eth0/carrier")) {
            printf("PI4-LAB: eth0 carrier detected\n");
        } else {
            printf("PI4-LAB: eth0 has no carrier\n");
        }
        dump_file("/sys/class/net/eth0/address");
        dump_file("/sys/class/net/eth0/operstate");
        dump_file("/sys/class/net/eth0/carrier");
        dump_ipv4_address("eth0");
        dump_file("/proc/net/dev");
        dump_file("/proc/net/route");
    }

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
