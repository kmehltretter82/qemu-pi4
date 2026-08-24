/*
 * Raspberry Pi 400 machine integration tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "libqtest-single.h"
#include "qemu/units.h"
#include "qobject/qdict.h"

#define RASPI4_MBOX_BASE       0xfe00b800
#define RASPI4_MBOX_READ       (RASPI4_MBOX_BASE + 0x80)
#define RASPI4_MBOX_WRITE      (RASPI4_MBOX_BASE + 0xa0)
#define RASPI4_PROPERTY_BUFFER 0x1000

#define RASPI400_BOARD_REVISION 0xc03130
#define RASPI4_BOARD_SERIAL      0x51454d55
#define RASPI400_RAM_SIZE       (4ULL * GiB)
#define RASPI400_UPPER_RAM_LAST 0xfbfffffc
#define RASPI400_VL805_PCI_ADDR (1U << 20)

static void property_request_qtest(QTestState *qts, uint32_t tag,
                                   uint64_t value, uint32_t value_size)
{
    uint32_t response;

    g_assert_true(value_size == sizeof(uint32_t) ||
                  value_size == sizeof(uint64_t));

    qtest_writel(qts, RASPI4_PROPERTY_BUFFER, 24 + value_size);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 4, 0);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 8, tag);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 12, value_size);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 16, 0);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 20, value);
    if (value_size == sizeof(uint64_t)) {
        qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 24, value >> 32);
    }
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 20 + value_size, 0);

    qtest_writel(qts, RASPI4_MBOX_WRITE,
                 RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    response = qtest_readl(qts, RASPI4_MBOX_READ);

    g_assert_cmphex(response, ==,
                    RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 4), ==,
                    0x80000000);
    g_assert_cmphex(qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 16), ==,
                    0x80000000 | value_size);
}

static void property_request(uint32_t tag, uint64_t value,
                             uint32_t value_size)
{
    property_request_qtest(global_qtest, tag, value, value_size);
}

static void test_board_identity(void)
{
    QTestState *custom;

    property_request(RPI_FWREQ_GET_BOARD_MODEL, UINT32_MAX,
                     sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);

    property_request(RPI_FWREQ_GET_BOARD_REVISION, 0, sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==,
                    RASPI400_BOARD_REVISION);

    property_request(RPI_FWREQ_GET_BOARD_SERIAL, 0, sizeof(uint64_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==,
                    RASPI4_BOARD_SERIAL);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==, 0);

    qtest_system_reset(global_qtest);
    property_request(RPI_FWREQ_GET_BOARD_REVISION, 0, sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==,
                    RASPI400_BOARD_REVISION);
    property_request(RPI_FWREQ_GET_BOARD_SERIAL, 0, sizeof(uint64_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==,
                    RASPI4_BOARD_SERIAL);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==, 0);

    custom = qtest_init(
        "-machine raspi400 "
        "-global bcm2835-property.board-serial=0x12345678");
    property_request_qtest(custom, RPI_FWREQ_GET_BOARD_SERIAL, 0,
                           sizeof(uint64_t));
    g_assert_cmphex(qtest_readl(custom, RASPI4_PROPERTY_BUFFER + 20), ==,
                    0x12345678);
    g_assert_cmphex(qtest_readl(custom, RASPI4_PROPERTY_BUFFER + 24), ==, 0);
    qtest_quit(custom);
}

static void test_notify_xhci_reset(void)
{
    property_request(RPI_FWREQ_NOTIFY_XHCI_RESET,
                     RASPI400_VL805_PCI_ADDR, sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);

    property_request(RPI_FWREQ_NOTIFY_XHCI_RESET, 0, sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, UINT32_MAX);
}

static void test_default_ram_size(void)
{
    QDict *response;
    QDict *summary;

    response = qtest_qmp(global_qtest,
                         "{ 'execute': 'query-memory-size-summary' }");
    g_assert(qdict_haskey(response, "return"));
    summary = qdict_get_qdict(response, "return");
    g_assert_cmpint(qdict_get_int(summary, "base-memory"), ==,
                    RASPI400_RAM_SIZE);
    qobject_unref(response);
}

static void test_upper_ram_mapping(void)
{
    const uint32_t pattern = 0x51454d55;

    writel(RASPI400_UPPER_RAM_LAST, pattern);
    g_assert_cmphex(readl(RASPI400_UPPER_RAM_LAST), ==, pattern);
}

static void test_usb_topology(void)
{
    g_autofree char *usb = qtest_hmp(global_qtest, "info usb");
    g_autofree char *qtree = qtest_hmp(global_qtest, "info qtree");

    g_assert_nonnull(strstr(usb,
        "Port 1, Speed 480 Mb/s, Product USB2.0 Hub"));
    g_assert_nonnull(strstr(usb,
        "Port 1.4, Speed 1.5 Mb/s, Product Raspberry Pi Internal Keyboard"));
    g_assert_nonnull(strstr(qtree, "dev: usb-via-3431-hub"));
    g_assert_nonnull(strstr(qtree, "dev: usb-pi400-keyboard"));
}

int main(int argc, char **argv)
{
    int ret;

    g_test_init(&argc, &argv, NULL);
#if HOST_LONG_BITS == 32
    g_test_message("raspi400 requires a 64-bit host");
    return 0;
#endif
    qtest_add_func("/raspi400/firmware/board_identity",
                   test_board_identity);
    qtest_add_func("/raspi400/firmware/notify_xhci_reset",
                   test_notify_xhci_reset);
    qtest_add_func("/raspi400/memory/default_size", test_default_ram_size);
    qtest_add_func("/raspi400/memory/upper_mapping",
                   test_upper_ram_mapping);
    qtest_add_func("/raspi400/usb/topology", test_usb_topology);

    qtest_start("-machine raspi400");
    ret = g_test_run();
    qtest_end();

    return ret;
}
