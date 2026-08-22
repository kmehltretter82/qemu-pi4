/*
 * Raspberry Pi 4 machine integration tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "libqtest-single.h"
#include "qemu/bswap.h"
#include "qemu/iov.h"
#include "qemu/sockets.h"
#include "qobject/qdict.h"
#include "qobject/qlist.h"

#define RASPI4_MBOX_BASE       0xfe00b800
#define RASPI4_MBOX_READ       (RASPI4_MBOX_BASE + 0x80)
#define RASPI4_MBOX_WRITE      (RASPI4_MBOX_BASE + 0xa0)
#define RASPI4_PROPERTY_BUFFER 0x1000

#define RASPI4_GENET_BASE             0xfd580000
#define RASPI4_GENET_REV              (RASPI4_GENET_BASE + 0x0000)
#define RASPI4_GENET_INTRL2_0         (RASPI4_GENET_BASE + 0x0200)
#define RASPI4_GENET_IRQ_STAT         0x00
#define RASPI4_GENET_IRQ_CLEAR        0x08
#define RASPI4_GENET_IRQ_MASK_CLEAR   0x14
#define RASPI4_GENET_UMAC_CMD         (RASPI4_GENET_BASE + 0x0808)
#define RASPI4_GENET_MDIO_CMD         (RASPI4_GENET_BASE + 0x0e14)
#define RASPI4_GENET_RDMA_DESC        (RASPI4_GENET_BASE + 0x2000)
#define RASPI4_GENET_RDMA_RING        (RASPI4_GENET_BASE + 0x2c00)
#define RASPI4_GENET_RDMA_COMMON      (RASPI4_GENET_BASE + 0x3040)
#define RASPI4_GENET_TDMA_DESC        (RASPI4_GENET_BASE + 0x4000)
#define RASPI4_GENET_TDMA_RING        (RASPI4_GENET_BASE + 0x4c00)
#define RASPI4_GENET_TDMA_COMMON      (RASPI4_GENET_BASE + 0x5040)

#define RASPI4_GENET_GIC_IRQ0         157
#define RASPI4_GENET_GIC_IRQ1         158
#define GENET_DMA_RING_INDEX          0x08
#define GENET_DMA_RING_INDEX_UPDATE   0x0c
#define GENET_DMA_RING_BUF_SIZE       0x10
#define GENET_DMA_RING_START          0x14
#define GENET_DMA_RING_END            0x1c
#define GENET_DMA_CTRL                0x04
#define GENET_DMA_ENABLE              0x03
#define GENET_DMA_SOP                 0x2000
#define GENET_DMA_EOP                 0x4000
#define GENET_UMAC_TX_ENABLE          (1U << 0)
#define GENET_UMAC_RX_ENABLE          (1U << 1)
#define GENET_TX_IRQ                  (1U << 0)
#define GENET_RX_IRQ                  (1U << 16)
#define GENET_STATUS_BLOCK_SIZE       64
#define GENET_RX_PREFIX_SIZE          66
#define GENET_MDIO_BUSY               (1U << 29)
#define GENET_MDIO_FAIL               (1U << 28)
#define GENET_MDIO_READ               (2U << 26)
#define GENET_MDIO_DONE_IRQ           (1U << 23)
#define GENET_MDIO_ERROR_IRQ          (1U << 24)

#ifndef _WIN32
static int genet_test_socket = -1;
#endif

static bool qom_bus_has_sd_card(const char *path)
{
    QDict *response;
    QList *properties;
    QListEntry *entry;
    bool found = false;

    response = qtest_qmp(global_qtest,
                         "{ 'execute': 'qom-list',"
                         "  'arguments': { 'path': %s } }", path);
    g_assert(qdict_haskey(response, "return"));
    properties = qdict_get_qlist(response, "return");

    QLIST_FOREACH_ENTRY(properties, entry) {
        QDict *property = qobject_to(QDict, qlist_entry_obj(entry));

        if (g_str_equal(qdict_get_str(property, "type"), "link<sd-card>")) {
            found = true;
            break;
        }
    }

    qobject_unref(response);
    return found;
}

static void test_cpu_configuration(void)
{
    QDict *response;

    response = qtest_qmp(global_qtest,
                         "{ 'execute': 'qom-get',"
                         "  'arguments': {"
                         "    'path': '/machine/soc/cpu[0]',"
                         "    'property': 'cntfrq' } }");
    g_assert(qdict_haskey(response, "return"));
    g_assert_cmpint(qdict_get_int(response, "return"), ==, 54000000);
    qobject_unref(response);
}

static void test_sd_card_on_emmc2(void)
{
    g_assert_true(qom_bus_has_sd_card(
        "/machine/soc/peripherals/emmc2/sd-bus"));
    g_assert_false(qom_bus_has_sd_card(
        "/machine/soc/peripherals/sdhci/sd-bus"));
}

static void property_request(uint32_t tag, const uint32_t *payload,
                             size_t words, size_t response_bytes)
{
    uint32_t payload_bytes = words * sizeof(*payload);
    uint32_t total_bytes = 24 + payload_bytes;
    uint32_t response;
    size_t i;

    writel(RASPI4_PROPERTY_BUFFER, total_bytes);
    writel(RASPI4_PROPERTY_BUFFER + 4, 0);
    writel(RASPI4_PROPERTY_BUFFER + 8, tag);
    writel(RASPI4_PROPERTY_BUFFER + 12, payload_bytes);
    writel(RASPI4_PROPERTY_BUFFER + 16, 0);
    for (i = 0; i < words; i++) {
        writel(RASPI4_PROPERTY_BUFFER + 20 + i * 4, payload[i]);
    }
    writel(RASPI4_PROPERTY_BUFFER + 20 + payload_bytes, 0);

    writel(RASPI4_MBOX_WRITE,
           RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    response = readl(RASPI4_MBOX_READ);

    g_assert_cmphex(response, ==,
                    RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==, 0x80000000);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==,
                    0x80000000 | response_bytes);
}

static uint32_t property_payload(size_t word)
{
    return readl(RASPI4_PROPERTY_BUFFER + 20 + word * 4);
}

static void test_firmware_gpio(void)
{
    const uint32_t gpio = 132;
    const uint32_t get_config[] = { gpio, 0, 0, 0, 0 };
    const uint32_t set_config[] = { gpio, 1, 1, 1, 1, 1 };
    const uint32_t get_state[] = { gpio, 0 };
    const uint32_t clear_state[] = { gpio, 0 };

    property_request(RPI_FWREQ_GET_GPIO_CONFIG, get_config,
                     G_N_ELEMENTS(get_config), 20);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 0);
    g_assert_cmphex(property_payload(2), ==, 0);
    g_assert_cmphex(property_payload(3), ==, 0);
    g_assert_cmphex(property_payload(4), ==, 0);

    property_request(RPI_FWREQ_SET_GPIO_CONFIG, set_config,
                     G_N_ELEMENTS(set_config), 24);
    g_assert_cmphex(property_payload(0), ==, 0);

    property_request(RPI_FWREQ_GET_GPIO_CONFIG, get_config,
                     G_N_ELEMENTS(get_config), 20);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 1);
    g_assert_cmphex(property_payload(2), ==, 1);
    g_assert_cmphex(property_payload(3), ==, 1);
    g_assert_cmphex(property_payload(4), ==, 1);

    property_request(RPI_FWREQ_GET_GPIO_STATE, get_state,
                     G_N_ELEMENTS(get_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 1);

    property_request(RPI_FWREQ_SET_GPIO_STATE, clear_state,
                     G_N_ELEMENTS(clear_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);

    property_request(RPI_FWREQ_GET_GPIO_STATE, get_state,
                     G_N_ELEMENTS(get_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 0);
}

static void test_firmware_dma_channels(void)
{
    const uint32_t payload[] = { 0 };

    property_request(RPI_FWREQ_GET_DMA_CHANNELS, payload,
                     G_N_ELEMENTS(payload), sizeof(payload));
    g_assert_cmphex(property_payload(0), ==, 0x07f5);
}

static uint32_t genet_mdio_read(unsigned int phy, unsigned int reg)
{
    uint32_t command = GENET_MDIO_BUSY | GENET_MDIO_READ |
                       (phy << 21) | (reg << 16);

    writel(RASPI4_GENET_MDIO_CMD, command);
    return readl(RASPI4_GENET_MDIO_CMD);
}

static void test_genet_registers_and_mdio(void)
{
    uint32_t command;

    g_assert_cmphex(readl(RASPI4_GENET_REV), ==, 0x06000000);

    /* Disabled DMA engines report every corresponding disabled bit. */
    g_assert_cmphex(readl(RASPI4_GENET_RDMA_COMMON + 0x08), ==, 0x1ffff);
    g_assert_cmphex(readl(RASPI4_GENET_TDMA_COMMON + 0x08), ==, 0x1ffff);
    writel(RASPI4_GENET_RDMA_COMMON + 0x04, 0x3);
    g_assert_cmphex(readl(RASPI4_GENET_RDMA_COMMON + 0x08), ==, 0x1fffc);

    writel(RASPI4_GENET_INTRL2_0 + RASPI4_GENET_IRQ_MASK_CLEAR,
           GENET_MDIO_DONE_IRQ | GENET_MDIO_ERROR_IRQ);
    command = genet_mdio_read(1, 2);
    g_assert_false(command & (GENET_MDIO_BUSY | GENET_MDIO_FAIL));
    g_assert_cmphex(command & 0xffff, ==, 0x600d);
    g_assert_true(readl(RASPI4_GENET_INTRL2_0 + RASPI4_GENET_IRQ_STAT) &
                  GENET_MDIO_DONE_IRQ);
    g_assert_true(get_irq(RASPI4_GENET_GIC_IRQ0));

    writel(RASPI4_GENET_INTRL2_0 + RASPI4_GENET_IRQ_CLEAR,
           GENET_MDIO_DONE_IRQ);
    g_assert_false(get_irq(RASPI4_GENET_GIC_IRQ0));

    command = genet_mdio_read(1, 3);
    g_assert_cmphex(command & 0xffff, ==, 0x84a2);
    writel(RASPI4_GENET_INTRL2_0 + RASPI4_GENET_IRQ_CLEAR,
           GENET_MDIO_DONE_IRQ);

    command = genet_mdio_read(0, 2);
    g_assert_true(command & GENET_MDIO_FAIL);
    g_assert_cmphex(command & 0xffff, ==, 0xffff);
    g_assert_cmphex(readl(RASPI4_GENET_INTRL2_0 + RASPI4_GENET_IRQ_STAT) &
                    (GENET_MDIO_DONE_IRQ | GENET_MDIO_ERROR_IRQ), ==,
                    GENET_MDIO_DONE_IRQ | GENET_MDIO_ERROR_IRQ);
}

