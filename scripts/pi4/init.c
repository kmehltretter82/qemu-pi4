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
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
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
#define EDID_BLOCK_LENGTH 128
#define HDMI_EDID_LENGTH (2 * EDID_BLOCK_LENGTH)
#define EDID_EXTENSION_COUNT 126
#define CTA_EXTENSION_TAG 0x02
#define CTA_REVISION 0x03
#define CTA_BASIC_AUDIO (1U << 6)
#define CTA_DB_AUDIO 1
#define CTA_DB_VENDOR 3
#define CTA_DB_SPEAKER 4
#define DRM_CARD_PATH "/dev/dri/card0"
#define DRM_CONNECTOR_PATH "/sys/class/drm/card0-HDMI-A-1"
#define FRAMEBUFFER_PATH "/dev/fb0"
#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 800
#define HDMI_AUDIO_RATE 48000
#define HDMI_AUDIO_CHANNELS 2
#define HDMI_AUDIO_PERIOD_FRAMES 1024
#define HDMI_AUDIO_BUFFER_FRAMES 4096
#define HDMI_AUDIO_TEST_FRAMES HDMI_AUDIO_RATE
#define HDMI_AUDIO_LEFT_PERIOD 48
#define HDMI_AUDIO_RIGHT_PERIOD 24
#define HDMI_AUDIO_LEFT_AMPLITUDE 0x400000
#define HDMI_AUDIO_RIGHT_AMPLITUDE 0x200000

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

static int file_has_line(const char *path, const char *expected)
{
    char line[128];
    FILE *file = fopen(path, "r");

    if (!file) {
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!strcmp(line, expected)) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int framebuffer_mode_matches(void)
{
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    int fd = open(FRAMEBUFFER_PATH, O_RDWR);

    if (fd < 0) {
        return 0;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var)) {
        close(fd);
        return 0;
    }
    close(fd);

    printf("PI4-LAB: fb0 mode %ux%u virtual %ux%u, %u bpp, pitch %u\n",
           var.xres, var.yres, var.xres_virtual, var.yres_virtual,
           var.bits_per_pixel, fix.line_length);
    return var.xres == DISPLAY_WIDTH && var.yres == DISPLAY_HEIGHT &&
           var.xres_virtual >= var.xres && var.yres_virtual >= var.yres &&
           var.bits_per_pixel == 16 &&
           fix.line_length >= var.xres * sizeof(uint16_t);
}

static int write_framebuffer_pattern(void)
{
    static const uint16_t colors[] = {
        0xf800, /* red */
        0x07e0, /* green */
        0x001f, /* blue */
        0xffff, /* white */
    };
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    uint8_t *mapping;
    size_t length;
    int fd = open(FRAMEBUFFER_PATH, O_RDWR | O_SYNC);

    if (fd < 0) {
        return -1;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var) ||
        var.xres != DISPLAY_WIDTH || var.yres != DISPLAY_HEIGHT ||
        var.bits_per_pixel != 16 ||
        fix.line_length < var.xres * sizeof(uint16_t)) {
        close(fd);
        return -1;
    }

    length = (size_t)fix.line_length * var.yres_virtual;
    mapping = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        close(fd);
        return -1;
    }

    for (uint32_t y = 0; y < var.yres; y++) {
        uint16_t *row = (uint16_t *)(mapping + y * fix.line_length);

        for (uint32_t x = 0; x < var.xres; x++) {
            row[x] = colors[(x * 4U) / var.xres];
        }
    }

    msync(mapping, length, MS_SYNC);
    munmap(mapping, length);
    close(fd);

    /* Let QEMU's display refresh timer consume the dirty guest pages. */
    usleep(500000);
    return 0;
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

