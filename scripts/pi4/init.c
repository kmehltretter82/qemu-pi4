/*
 * Minimal init for the qemu-pi4 upstream Linux smoke image.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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

#define PCI_ROOT_BDF "0000:00:00.0"
#define VL805_BDF "0000:01:00.0"
#define VL805_DRIVER_PATH "/sys/bus/pci/devices/" VL805_BDF "/driver"
#define VL805_UNBIND_PATH "/sys/bus/pci/drivers/xhci_hcd/unbind"
#define VL805_BIND_PATH "/sys/bus/pci/drivers/xhci_hcd/bind"
#define SYSFS_WAIT_ATTEMPTS 100
#define SYSFS_WAIT_US 100000

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

static int read_value(const char *path, char *value, size_t size)
{
    ssize_t count;
    int fd;

    if (!size) {
        return -1;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    count = read(fd, value, size - 1);
    close(fd);
    if (count < 0) {
        return -1;
    }
    while (count && (value[count - 1] == '\n' ||
                     value[count - 1] == '\r' ||
                     value[count - 1] == '\0')) {
        count--;
    }
    value[count] = '\0';
    return 0;
}

static int value_equals(const char *path, const char *expected)
{
    char value[128];

    return !read_value(path, value, sizeof(value)) &&
           !strcmp(value, expected);
}

static int pci_identity_present(const char *bdf, const char *vendor,
                                const char *device)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", bdf);
    if (!value_equals(path, vendor)) {
        return 0;
    }
    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", bdf);
    return value_equals(path, device);
}

static int wait_for_pci_identity(const char *bdf, const char *vendor,
                                 const char *device)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if (pci_identity_present(bdf, vendor, device)) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static int usb_device_present(const char *vendor, const char *product)
{
    struct dirent *entry;
    DIR *directory = opendir("/sys/bus/usb/devices");

    if (!directory) {
        return 0;
    }
    while ((entry = readdir(directory))) {
        char path[PATH_MAX];

        if (entry->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idVendor",
                 entry->d_name);
        if (!value_equals(path, vendor)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idProduct",
                 entry->d_name);
        if (value_equals(path, product)) {
            closedir(directory);
            return 1;
        }
    }
    closedir(directory);
    return 0;
}

static int usb_root_hub_present(const char *product, const char *speed,
                                const char *maxchild)
{
    struct dirent *entry;
    DIR *directory = opendir("/sys/bus/usb/devices");

    if (!directory) {
        return 0;
    }
    while ((entry = readdir(directory))) {
        char path[PATH_MAX];
        char resolved[PATH_MAX];

        if (strncmp(entry->d_name, "usb", 3)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s",
                 entry->d_name);
        if (!realpath(path, resolved) ||
            !strstr(resolved, "/" VL805_BDF "/usb")) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idVendor",
                 entry->d_name);
        if (!value_equals(path, "1d6b")) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/idProduct",
                 entry->d_name);
        if (!value_equals(path, product)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/speed",
                 entry->d_name);
        if (!value_equals(path, speed)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/maxchild",
                 entry->d_name);
        if (value_equals(path, maxchild)) {
            closedir(directory);
            return 1;
        }
    }
    closedir(directory);
    return 0;
}

static int wait_for_usb_device(const char *vendor, const char *product,
                               int present)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if (usb_device_present(vendor, product) == present) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static int wait_for_usb_root_hub(const char *product, const char *speed,
                                 const char *maxchild)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if (usb_root_hub_present(product, speed, maxchild)) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static unsigned int pi400_hid_interfaces(void)
{
    static const char prefix[] = "0003:04D9:0007.";
    struct dirent *entry;
    unsigned int count = 0;
    DIR *directory = opendir("/sys/bus/hid/devices");

    if (!directory) {
        return 0;
    }
    while ((entry = readdir(directory))) {
        if (!strncmp(entry->d_name, prefix, strlen(prefix))) {
            count++;
        }
    }
    closedir(directory);
    return count;
}

static int wait_for_pi400_hid_interfaces(unsigned int minimum)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if (pi400_hid_interfaces() >= minimum) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static unsigned long xhci_interrupt_count(void)
{
    char line[4096];
    FILE *file = fopen("/proc/interrupts", "r");

    if (!file) {
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        char *position;
        unsigned long total = 0;

        if (!strstr(line, "xhci_hcd")) {
            continue;
        }
        position = strchr(line, ':');
        if (!position) {
            continue;
        }
        position++;
        for (;;) {
            unsigned long count = 0;

            while (isspace((unsigned char)*position)) {
                position++;
            }
            if (!isdigit((unsigned char)*position)) {
                break;
            }
            do {
                count = count * 10 + (*position - '0');
                position++;
            } while (isdigit((unsigned char)*position));
            if (ULONG_MAX - total < count) {
                total = ULONG_MAX;
            } else {
                total += count;
            }
        }
        fclose(file);
        return total;
    }
    fclose(file);
    return 0;
}

static int wait_for_xhci_interrupts(unsigned long previous)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if (xhci_interrupt_count() > previous) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static int write_value(const char *path, const char *value)
{
    size_t length = strlen(value);
    ssize_t written;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        return -1;
    }
    written = write(fd, value, length);
    close(fd);
    return written == (ssize_t)length ? 0 : -1;
}

static int wait_for_path(const char *path, int present)
{
    int attempt;

    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        if ((access(path, F_OK) == 0) == present) {
            return 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static int rebind_vl805(void)
{
    if (write_value(VL805_UNBIND_PATH, VL805_BDF) ||
        wait_for_path(VL805_DRIVER_PATH, 0) ||
        wait_for_usb_device("2109", "3431", 0)) {
        return -1;
    }
    if (write_value(VL805_BIND_PATH, VL805_BDF) ||
        wait_for_path(VL805_DRIVER_PATH, 1)) {
        return -1;
    }
    return 0;
}

static int model_is_pi400(void)
{
    char model[128];

    return !read_value("/proc/device-tree/model", model, sizeof(model)) &&
           !strcmp(model, "Raspberry Pi 400");
}

static void report_check(const char *name, int failed, int *failures)
{
    printf("PI4-LAB: %-48s %s\n", name, failed ? "FAIL" : "ok");
    if (failed) {
        (*failures)++;
    }
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
    unsigned long interrupts_before_rebind;
    int failures = 0;
    int pi400;
    int vl805_ready;

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

    pi400 = model_is_pi400();
    report_check("BCM2711 PCIe root port 14e4:2711",
                 wait_for_pci_identity(PCI_ROOT_BDF, "0x14e4", "0x2711"),
                 &failures);
    vl805_ready = !wait_for_pci_identity(VL805_BDF, "0x1106", "0x3483");
    report_check("VL805 PCIe endpoint 1106:3483", !vl805_ready, &failures);
    report_check("xHCI USB 2 root: 480 Mbit/s, one port",
                 wait_for_usb_root_hub("0002", "480", "1"), &failures);
    report_check("xHCI USB 3 root: 5 Gbit/s, four ports",
                 wait_for_usb_root_hub("0003", "5000", "4"), &failures);
    report_check("VIA 2109:3431 high-speed hub",
                 wait_for_usb_device("2109", "3431", 1), &failures);
    if (pi400) {
        report_check("Pi 400 keyboard 04d9:0007",
                     wait_for_usb_device("04d9", "0007", 1), &failures);
        report_check("Pi 400 keyboard two HID interfaces",
                     wait_for_pi400_hid_interfaces(2), &failures);
    }
    report_check("xHCI MSI activity", wait_for_xhci_interrupts(0),
                 &failures);

    interrupts_before_rebind = xhci_interrupt_count();
    printf("PI4-LAB: xHCI MSI count before rebind: %lu\n",
           interrupts_before_rebind);
    if (vl805_ready) {
        int rebind_failed = rebind_vl805();

        report_check("VL805 xHCI unbind/rebind", rebind_failed, &failures);
        if (!rebind_failed) {
            report_check("xHCI USB 2 root restored after rebind",
                         wait_for_usb_root_hub("0002", "480", "1"),
                         &failures);
            report_check("xHCI USB 3 root restored after rebind",
                         wait_for_usb_root_hub("0003", "5000", "4"),
                         &failures);
            report_check("VIA hub re-enumerated after rebind",
                         wait_for_usb_device("2109", "3431", 1), &failures);
            if (pi400) {
                report_check("Pi 400 keyboard re-enumerated after rebind",
                             wait_for_usb_device("04d9", "0007", 1),
                             &failures);
                report_check("Both Pi 400 HID interfaces rebound",
                             wait_for_pi400_hid_interfaces(2), &failures);
            }
            report_check("xHCI MSI active after rebind",
                         wait_for_xhci_interrupts(0),
                         &failures);
            printf("PI4-LAB: xHCI MSI count after rebind: %lu\n",
                   xhci_interrupt_count());
        }
    }

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

    if (failures) {
        printf("\nPI4-LAB: acceptance failed (%d check%s)\n", failures,
               failures == 1 ? "" : "s");
    } else {
        printf("\nPI4-LAB: upstream Linux boot successful\n");
    }
    fflush(NULL);
    sync();
    sleep(1);

    if (reboot(RB_AUTOBOOT)) {
        fprintf(stderr, "pi4-lab: reboot failed: %s\n", strerror(errno));
    }

    for (;;) {
        pause();
    }
}