#ifndef _WIN32
static void genet_configure_ring(uint64_t ring, uint64_t common,
                                 unsigned int count)
{
    writel(ring + GENET_DMA_RING_INDEX, 0);
    writel(ring + GENET_DMA_RING_INDEX_UPDATE, 0);
    writel(ring + GENET_DMA_RING_BUF_SIZE, (count << 16) | 2048);
    writel(ring + GENET_DMA_RING_START, 0);
    writel(ring + GENET_DMA_RING_END, count * 3 - 1);
    writel(common, 1);
    writel(common + GENET_DMA_CTRL, GENET_DMA_ENABLE);
}

static void genet_recv_all(void *buf, size_t size)
{
    uint8_t *p = buf;

    while (size) {
        GPollFD pollfd = {
            .fd = genet_test_socket,
            .events = G_IO_IN | G_IO_HUP | G_IO_ERR,
        };
        ssize_t ret;

        g_assert_cmpint(g_poll(&pollfd, 1, 5000), >, 0);
        ret = recv(genet_test_socket, p, size, 0);
        g_assert_cmpint(ret, >, 0);
        p += ret;
        size -= ret;
    }
}

static void test_genet_packet_dma(void)
{
    static const uint8_t frame[64] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x52, 0x54, 0x00, 0x12, 0x34, 0x56,
        0x08, 0x00,
        0x45, 0x00, 0x00, 0x32, 0x00, 0x00, 0x40, 0x00,
        0x40, 0x11, 0x00, 0x00, 0x0a, 0x00, 0x02, 0x0f,
        0x0a, 0x00, 0x02, 0x02, 0x04, 0xd2, 0x00, 0x09,
        0x00, 0x1e, 0x00, 0x00,
        'q', 'e', 'm', 'u', '-', 'p', 'i', '4', '-', 'g', 'e', 'n', 'e', 't',
    };
    const uint64_t tx_buffer = 0x20000;
    const uint64_t rx_buffer = 0x30000;
    uint8_t tx_dma[GENET_STATUS_BLOCK_SIZE + sizeof(frame)] = { 0 };
    uint8_t received[sizeof(frame)];
    uint32_t frame_len;
    uint32_t length_status;
    int net_len = htonl(sizeof(frame));
    struct iovec iov[] = {
        { .iov_base = &net_len, .iov_len = sizeof(net_len) },
        { .iov_base = (void *)frame, .iov_len = sizeof(frame) },
    };
    int ret;

    memcpy(tx_dma + GENET_STATUS_BLOCK_SIZE, frame, sizeof(frame));
    memwrite(tx_buffer, tx_dma, sizeof(tx_dma));
    writel(RASPI4_GENET_TDMA_DESC,
           (sizeof(tx_dma) << 16) | GENET_DMA_SOP | GENET_DMA_EOP);
    writel(RASPI4_GENET_TDMA_DESC + 4, tx_buffer);
    writel(RASPI4_GENET_TDMA_DESC + 8, 0);
    genet_configure_ring(RASPI4_GENET_TDMA_RING,
                         RASPI4_GENET_TDMA_COMMON, 128);
    writel(RASPI4_GENET_BASE + 0x0240 + RASPI4_GENET_IRQ_MASK_CLEAR,
           GENET_TX_IRQ);
    writel(RASPI4_GENET_UMAC_CMD, GENET_UMAC_TX_ENABLE);

    writel(RASPI4_GENET_TDMA_RING + GENET_DMA_RING_INDEX_UPDATE, 1);
    genet_recv_all(&frame_len, sizeof(frame_len));
    g_assert_cmpuint(ntohl(frame_len), ==, sizeof(frame));
    genet_recv_all(received, sizeof(received));
    g_assert_cmpmem(received, sizeof(received), frame, sizeof(frame));
    g_assert_cmphex(readl(RASPI4_GENET_TDMA_RING + GENET_DMA_RING_INDEX),
                    ==, 1);
    g_assert_true(get_irq(RASPI4_GENET_GIC_IRQ1));
    writel(RASPI4_GENET_BASE + 0x0240 + RASPI4_GENET_IRQ_CLEAR,
           GENET_TX_IRQ);
    g_assert_false(get_irq(RASPI4_GENET_GIC_IRQ1));

    writel(RASPI4_GENET_RDMA_DESC + 4, rx_buffer);
    writel(RASPI4_GENET_RDMA_DESC + 8, 0);
    genet_configure_ring(RASPI4_GENET_RDMA_RING,
                         RASPI4_GENET_RDMA_COMMON, 256);
    writel(RASPI4_GENET_BASE + 0x0240 + RASPI4_GENET_IRQ_MASK_CLEAR,
           GENET_RX_IRQ);
    writel(RASPI4_GENET_UMAC_CMD,
           GENET_UMAC_TX_ENABLE | GENET_UMAC_RX_ENABLE);

    ret = iov_send(genet_test_socket, iov, 2, 0,
                   sizeof(net_len) + sizeof(frame));
    g_assert_cmpint(ret, ==, sizeof(net_len) + sizeof(frame));
    qtest_qmp_assert_success(global_qtest,
                             "{ 'execute': 'query-status' }");
    for (unsigned int i = 0;
         i < 1000 &&
         readl(RASPI4_GENET_RDMA_RING + GENET_DMA_RING_INDEX) == 0;
         i++) {
        qtest_clock_step(global_qtest, 100);
    }

    g_assert_cmphex(readl(RASPI4_GENET_RDMA_RING + GENET_DMA_RING_INDEX),
                    ==, 1);
    memread(rx_buffer, &length_status, sizeof(length_status));
    length_status = le32_to_cpu(length_status);
    g_assert_cmphex(length_status, ==,
                    ((sizeof(frame) + GENET_RX_PREFIX_SIZE) << 16) |
                    GENET_DMA_SOP | GENET_DMA_EOP | 0x40);
    memread(rx_buffer + GENET_RX_PREFIX_SIZE, received, sizeof(received));
    g_assert_cmpmem(received, sizeof(received), frame, sizeof(frame));
    g_assert_true(get_irq(RASPI4_GENET_GIC_IRQ1));
    writel(RASPI4_GENET_BASE + 0x0240 + RASPI4_GENET_IRQ_CLEAR,
           GENET_RX_IRQ);
    g_assert_false(get_irq(RASPI4_GENET_GIC_IRQ1));
}
#endif /* _WIN32 */

int main(int argc, char **argv)
{
    int ret;
#ifndef _WIN32
    int test_sockets[2];
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
#endif

    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/raspi4b/cpu/configuration", test_cpu_configuration);
    qtest_add_func("/raspi4b/sd/card_on_emmc2", test_sd_card_on_emmc2);
    qtest_add_func("/raspi4b/firmware_gpio", test_firmware_gpio);
    qtest_add_func("/raspi4b/firmware_dma_channels",
                   test_firmware_dma_channels);
    qtest_add_func("/raspi4b/genet/registers_and_mdio",
                   test_genet_registers_and_mdio);
#ifndef _WIN32
    qtest_add_func("/raspi4b/genet/packet_dma", test_genet_packet_dma);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, test_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(cmd_line, " -nic socket,fd=%d,model=genet",
                           test_sockets[1]);
    genet_test_socket = test_sockets[0];
    qtest_start(cmd_line->str);
    close(test_sockets[1]);
#else
    qtest_start("-machine raspi4b");
#endif

    qtest_irq_intercept_in(global_qtest, "/machine/soc/peripherals");
    ret = g_test_run();
    qtest_end();
#ifndef _WIN32
    close(genet_test_socket);
#endif

    return ret;
}