static int read_hdmi_edid_block(int fd, unsigned char offset,
                                unsigned char block[EDID_BLOCK_LENGTH])
{
    struct i2c_rdwr_ioctl_data transfer;
    struct i2c_msg messages[2];
    int result;

    messages[0].addr = HDMI_EDID_ADDRESS;
    messages[0].flags = 0;
    messages[0].len = sizeof(offset);
    messages[0].buf = &offset;
    messages[1].addr = HDMI_EDID_ADDRESS;
    messages[1].flags = I2C_M_RD;
    messages[1].len = EDID_BLOCK_LENGTH;
    messages[1].buf = block;
    transfer.msgs = messages;
    transfer.nmsgs = 2;

    result = ioctl(fd, I2C_RDWR, &transfer);
    if (result != 2) {
        printf("PI4-LAB: HDMI0 EDID block 0x%02x I2C_RDWR"
               " returned %d%s%s\n", offset, result,
               result < 0 ? ": " : "",
               result < 0 ? strerror(errno) : "");
        return -1;
    }
    return 0;
}

static int verify_hdmi_cta_audio(const unsigned char *cta)
{
    unsigned int end = cta[2];
    int has_lpcm = 0;
    int has_speakers = 0;
    int has_hdmi = 0;

    if (cta[0] != CTA_EXTENSION_TAG || cta[1] != CTA_REVISION ||
        end <= 4 || end >= EDID_BLOCK_LENGTH ||
        !(cta[3] & CTA_BASIC_AUDIO)) {
        printf("PI4-LAB: HDMI0 CTA header is"
               " tag=%02x revision=%02x dtd=%u features=%02x\n",
               cta[0], cta[1], end, cta[3]);
        return -1;
    }

    for (unsigned int offset = 4; offset < end; ) {
        unsigned int length = cta[offset] & 0x1f;
        unsigned int tag = cta[offset] >> 5;
        const unsigned char *payload = cta + offset + 1;

        if (offset + length + 1 > end) {
            printf("PI4-LAB: HDMI0 CTA data block overruns offset %u\n",
                   offset);
            return -1;
        }
        if (tag == CTA_DB_AUDIO) {
            for (unsigned int descriptor = 0;
                 descriptor + 3 <= length; descriptor += 3) {
                const unsigned char *sad = payload + descriptor;

                if ((sad[0] & 0x78) == 0x08 &&
                    (sad[0] & 0x07) == 0x01 &&
                    (sad[1] & 0x07) == 0x07 &&
                    (sad[2] & 0x07) == 0x07) {
                    has_lpcm = 1;
                }
            }
        } else if (tag == CTA_DB_SPEAKER && length >= 1 &&
                   (payload[0] & 0x01)) {
            has_speakers = 1;
        } else if (tag == CTA_DB_VENDOR && length >= 3 &&
                   payload[0] == 0x03 && payload[1] == 0x0c &&
                   payload[2] == 0x00) {
            has_hdmi = 1;
        }
        offset += length + 1;
    }

    if (!has_lpcm || !has_speakers || !has_hdmi) {
        printf("PI4-LAB: HDMI0 CTA capabilities are"
               " lpcm=%d speakers=%d hdmi=%d\n",
               has_lpcm, has_speakers, has_hdmi);
        return -1;
    }
    return 0;
}

