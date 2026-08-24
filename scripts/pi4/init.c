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
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
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
#define USB_STORAGE_VENDOR "46f4"
#define USB_STORAGE_PRODUCT "0001"
#define USB_STORAGE_OFFSET (1024 * 1024)
#define USB_STORAGE_TRANSFER_SIZE (256 * 1024)
#define HWRNG_SAMPLE_SIZE 64
#define SYSFS_WAIT_ATTEMPTS 100
#define SYSFS_WAIT_US 100000
#define HDMI_DDC_ADAPTER "fef04500.i2c"
#define HDMI_EDID_ADDRESS 0x50
#define HDMI_EDID_LENGTH 128

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

static int cmdline_has_word(const char *word)
{
    char cmdline[4096];
    const char *position;
    size_t length = strlen(word);

    if (read_value("/proc/cmdline", cmdline, sizeof(cmdline))) {
        return 0;
    }
    position = cmdline;
    while ((position = strstr(position, word))) {
        if ((position == cmdline ||
             isspace((unsigned char)position[-1])) &&
            (!position[length] ||
             isspace((unsigned char)position[length]))) {
            return 1;
        }
        position += length;
    }
    return 0;
}

static int vl805_usb_block_path(char *device_path, size_t path_size)
{
    struct dirent *entry;
    DIR *directory = opendir("/sys/class/block");

    if (!directory) {
        return 0;
    }
    while ((entry = readdir(directory))) {
        char path[PATH_MAX];
        char resolved[PATH_MAX];

        if (strncmp(entry->d_name, "sd", 2)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/class/block/%s/partition",
                 entry->d_name);
        if (!access(path, F_OK)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/class/block/%s", entry->d_name);
        if (!realpath(path, resolved) ||
            !strstr(resolved, "/" VL805_BDF "/usb")) {
            continue;
        }
        if (device_path) {
            int count = snprintf(device_path, path_size, "/dev/%s",
                                 entry->d_name);

            if (count < 0 || (size_t)count >= path_size ||
                access(device_path, F_OK)) {
                continue;
            }
        }
        closedir(directory);
        return 1;
    }
    closedir(directory);
    return 0;
}

static int wait_for_vl805_usb_block(int present, char *device_path,
                                    size_t path_size)
{
    int consecutive_ready = 0;
    int attempt;

    if (present && (!device_path || !path_size)) {
        return -1;
    }
    for (attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        int found = vl805_usb_block_path(device_path, path_size);

        if (!present && !found) {
            return 0;
        }
        if (present && found) {
            unsigned char sector[512];
            ssize_t count;
            int fd = open(device_path, O_RDONLY);

            if (fd >= 0) {
                do {
                    count = pread(fd, sector, sizeof(sector),
                                  USB_STORAGE_OFFSET +
                                  USB_STORAGE_TRANSFER_SIZE -
                                  sizeof(sector));
                } while (count < 0 && errno == EINTR);
                close(fd);
                if (count == (ssize_t)sizeof(sector)) {
                    /*
                     * With no udev daemon, the block node can become visible
                     * while the kernel is still scanning the disk.  Require
                     * readiness across two polls before starting destructive
                     * I/O or an xHCI unbind.
                     */
                    if (++consecutive_ready == 2) {
                        return 0;
                    }
                } else {
                    consecutive_ready = 0;
                }
            } else {
                consecutive_ready = 0;
            }
        } else {
            consecutive_ready = 0;
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static int storage_transfer(int fd, void *buffer, size_t length,
                            off_t offset, int write_data)
{
    unsigned char *position = buffer;

    while (length) {
        ssize_t count;

        if (write_data) {
            count = pwrite(fd, position, length, offset);
        } else {
            count = pread(fd, position, length, offset);
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            if (!count) {
                errno = EIO;
            }
            return -1;
        }
        position += count;
        length -= count;
        offset += count;
    }
    return 0;
}

static void fill_storage_pattern(unsigned char *buffer, size_t length,
                                 unsigned int sequence)
{
    size_t index;

    for (index = 0; index < length; index++) {
        buffer[index] = (index * 37U) ^ (index >> 7) ^
                        (sequence * 0x5bU);
    }
}

static int verify_usb_storage(const char *device_path,
                              unsigned int sequence, int write_first)
{
    unsigned char *expected = malloc(USB_STORAGE_TRANSFER_SIZE);
    unsigned char *actual = malloc(USB_STORAGE_TRANSFER_SIZE);
    int result = -1;
    int fd = -1;

    if (!expected || !actual) {
        errno = ENOMEM;
        goto out;
    }
    fill_storage_pattern(expected, USB_STORAGE_TRANSFER_SIZE, sequence);
    memset(actual, 0, USB_STORAGE_TRANSFER_SIZE);

    fd = open(device_path, O_RDWR | O_SYNC);
    if (fd < 0) {
        goto out;
    }
    if (write_first &&
        (storage_transfer(fd, expected, USB_STORAGE_TRANSFER_SIZE,
                          USB_STORAGE_OFFSET, 1) || fsync(fd))) {
        goto out;
    }

    /* Force the comparison to traverse USB instead of the guest block cache. */
    if (ioctl(fd, BLKFLSBUF, 0) ||
        storage_transfer(fd, actual, USB_STORAGE_TRANSFER_SIZE,
                         USB_STORAGE_OFFSET, 0)) {
        goto out;
    }
    if (memcmp(expected, actual, USB_STORAGE_TRANSFER_SIZE)) {
        errno = EILSEQ;
        goto out;
    }
    result = 0;

out:
    if (result) {
        fprintf(stderr, "pi4-lab: USB storage transfer on %s failed: %s\n",
                device_path, strerror(errno));
    }
    if (fd >= 0) {
        close(fd);
    }
    free(actual);
    free(expected);
    return result;
}

static int rebind_vl805(int storage_expected)
{
    if (write_value(VL805_UNBIND_PATH, VL805_BDF) ||
        wait_for_path(VL805_DRIVER_PATH, 0) ||
        wait_for_usb_device("2109", "3431", 0) ||
        (storage_expected &&
         (wait_for_usb_device(USB_STORAGE_VENDOR, USB_STORAGE_PRODUCT, 0) ||
          wait_for_vl805_usb_block(0, NULL, 0)))) {
        return -1;
    }
    if (write_value(VL805_BIND_PATH, VL805_BDF) ||
        wait_for_path(VL805_DRIVER_PATH, 1)) {
        return -1;
    }
    return 0;
}

static int verify_hwrng(void)
{
    unsigned char sample[HWRNG_SAMPLE_SIZE];
    size_t offset = 0;
    int attempts = 0;
    int fd;

    if (!value_equals("/sys/class/misc/hw_random/rng_current",
                      "iproc-rng200")) {
        return -1;
    }

    fd = open("/dev/hwrng", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }
    while (offset < sizeof(sample) && attempts < SYSFS_WAIT_ATTEMPTS) {
        ssize_t count = read(fd, sample + offset, sizeof(sample) - offset);

        if (count > 0) {
            offset += count;
            continue;
        }
        if (count < 0 && errno != EAGAIN && errno != EINTR) {
            break;
        }
        attempts++;
        usleep(SYSFS_WAIT_US);
    }
    close(fd);
    return offset == sizeof(sample) ? 0 : -1;
}

static int cpu_thermal_temperature(long *temperature)
{
    struct dirent *entry;
    DIR *directory = opendir("/sys/class/thermal");

    if (!directory) {
        return -1;
    }
    while ((entry = readdir(directory))) {
        char path[PATH_MAX];
        char value[128];
        char trailing;
        long parsed;

        if (strncmp(entry->d_name, "thermal_zone", 12)) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/type",
                 entry->d_name);
        if (!value_equals(path, "cpu-thermal")) {
            continue;
        }
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp",
                 entry->d_name);
        if (read_value(path, value, sizeof(value))) {
            continue;
        }
        if (sscanf(value, "%ld%c", &parsed, &trailing) != 1) {
            continue;
        }
        *temperature = parsed;
        closedir(directory);
        return 0;
    }
    closedir(directory);
    return -1;
}

static int model_is_pi400(void)
{
    char model[128];

    return !read_value("/proc/device-tree/model", model, sizeof(model)) &&
           !strcmp(model, "Raspberry Pi 400");
}

static int platform_driver_bound(const char *device, const char *driver)
{
    char path[PATH_MAX];
    char resolved[PATH_MAX];
    const char *basename;

    snprintf(path, sizeof(path), "/sys/bus/platform/devices/%s/driver",
             device);
    if (!realpath(path, resolved)) {
        return 0;
    }
    basename = strrchr(resolved, '/');
    basename = basename ? basename + 1 : resolved;
    return !strcmp(basename, driver);
}

static int find_i2c_device(const char *adapter_name, char *device_path,
                           size_t device_path_size)
{
    struct dirent *entry;
    DIR *directory = opendir("/sys/bus/i2c/devices");

    if (!directory) {
        return -1;
    }
    while ((entry = readdir(directory))) {
        char name_path[PATH_MAX];

        if (strncmp(entry->d_name, "i2c-", 4)) {
            continue;
        }
        snprintf(name_path, sizeof(name_path),
                 "/sys/bus/i2c/devices/%s/name", entry->d_name);
        if (!value_equals(name_path, adapter_name)) {
            continue;
        }
        snprintf(device_path, device_path_size, "/dev/%s", entry->d_name);
        closedir(directory);
        return 0;
    }
    closedir(directory);
    return -1;
}

static int verify_hdmi_edid(void)
{
    static const unsigned char header[] = {
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    };
    struct i2c_rdwr_ioctl_data transfer;
    struct i2c_msg messages[2];
    unsigned char offset = 0;
    unsigned char edid[HDMI_EDID_LENGTH];
    char device_path[PATH_MAX];
    unsigned int checksum = 0;
    int result;
    int fd;

    if (find_i2c_device(HDMI_DDC_ADAPTER, device_path,
                        sizeof(device_path))) {
        printf("PI4-LAB: HDMI0 DDC adapter %s was not found\n",
               HDMI_DDC_ADAPTER);
        return -1;
    }
    printf("PI4-LAB: HDMI0 DDC adapter device: %s\n", device_path);
    fd = open(device_path, O_RDWR);
    if (fd < 0) {
        printf("PI4-LAB: cannot open HDMI0 DDC adapter: %s\n",
               strerror(errno));
        return -1;
    }

    messages[0].addr = HDMI_EDID_ADDRESS;
    messages[0].flags = 0;
    messages[0].len = sizeof(offset);
    messages[0].buf = &offset;
    messages[1].addr = HDMI_EDID_ADDRESS;
    messages[1].flags = I2C_M_RD;
    messages[1].len = sizeof(edid);
    messages[1].buf = edid;
    transfer.msgs = messages;
    transfer.nmsgs = 2;

    result = ioctl(fd, I2C_RDWR, &transfer);
    if (result != 2) {
        int saved_errno = errno;

        close(fd);
        printf("PI4-LAB: HDMI0 EDID I2C_RDWR returned %d: %s\n",
               result, strerror(saved_errno));
        return -1;
    }
    close(fd);

    if (memcmp(edid, header, sizeof(header))) {
        printf("PI4-LAB: HDMI0 EDID header is"
               " %02x %02x %02x %02x %02x %02x %02x %02x\n",
               edid[0], edid[1], edid[2], edid[3],
               edid[4], edid[5], edid[6], edid[7]);
        return -1;
    }
    for (size_t i = 0; i < sizeof(edid); i++) {
        checksum += edid[i];
    }
    if (checksum & 0xff) {
        printf("PI4-LAB: HDMI0 EDID checksum is 0x%02x\n",
               checksum & 0xff);
        return -1;
    }
    return 0;
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
    char storage_device[PATH_MAX] = { 0 };
    struct utsname uts;
    long temperature = 0;
    unsigned long interrupts_before_rebind;
    int failures = 0;
    int pi400;
    int storage_ready = 0;
    int storage_test;
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
    storage_test = cmdline_has_word("pi4lab.usb_storage=1");
    report_check("BCM2711 HDMI DVP clock/reset driver",
                 !platform_driver_bound("fef00000.clock", "brcm2711-dvp"),
                 &failures);
    report_check("BCM2711 HDMI DDC0 controller and driver",
                 !platform_driver_bound("fef04500.i2c", "brcmstb-i2c"),
                 &failures);
    report_check("BCM2711 HDMI DDC1 controller and driver",
                 !platform_driver_bound("fef09500.i2c", "brcmstb-i2c"),
                 &failures);
    report_check("HDMI0 DDC reads a valid 128-byte EDID",
                 verify_hdmi_edid(), &failures);
    report_check("RNG200 selected and /dev/hwrng supplies 64 bytes",
                 verify_hwrng(), &failures);
    if (!cpu_thermal_temperature(&temperature)) {
        printf("PI4-LAB: cpu-thermal temperature: %ld mC\n", temperature);
        report_check("BCM2711 cpu-thermal reports 35050 mC",
                     temperature != 35050, &failures);
    } else {
        report_check("BCM2711 cpu-thermal reports 35050 mC", 1,
                     &failures);
    }
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
    if (storage_test) {
        int usb_ready = !wait_for_usb_device(USB_STORAGE_VENDOR,
                                             USB_STORAGE_PRODUCT, 1);
        int block_ready = !wait_for_vl805_usb_block(1, storage_device,
                                                    sizeof(storage_device));

        report_check("QEMU 46f4:0001 USB mass storage", !usb_ready,
                     &failures);
        report_check("VL805 USB storage block device", !block_ready,
                     &failures);
        storage_ready = usb_ready && block_ready;
        report_check("USB storage write/read integrity",
                     !storage_ready ||
                     verify_usb_storage(storage_device, 0, 1), &failures);
    }
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
        int rebind_failed = rebind_vl805(storage_test);

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
            if (storage_test) {
                int usb_ready = !wait_for_usb_device(USB_STORAGE_VENDOR,
                                                     USB_STORAGE_PRODUCT, 1);
                int block_ready = !wait_for_vl805_usb_block(
                    1, storage_device, sizeof(storage_device));

                report_check("USB storage re-enumerated after rebind",
                             !usb_ready, &failures);
                report_check("USB block device restored after rebind",
                             !block_ready, &failures);
                storage_ready = usb_ready && block_ready;
                report_check("USB storage retained data across rebind",
                             !storage_ready ||
                             verify_usb_storage(storage_device, 0, 0),
                             &failures);
                report_check("USB storage write/read after rebind",
                             !storage_ready ||
                             verify_usb_storage(storage_device, 1, 1),
                             &failures);
            }
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