static int verify_hdmi_edid(void)
{
    static const unsigned char header[] = {
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    };
    unsigned char edid[HDMI_EDID_LENGTH];
    char device_path[PATH_MAX];
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

    if (read_hdmi_edid_block(fd, 0, edid) ||
        read_hdmi_edid_block(fd, EDID_BLOCK_LENGTH,
                             edid + EDID_BLOCK_LENGTH)) {
        close(fd);
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
    if (edid[20] != 0xa2 || edid[EDID_EXTENSION_COUNT] != 1) {
        printf("PI4-LAB: HDMI0 EDID interface is 0x%02x with"
               " %u extensions\n", edid[20],
               edid[EDID_EXTENSION_COUNT]);
        return -1;
    }
    for (unsigned int block = 0; block < 2; block++) {
        unsigned int checksum = 0;

        for (unsigned int i = 0; i < EDID_BLOCK_LENGTH; i++) {
            checksum += edid[block * EDID_BLOCK_LENGTH + i];
        }
        if (checksum & 0xff) {
            printf("PI4-LAB: HDMI0 EDID block %u checksum is 0x%02x\n",
                   block, checksum & 0xff);
            return -1;
        }
    }
    return verify_hdmi_cta_audio(edid + EDID_BLOCK_LENGTH);
}

static int find_hdmi_pcm_device(char *device_path, size_t device_path_size)
{
    for (int attempt = 0; attempt < SYSFS_WAIT_ATTEMPTS; attempt++) {
        for (unsigned int card = 0; card < 32; card++) {
            char path[PATH_MAX];
            char resolved[PATH_MAX];
            int count;

            snprintf(path, sizeof(path),
                     "/sys/class/sound/card%u/device", card);
            if (!realpath(path, resolved) ||
                !strstr(resolved, "/fef00700.hdmi")) {
                continue;
            }
            count = snprintf(device_path, device_path_size,
                             "/dev/snd/pcmC%uD0p", card);
            if (count >= 0 && (size_t)count < device_path_size &&
                !access(device_path, R_OK | W_OK)) {
                return 0;
            }
        }
        usleep(SYSFS_WAIT_US);
    }
    return -1;
}

static struct snd_mask *pcm_hw_mask(struct snd_pcm_hw_params *params,
                                    unsigned int parameter)
{
    return &params->masks[parameter - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

static struct snd_interval *pcm_hw_interval(struct snd_pcm_hw_params *params,
                                            unsigned int parameter)
{
    return &params->intervals[parameter -
                              SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

static void pcm_hw_params_any(struct snd_pcm_hw_params *params)
{
    memset(params, 0, sizeof(*params));
    for (unsigned int parameter = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         parameter <= SNDRV_PCM_HW_PARAM_LAST_MASK; parameter++) {
        struct snd_mask *mask = pcm_hw_mask(params, parameter);

        for (size_t word = 0; word < sizeof(mask->bits) /
                                      sizeof(mask->bits[0]); word++) {
            mask->bits[word] = UINT_MAX;
        }
        params->cmask |= 1U << parameter;
        params->rmask |= 1U << parameter;
    }
    for (unsigned int parameter = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         parameter <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; parameter++) {
        struct snd_interval *interval = pcm_hw_interval(params, parameter);

        interval->min = 0;
        interval->max = UINT_MAX;
        params->cmask |= 1U << parameter;
        params->rmask |= 1U << parameter;
    }
    params->info = UINT_MAX;
}

static void pcm_hw_param_set(struct snd_pcm_hw_params *params,
                             unsigned int parameter, unsigned int value)
{
    if (parameter <= SNDRV_PCM_HW_PARAM_LAST_MASK) {
        struct snd_mask *mask = pcm_hw_mask(params, parameter);

        memset(mask, 0, sizeof(*mask));
        mask->bits[value / 32] = 1U << (value % 32);
    } else {
        struct snd_interval *interval = pcm_hw_interval(params, parameter);

        memset(interval, 0, sizeof(*interval));
        interval->min = value;
        interval->max = value;
        interval->integer = 1;
    }
    params->cmask |= 1U << parameter;
    params->rmask |= 1U << parameter;
}

static unsigned int pcm_hw_param_value(struct snd_pcm_hw_params *params,
                                       unsigned int parameter)
{
    if (parameter <= SNDRV_PCM_HW_PARAM_LAST_MASK) {
        struct snd_mask *mask = pcm_hw_mask(params, parameter);

        for (unsigned int value = 0; value < SNDRV_MASK_MAX; value++) {
            if (mask->bits[value / 32] & (1U << (value % 32))) {
                return value;
            }
        }
        return UINT_MAX;
    }
    return pcm_hw_interval(params, parameter)->min;
}

static uint32_t hdmi_audio_subframe(int32_t sample, unsigned int frame,
                                    unsigned int channel)
{
    uint32_t preamble;
    uint32_t subframe;

    if (channel) {
        preamble = 0x4;
    } else if (!(frame % 192)) {
        preamble = 0x8;
    } else {
        preamble = 0x2;
    }

    subframe = (((uint32_t)sample & 0xffffff) << 4) | preamble;
    if (__builtin_parity(subframe >> 4)) {
        subframe |= 1U << 31;
    }
    return subframe;
}

static int play_hdmi_audio(const char *device_path)
{
    struct snd_pcm_hw_params params;
    uint32_t *samples = NULL;
    const char *stage = "allocate sample buffer";
    unsigned int transferred = 0;
    int result = -1;
    int fd = -1;

    samples = malloc(HDMI_AUDIO_TEST_FRAMES * HDMI_AUDIO_CHANNELS *
                     sizeof(*samples));
    if (!samples) {
        errno = ENOMEM;
        goto out;
    }
    for (unsigned int frame = 0; frame < HDMI_AUDIO_TEST_FRAMES; frame++) {
        int32_t left = frame % HDMI_AUDIO_LEFT_PERIOD <
                       HDMI_AUDIO_LEFT_PERIOD / 2 ?
                       HDMI_AUDIO_LEFT_AMPLITUDE :
                       -HDMI_AUDIO_LEFT_AMPLITUDE;
        int32_t right = frame % HDMI_AUDIO_RIGHT_PERIOD <
                        HDMI_AUDIO_RIGHT_PERIOD / 2 ?
                        HDMI_AUDIO_RIGHT_AMPLITUDE :
                        -HDMI_AUDIO_RIGHT_AMPLITUDE;

        samples[frame * HDMI_AUDIO_CHANNELS] =
            hdmi_audio_subframe(left, frame, 0);
        samples[frame * HDMI_AUDIO_CHANNELS + 1] =
            hdmi_audio_subframe(right, frame, 1);
    }

    stage = "open PCM device";
    fd = open(device_path, O_WRONLY);
    if (fd < 0) {
        goto out;
    }
    stage = "configure PCM hardware";
    pcm_hw_params_any(&params);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                     SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_FORMAT,
                     SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_CHANNELS,
                     HDMI_AUDIO_CHANNELS);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_RATE, HDMI_AUDIO_RATE);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                     HDMI_AUDIO_PERIOD_FRAMES);
    pcm_hw_param_set(&params, SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
                     HDMI_AUDIO_BUFFER_FRAMES);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params)) {
        goto out;
    }
    stage = "validate PCM hardware parameters";
    if (pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_ACCESS) !=
            SNDRV_PCM_ACCESS_RW_INTERLEAVED ||
        pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_FORMAT) !=
            SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE ||
        pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_CHANNELS) !=
            HDMI_AUDIO_CHANNELS ||
        pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_RATE) !=
            HDMI_AUDIO_RATE ||
        pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE) !=
            HDMI_AUDIO_PERIOD_FRAMES ||
        pcm_hw_param_value(&params, SNDRV_PCM_HW_PARAM_BUFFER_SIZE) !=
            HDMI_AUDIO_BUFFER_FRAMES) {
        errno = EINVAL;
        goto out;
    }

    printf("PI4-LAB: HDMI0 PCM device %s, %u Hz IEC958, %u channels\n",
           device_path, HDMI_AUDIO_RATE, HDMI_AUDIO_CHANNELS);
    stage = "prepare PCM device";
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE)) {
        goto out;
    }
    stage = "write PCM frames";
    while (transferred < HDMI_AUDIO_TEST_FRAMES) {
        struct snd_xferi transfer = {
            .buf = samples + transferred * HDMI_AUDIO_CHANNELS,
            .frames = HDMI_AUDIO_TEST_FRAMES - transferred,
        };
        int ioctl_result = ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES,
                                 &transfer);

        if (ioctl_result || transfer.result <= 0) {
            if (!ioctl_result && transfer.result < 0) {
                errno = -transfer.result;
            } else if (!ioctl_result) {
                errno = EIO;
            }
            goto out;
        }
        transferred += transfer.result;
    }
    stage = "drain PCM device";
    if (ioctl(fd, SNDRV_PCM_IOCTL_DRAIN)) {
        goto out;
    }
    result = 0;

out:
    if (result) {
        fprintf(stderr, "pi4-lab: HDMI playback on %s failed during %s: %s\n",
                device_path, stage, strerror(errno));
    }
    if (fd >= 0) {
        close(fd);
    }
    free(samples);
    return result;
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
    char hdmi_pcm_device[PATH_MAX] = { 0 };
    char storage_device[PATH_MAX] = { 0 };
    struct utsname uts;
    long temperature = 0;
    unsigned long interrupts_before_rebind;
    int failures = 0;
    int hdmi_audio_ready;
    int hdmi_audio_test;
    int pi400;
    int display_test;
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
    display_test = cmdline_has_word("pi4lab.display_test=1");
    hdmi_audio_test = cmdline_has_word("pi4lab.hdmi_audio_test=1");
    report_check("VC4 DRM card0 registered",
                 wait_for_path(DRM_CARD_PATH, 1), &failures);
    report_check("HDMI-A-1 connector reports connected",
                 !value_equals(DRM_CONNECTOR_PATH "/status", "connected"),
                 &failures);
    report_check("HDMI-A-1 advertises preferred 1280x800 mode",
                 !file_has_line(DRM_CONNECTOR_PATH "/modes", "1280x800"),
                 &failures);
    report_check("HDMI-A-1 has an active scanout",
                 !value_equals(DRM_CONNECTOR_PATH "/enabled", "enabled"),
                 &failures);
    report_check("VC4 DRM framebuffer is 1280x800 RGB565",
                 wait_for_path(FRAMEBUFFER_PATH, 1) ||
                 !framebuffer_mode_matches(), &failures);
    report_check("BCM2711 HDMI DVP clock/reset driver",
                 !platform_driver_bound("fef00000.clock", "brcm2711-dvp"),
                 &failures);
    report_check("BCM2711 HDMI DDC0 controller and driver",
                 !platform_driver_bound("fef04500.i2c", "brcmstb-i2c"),
                 &failures);
    report_check("BCM2711 HDMI DDC1 controller and driver",
                 !platform_driver_bound("fef09500.i2c", "brcmstb-i2c"),
                 &failures);
    report_check("HDMI0 DDC advertises HDMI stereo audio",
                 verify_hdmi_edid(), &failures);
    hdmi_audio_ready = !find_hdmi_pcm_device(hdmi_pcm_device,
                                              sizeof(hdmi_pcm_device));
    report_check("vc4-hdmi-0 IEC958 playback device",
                 !hdmi_audio_ready, &failures);
    if (hdmi_audio_test) {
        report_check("HDMI0 48 kHz stereo MAI/DMA playback",
                     !hdmi_audio_ready ||
                     play_hdmi_audio(hdmi_pcm_device), &failures);
    }
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

    if (display_test) {
        report_check("VC4 scanout deterministic RGB565 pattern ready",
                     write_framebuffer_pattern(), &failures);
    }

    if (failures) {
        printf("\nPI4-LAB: acceptance failed (%d check%s)\n", failures,
               failures == 1 ? "" : "s");
    } else {
        printf("\nPI4-LAB: upstream Linux boot successful\n");
    }
    fflush(NULL);
    sync();
    sleep(display_test ? 5 : 1);

    if (reboot(RB_AUTOBOOT)) {
        fprintf(stderr, "pi4-lab: reboot failed: %s\n", strerror(errno));
    }

    for (;;) {
        pause();
    }
}
