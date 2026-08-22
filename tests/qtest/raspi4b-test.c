/*
 * Raspberry Pi 4 machine integration tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "hw/pci/pci_ids.h"
#include "hw/pci/pci_regs.h"
#include "libqtest-single.h"
#include "qemu/bswap.h"
#include "qemu/iov.h"
#include "qemu/sockets.h"
#include "qemu/timer.h"
#include "qobject/qdict.h"
#include "qobject/qlist.h"

#define RASPI4_MBOX_BASE       0xfe00b800
#define RASPI4_MBOX_READ       (RASPI4_MBOX_BASE + 0x80)
#define RASPI4_MBOX_WRITE      (RASPI4_MBOX_BASE + 0xa0)
#define RASPI4_PROPERTY_BUFFER 0x1000

#define RASPI4_PM_BASE          0xfe100000
#define RASPI4_PM_RSTC          (RASPI4_PM_BASE + 0x1c)
#define RASPI4_PM_RSTS          (RASPI4_PM_BASE + 0x20)
#define RASPI4_PM_WDOG          (RASPI4_PM_BASE + 0x24)
#define RASPI4_PM_PASSWORD      0x5a000000
#define RASPI4_PM_RSTC_STOP     0x00000102
#define RASPI4_PM_RSTC_FULL     0x00000020
#define RASPI4_PM_RSTS_DEFAULT  0x00001000
#define RASPI4_PM_WDOG_HZ       65536

#define RASPI4_ASB_BASE         0xfe00a000
#define RASPI4_RPIVID_ASB_BASE  0xfec11000
#define ASB_AXI_BRDG_ID         0x20
#define BCM2835_BRDG_ID         0x62726467

#define RASPI4_SYSTIMER_BASE      0xfe003000
#define RASPI4_SYSTIMER_CS        (RASPI4_SYSTIMER_BASE + 0x00)
#define RASPI4_SYSTIMER_CLO       (RASPI4_SYSTIMER_BASE + 0x04)
#define RASPI4_SYSTIMER_COMPARE_0 (RASPI4_SYSTIMER_BASE + 0x0c)
#define RASPI4_SYSTIMER_GIC_IRQ_0 64

#define RASPI4_SPI0_BASE          0xfe204000
#define RASPI4_SPI0_CS            RASPI4_SPI0_BASE
#define RASPI4_SPI0_GIC_IRQ       118
#define SPI0_CS_DONE              (1U << 16)
#define SPI0_CS_INTD              (1U << 9)
#define SPI0_CS_TA                (1U << 7)

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

#define RASPI4_PCIE_BASE             0xfd500000ULL
#define RASPI4_PCIE_MDIO_ADDR        (RASPI4_PCIE_BASE + 0x1100)
#define RASPI4_PCIE_MDIO_WR_DATA     (RASPI4_PCIE_BASE + 0x1104)
#define RASPI4_PCIE_MDIO_RD_DATA     (RASPI4_PCIE_BASE + 0x1108)
#define RASPI4_PCIE_MISC_CTRL        (RASPI4_PCIE_BASE + 0x4008)
#define RASPI4_PCIE_OUT_LO           (RASPI4_PCIE_BASE + 0x400c)
#define RASPI4_PCIE_OUT_HI           (RASPI4_PCIE_BASE + 0x4010)
#define RASPI4_PCIE_RC_BAR2_LO       (RASPI4_PCIE_BASE + 0x4034)
#define RASPI4_PCIE_RC_BAR2_HI       (RASPI4_PCIE_BASE + 0x4038)
#define RASPI4_PCIE_MSI_BAR_LO       (RASPI4_PCIE_BASE + 0x4044)
#define RASPI4_PCIE_MSI_BAR_HI       (RASPI4_PCIE_BASE + 0x4048)
#define RASPI4_PCIE_MSI_DATA         (RASPI4_PCIE_BASE + 0x404c)
#define RASPI4_PCIE_STATUS           (RASPI4_PCIE_BASE + 0x4068)
#define RASPI4_PCIE_MISC_REVISION    (RASPI4_PCIE_BASE + 0x406c)
#define RASPI4_PCIE_BASE_LIMIT       (RASPI4_PCIE_BASE + 0x4070)
#define RASPI4_PCIE_BASE_HI          (RASPI4_PCIE_BASE + 0x4080)
#define RASPI4_PCIE_LIMIT_HI         (RASPI4_PCIE_BASE + 0x4084)
#define RASPI4_PCIE_EXT_CFG_DATA     (RASPI4_PCIE_BASE + 0x8000)
#define RASPI4_PCIE_EXT_CFG_INDEX    (RASPI4_PCIE_BASE + 0x9000)
#define RASPI4_PCIE_SW_INIT          (RASPI4_PCIE_BASE + 0x9210)
#define RASPI4_PCIE_MSI_STATUS       (RASPI4_PCIE_BASE + 0x4500)
#define RASPI4_PCIE_MSI_CLEAR        (RASPI4_PCIE_BASE + 0x4508)
#define RASPI4_PCIE_MSI_MASK_STATUS  (RASPI4_PCIE_BASE + 0x450c)
#define RASPI4_PCIE_MSI_MASK_SET     (RASPI4_PCIE_BASE + 0x4510)
#define RASPI4_PCIE_MSI_MASK_CLEAR   (RASPI4_PCIE_BASE + 0x4514)
#define RASPI4_PCIE_VENDOR_REG1      (RASPI4_PCIE_BASE + 0x0188)
#define RASPI4_PCIE_ID_VAL3          (RASPI4_PCIE_BASE + 0x043c)
#define RASPI4_PCIE_PRIV_LINK_CAP    (RASPI4_PCIE_BASE + 0x04dc)
#define RASPI4_PCIE_PRIV_ROOT_CAP    (RASPI4_PCIE_BASE + 0x04f8)

#define PCIE_SW_INIT_PERST           (1U << 0)
#define PCIE_SW_INIT_BRIDGE          (1U << 1)
#define PCIE_SW_INIT_MASK            (PCIE_SW_INIT_PERST | \
                                      PCIE_SW_INIT_BRIDGE)
#define PCIE_STATUS_PHY_LINK_UP      (1U << 4)
#define PCIE_STATUS_DL_ACTIVE        (1U << 5)
#define PCIE_STATUS_RC_MODE          (1U << 7)
#define PCIE_STATUS_LINK_MASK        (PCIE_STATUS_PHY_LINK_UP | \
                                      PCIE_STATUS_DL_ACTIVE)
#define PCIE_STATUS_TEST_MASK        (PCIE_STATUS_RC_MODE | \
                                      PCIE_STATUS_LINK_MASK)
#define PCIE_MISC_CTRL_SCB_ACCESS_EN (1U << 12)

#define BCM2711_PCIE_ID              0x271114e4U
#define BCM2711_PCIE_REVISION        0x20U
#define BCM2711_PCIE_CLASS_REV       \
    (((uint32_t)PCI_CLASS_BRIDGE_PCI << 16) | BCM2711_PCIE_REVISION)
#define BCM2711_PCIE_HW_REVISION     0x0320U
#define BCM2711_PCIE_MDIO_DONE       (1U << 31)
#define BCM2711_PCIE_MDIO_SSC_PLL    ((1U << 10) | (1U << 11))

#define RASPI4_PCIE_CPU_WINDOW       0x600000000ULL
#define RASPI4_PCIE_EDU_PCI_BAR      0xf8000000U
#define RASPI4_PCIE_EDU_CPU_BAR      RASPI4_PCIE_CPU_WINDOW
#define RASPI4_PCIE_EDU_ID           0x11e81234U
#define RASPI4_PCIE_EDU_MAGIC        0x010000edU
#define RASPI4_PCIE_INTX_FIRST       143
#define RASPI4_PCIE_INTX_LAST        146
#define RASPI4_PCIE_EDU_GIC_INTX     144
#define RASPI4_PCIE_EDU_GIC_MSI      148

#define RASPI4_PCIE_VL805_PCI_BAR    0xf8000000U
#define RASPI4_PCIE_VL805_CPU_BAR    RASPI4_PCIE_CPU_WINDOW
#define RASPI4_PCIE_VL805_ID         0x34831106U
#define RASPI4_PCIE_VL805_CLASS_REV  0x0c033001U
#define RASPI4_PCIE_VL805_PM_CAP     0x80
#define RASPI4_PCIE_VL805_MSI_CAP    0x90
#define RASPI4_PCIE_VL805_PCIE_CAP   0xc4
#define RASPI4_PCIE_VL805_AER_CAP    0x100
#define RASPI4_PCIE_VL805_GIC_MSI    148

#define VL805_XHCI_CAPLENGTH          0x20
#define VL805_XHCI_DOORBELL           0x100
#define VL805_XHCI_RUNTIME            0x200
#define VL805_XHCI_PORT1              0x420
#define VL805_XHCI_USBCMD             (VL805_XHCI_CAPLENGTH + 0x00)
#define VL805_XHCI_USBSTS             (VL805_XHCI_CAPLENGTH + 0x04)
#define VL805_XHCI_IMAN0              (VL805_XHCI_RUNTIME + 0x20)
#define VL805_XHCI_ERSTSZ0            (VL805_XHCI_RUNTIME + 0x28)
#define VL805_XHCI_ERSTBA0            (VL805_XHCI_RUNTIME + 0x30)
#define VL805_XHCI_ERDP0              (VL805_XHCI_RUNTIME + 0x38)
#define VL805_XHCI_USBCMD_RUN         (1U << 0)
#define VL805_XHCI_USBCMD_INTE        (1U << 2)
#define VL805_XHCI_USBSTS_HCH         (1U << 0)
#define VL805_XHCI_IMAN_IE            (1U << 1)
#define VL805_XHCI_PORTSC_CCS         (1U << 0)
#define VL805_XHCI_PORTSC_SPEED_MASK  (0xfU << 10)
#define VL805_XHCI_PORTSC_SPEED_HIGH  (3U << 10)
#define VL805_XHCI_PORTSC_CSC         (1U << 17)
#define VL805_XHCI_TRB_CYCLE          (1U << 0)
#define VL805_XHCI_TRB_TYPE_SHIFT     10
#define VL805_XHCI_TRB_PORT_STATUS    34U
#define VL805_XHCI_CC_SUCCESS         1U
#define VL805_XHCI_ERST_CPU           0x20000ULL
#define VL805_XHCI_EVENT_RING_CPU     0x21000ULL
#define VL805_XHCI_EVENT_RING_TRBS    16U

#define EDU_IRQ_STATUS               0x24
#define EDU_IRQ_RAISE                0x60
#define EDU_IRQ_ACK                  0x64
#define EDU_DMA_SRC                  0x80
#define EDU_DMA_DST                  0x88
#define EDU_DMA_COUNT                0x90
#define EDU_DMA_CMD                  0x98
#define EDU_DMA_BUFFER               0x40000
#define EDU_DMA_RUN                  (1U << 0)
#define EDU_DMA_TO_PCI               (1U << 1)

#define PCIE_DMA_BASE                0x400000000ULL
#define PCIE_DMA_SIZE_ENCODING_256M  0x0d

#ifndef _WIN32
static int genet_test_socket = -1;
#endif
static bool pcie_has_edu;

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

static uint32_t pcie_cfg_index(unsigned int bus, unsigned int slot,
                               unsigned int function)
{
    return (bus << 20) | (slot << 15) | (function << 12);
}

static void pcie_assert_link(bool up)
{
    uint32_t expected = PCIE_STATUS_RC_MODE;

    if (up) {
        expected |= PCIE_STATUS_LINK_MASK;
    }

    g_assert_cmphex(readl(RASPI4_PCIE_STATUS) & PCIE_STATUS_TEST_MASK,
                    ==, expected);
}

static void pcie_assert_root_identity(void)
{
    g_assert_cmphex(readl(RASPI4_PCIE_BASE + PCI_VENDOR_ID),
                    ==, BCM2711_PCIE_ID);
    g_assert_cmphex(readl(RASPI4_PCIE_BASE + PCI_CLASS_REVISION),
                    ==, BCM2711_PCIE_CLASS_REV);
    g_assert_cmphex(readb(RASPI4_PCIE_BASE + PCI_HEADER_TYPE) &
                    PCI_HEADER_TYPE_MASK, ==, PCI_HEADER_TYPE_BRIDGE);
    g_assert_cmphex(readb(RASPI4_PCIE_BASE + 0xac), ==, PCI_CAP_ID_EXP);
    g_assert_cmphex(readl(RASPI4_PCIE_MISC_REVISION), ==,
                    BCM2711_PCIE_HW_REVISION);
    g_assert_cmphex(readl(RASPI4_PCIE_VENDOR_REG1), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_ID_VAL3), ==, 0x00060400);
    g_assert_cmphex(readl(RASPI4_PCIE_PRIV_LINK_CAP), ==, 0x10);
    g_assert_cmphex(readl(RASPI4_PCIE_PRIV_ROOT_CAP), ==, 0);
}

static void pcie_assert_qmp_root(uint8_t primary, uint8_t secondary,
                                 uint8_t subordinate)
{
    g_autoptr(QDict) response =
        qtest_qmp(global_qtest, "{ 'execute': 'query-pci' }");
    QList *buses = qdict_get_qlist(response, "return");
    QListEntry *bus_entry;

    QLIST_FOREACH_ENTRY(buses, bus_entry) {
        QDict *bus = qobject_to(QDict, qlist_entry_obj(bus_entry));
        QList *devices;
        QListEntry *dev_entry;

        if (qdict_get_int(bus, "bus") != 0) {
            continue;
        }

        devices = qdict_get_qlist(bus, "devices");
        QLIST_FOREACH_ENTRY(devices, dev_entry) {
            QDict *dev = qobject_to(QDict, qlist_entry_obj(dev_entry));
            QDict *id;
            QDict *class_info;
            QDict *bridge;
            QDict *bus_info;

            if (qdict_get_int(dev, "slot") != 0 ||
                qdict_get_int(dev, "function") != 0) {
                continue;
            }

            id = qdict_get_qdict(dev, "id");
            class_info = qdict_get_qdict(dev, "class_info");
            bridge = qdict_get_qdict(dev, "pci_bridge");
            bus_info = qdict_get_qdict(bridge, "bus");

            g_assert_cmpint(qdict_get_int(id, "vendor"), ==, 0x14e4);
            g_assert_cmpint(qdict_get_int(id, "device"), ==, 0x2711);
            g_assert_cmpint(qdict_get_int(class_info, "class"), ==,
                            PCI_CLASS_BRIDGE_PCI);
            g_assert_cmpint(qdict_get_int(bus_info, "number"), ==, primary);
            g_assert_cmpint(qdict_get_int(bus_info, "secondary"), ==,
                            secondary);
            g_assert_cmpint(qdict_get_int(bus_info, "subordinate"), ==,
                            subordinate);
            return;
        }
    }

    g_assert_not_reached();
}

static void pcie_select_endpoint(unsigned int slot)
{
    writel(RASPI4_PCIE_EXT_CFG_INDEX, pcie_cfg_index(1, slot, 0));
}

static uint8_t pcie_endpoint_cfg_readb(unsigned int slot, unsigned int offset)
{
    pcie_select_endpoint(slot);
    return readb(RASPI4_PCIE_EXT_CFG_DATA + offset);
}

static uint16_t pcie_endpoint_cfg_readw(unsigned int slot,
                                        unsigned int offset)
{
    pcie_select_endpoint(slot);
    return readw(RASPI4_PCIE_EXT_CFG_DATA + offset);
}

static uint32_t pcie_endpoint_cfg_readl(unsigned int slot,
                                        unsigned int offset)
{
    pcie_select_endpoint(slot);
    return readl(RASPI4_PCIE_EXT_CFG_DATA + offset);
}

static void pcie_endpoint_cfg_writew(unsigned int slot, unsigned int offset,
                                     uint16_t value)
{
    pcie_select_endpoint(slot);
    writew(RASPI4_PCIE_EXT_CFG_DATA + offset, value);
}

static void pcie_endpoint_cfg_writel(unsigned int slot, unsigned int offset,
                                     uint32_t value)
{
    pcie_select_endpoint(slot);
    writel(RASPI4_PCIE_EXT_CFG_DATA + offset, value);
}

static uint8_t pcie_endpoint_find_capability(unsigned int slot, uint8_t cap_id)
{
    uint8_t offset = pcie_endpoint_cfg_readb(slot, PCI_CAPABILITY_LIST) & ~3U;
    unsigned int ttl = 48;

    while (offset >= 0x40 && ttl--) {
        if (pcie_endpoint_cfg_readb(slot, offset + PCI_CAP_LIST_ID) == cap_id) {
            return offset;
        }
        offset = pcie_endpoint_cfg_readb(slot,
                                         offset + PCI_CAP_LIST_NEXT) & ~3U;
    }
    return 0;
}

#define pcie_edu_cfg_readb(offset) \
    pcie_endpoint_cfg_readb(1, (offset))
#define pcie_edu_cfg_readw(offset) \
    pcie_endpoint_cfg_readw(1, (offset))
#define pcie_edu_cfg_readl(offset) \
    pcie_endpoint_cfg_readl(1, (offset))
#define pcie_edu_cfg_writew(offset, value) \
    pcie_endpoint_cfg_writew(1, (offset), (value))
#define pcie_edu_cfg_writel(offset, value) \
    pcie_endpoint_cfg_writel(1, (offset), (value))
#define pcie_vl805_cfg_readb(offset) \
    pcie_endpoint_cfg_readb(0, (offset))
#define pcie_vl805_cfg_readw(offset) \
    pcie_endpoint_cfg_readw(0, (offset))
#define pcie_vl805_cfg_readl(offset) \
    pcie_endpoint_cfg_readl(0, (offset))
#define pcie_vl805_cfg_writew(offset, value) \
    pcie_endpoint_cfg_writew(0, (offset), (value))
#define pcie_vl805_cfg_writel(offset, value) \
    pcie_endpoint_cfg_writel(0, (offset), (value))

static bool pcie_edu_test_start(void)
{
    const uint16_t command = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

    if (!pcie_has_edu) {
        g_test_skip("QEMU was built without the edu PCI test endpoint");
        return false;
    }

    qtest_system_reset(global_qtest);
    writel(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS, 0x00010100);
    pcie_select_endpoint(1);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA), ==, UINT32_MAX);

    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);
    g_assert_cmphex(pcie_edu_cfg_readl(PCI_VENDOR_ID), ==,
                    RASPI4_PCIE_EDU_ID);

    pcie_edu_cfg_writel(PCI_BASE_ADDRESS_0, UINT32_MAX);
    g_assert_cmphex(pcie_edu_cfg_readl(PCI_BASE_ADDRESS_0) &
                    PCI_BASE_ADDRESS_MEM_MASK, ==, 0xfff00000);
    pcie_edu_cfg_writel(PCI_BASE_ADDRESS_0, RASPI4_PCIE_EDU_PCI_BAR);

    /* Forward the endpoint's 1 MiB BAR through the root port. */
    writel(RASPI4_PCIE_BASE + PCI_MEMORY_BASE, 0xf800f800);
    writew(RASPI4_PCIE_BASE + PCI_COMMAND, command);
    pcie_edu_cfg_writew(PCI_COMMAND, command);

    /* Linux 7.2 DT: CPU 0x600000000 -> PCI 0xf8000000, 64 MiB. */
    writel(RASPI4_PCIE_OUT_LO, RASPI4_PCIE_EDU_PCI_BAR);
    writel(RASPI4_PCIE_OUT_HI, 0);
    writel(RASPI4_PCIE_BASE_LIMIT, 0x03f00000);
    writel(RASPI4_PCIE_BASE_HI, 6);
    writel(RASPI4_PCIE_LIMIT_HI, 6);

    g_assert_cmphex(readl(RASPI4_PCIE_EDU_CPU_BAR), ==,
                    RASPI4_PCIE_EDU_MAGIC);
    return true;
}

static void pcie_vl805_test_start(void)
{
    const uint16_t command = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

    qtest_system_reset(global_qtest);
    writel(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS, 0x00010100);
    pcie_select_endpoint(0);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA), ==, UINT32_MAX);

    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_VENDOR_ID), ==,
                    RASPI4_PCIE_VL805_ID);

    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_0, UINT32_MAX);
    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_1, UINT32_MAX);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_0) &
                    PCI_BASE_ADDRESS_MEM_MASK, ==, 0xfffff000);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_1), ==,
                    UINT32_MAX);
    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_0,
                          RASPI4_PCIE_VL805_PCI_BAR);
    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_1, 0);

    /* Forward the endpoint's 4 KiB BAR through the root port. */
    writel(RASPI4_PCIE_BASE + PCI_MEMORY_BASE, 0xf800f800);
    writew(RASPI4_PCIE_BASE + PCI_COMMAND, command);
    pcie_vl805_cfg_writew(PCI_COMMAND, command);

    /* Linux v7.2 DT: CPU 0x600000000 -> PCI 0xf8000000, 64 MiB. */
    writel(RASPI4_PCIE_OUT_LO, RASPI4_PCIE_VL805_PCI_BAR);
    writel(RASPI4_PCIE_OUT_HI, 0);
    writel(RASPI4_PCIE_BASE_LIMIT, 0x03f00000);
    writel(RASPI4_PCIE_BASE_HI, 6);
    writel(RASPI4_PCIE_LIMIT_HI, 6);

    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR), ==, 0x01000020);
}

static void pcie_edu_dma(uint64_t src, uint64_t dst, size_t size,
                         uint32_t command)
{
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_SRC, src);
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_DST, dst);
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_COUNT, size);
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_CMD,
           command | EDU_DMA_RUN);
    qtest_clock_step(global_qtest, 101 * 1000 * 1000);
    g_assert_false(readq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_CMD) &
                   EDU_DMA_RUN);
}

static void test_pcie_root_config(void)
{
    const uint32_t initial_buses = 0x00080100;
    const uint32_t final_buses = 0x00080700;
    const uint16_t command = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

    qtest_system_reset(global_qtest);
    pcie_assert_root_identity();

    writel(RASPI4_PCIE_BASE + PCI_VENDOR_ID, 0);
    writel(RASPI4_PCIE_BASE + PCI_CLASS_REVISION, 0);
    pcie_assert_root_identity();

    writel(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS, initial_buses);
    writeb(RASPI4_PCIE_BASE + PCI_SECONDARY_BUS, 7);
    g_assert_cmphex(readl(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS), ==,
                    final_buses);

    writew(RASPI4_PCIE_BASE + PCI_COMMAND, command);
    g_assert_cmphex(readw(RASPI4_PCIE_BASE + PCI_COMMAND), ==, command);

    writel(RASPI4_PCIE_SW_INIT, 0);
    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_PERST);
    g_assert_cmphex(readl(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS), ==,
                    final_buses);
    g_assert_cmphex(readw(RASPI4_PCIE_BASE + PCI_COMMAND), ==, command);

    pcie_assert_qmp_root(0, 7, 8);
    qtest_system_reset(global_qtest);
}

static void test_pcie_reset_link_and_mdio(void)
{
    qtest_system_reset(global_qtest);

    g_assert_cmphex(readl(RASPI4_PCIE_SW_INIT), ==, PCIE_SW_INIT_MASK);
    pcie_assert_link(false);

    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_PERST);
    pcie_assert_link(false);
    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);

    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_PERST);
    pcie_assert_link(false);
    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);
    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_BRIDGE);
    pcie_assert_link(false);
    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);

    writel(RASPI4_PCIE_MDIO_ADDR, (1U << 20) | 1);
    g_assert_cmphex(readl(RASPI4_PCIE_MDIO_RD_DATA), ==,
                    BCM2711_PCIE_MDIO_DONE | BCM2711_PCIE_MDIO_SSC_PLL);
    writel(RASPI4_PCIE_MDIO_WR_DATA, BCM2711_PCIE_MDIO_DONE | 0x1234);
    g_assert_cmphex(readl(RASPI4_PCIE_MDIO_WR_DATA), ==, 0x1234);

    qtest_system_reset(global_qtest);
}

static void test_pcie_indirect_absent(void)
{
    const uint32_t index = pcie_cfg_index(1, 31, 7);

    qtest_system_reset(global_qtest);
    writel(RASPI4_PCIE_EXT_CFG_INDEX, index);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_INDEX), ==, index);

    g_assert_cmphex(readb(RASPI4_PCIE_EXT_CFG_DATA), ==, 0xff);
    g_assert_cmphex(readw(RASPI4_PCIE_EXT_CFG_DATA + PCI_DEVICE_ID), ==,
                    0xffff);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA + PCI_CLASS_REVISION), ==,
                    0xffffffff);

    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);
    g_assert_cmphex(readb(RASPI4_PCIE_EXT_CFG_DATA), ==, 0xff);
    g_assert_cmphex(readw(RASPI4_PCIE_EXT_CFG_DATA + PCI_DEVICE_ID), ==,
                    0xffff);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA + PCI_CLASS_REVISION), ==,
                    0xffffffff);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA + 0xffc), ==,
                    0xffffffff);

    writel(RASPI4_PCIE_EXT_CFG_DATA + PCI_VENDOR_ID, 0);
    writew(RASPI4_PCIE_EXT_CFG_DATA + PCI_COMMAND, 0);
    writeb(RASPI4_PCIE_EXT_CFG_DATA + 0xfff, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA), ==, 0xffffffff);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA + 0xffc), ==,
                    0xffffffff);

    qtest_system_reset(global_qtest);
}

static void test_pcie_outbound_windows(void)
{
    qtest_system_reset(global_qtest);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0);

    /* Linux 7.2 DT: CPU 0x600000000 -> PCI 0xf8000000, 64 MiB. */
    writel(RASPI4_PCIE_OUT_LO, 0xf8000000);
    writel(RASPI4_PCIE_OUT_HI, 0);
    writel(RASPI4_PCIE_BASE_LIMIT, 0x03f00000);
    writel(RASPI4_PCIE_BASE_HI, 6);
    writel(RASPI4_PCIE_LIMIT_HI, 6);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0xffffffff);

    /* Deployed firmware DT: the same CPU base maps 1 GiB at PCI c0000000. */
    writel(RASPI4_PCIE_OUT_LO, 0xc0000000);
    writel(RASPI4_PCIE_BASE_LIMIT, 0x3ff00000);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0xffffffff);

    /* Making base greater than limit removes the old CPU mapping. */
    writel(RASPI4_PCIE_BASE_HI, 0xff);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0);

    qtest_system_reset(global_qtest);
}

static void test_pcie_edu_config_and_mmio(void)
{
    const uint32_t value = 0x12345678;

    if (!pcie_edu_test_start()) {
        return;
    }

    g_assert_cmphex(pcie_edu_cfg_readl(PCI_VENDOR_ID), ==,
                    RASPI4_PCIE_EDU_ID);
    g_assert_cmphex(pcie_edu_cfg_readl(PCI_BASE_ADDRESS_0), ==,
                    RASPI4_PCIE_EDU_PCI_BAR);
    writel(RASPI4_PCIE_EDU_CPU_BAR + 4, value);
    g_assert_cmphex(readl(RASPI4_PCIE_EDU_CPU_BAR + 4), ==, ~value);

    qtest_system_reset(global_qtest);
}

static void test_pcie_edu_intx(void)
{
    if (!pcie_edu_test_start()) {
        return;
    }

    writel(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_RAISE, 1);
    g_assert_cmphex(readl(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_STATUS), ==, 1);
    g_assert_true(get_irq(RASPI4_PCIE_EDU_GIC_INTX));
    for (unsigned int irq = RASPI4_PCIE_INTX_FIRST;
         irq <= RASPI4_PCIE_INTX_LAST; irq++) {
        if (irq != RASPI4_PCIE_EDU_GIC_INTX) {
            g_assert_false(get_irq(irq));
        }
    }
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_MSI));

    writel(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_ACK, 1);
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_INTX));
    qtest_system_reset(global_qtest);
}

static void test_pcie_edu_inbound_dma(void)
{
    static const uint8_t payload[] = {
        0x51, 0x45, 0x4d, 0x55, 0x2d, 0x50, 0x69, 0x34,
        0x2d, 0x50, 0x43, 0x49, 0x65, 0x2d, 0x44,
        0x4d, 0x41, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x21,
    };
    const uint64_t source = 0x10000;
    const uint64_t destination = 0x20000;
    const uint64_t protected = 0x30000;
    uint8_t actual[sizeof(payload)];
    uint8_t sentinel[sizeof(payload)];

    if (!pcie_edu_test_start()) {
        return;
    }

    /* PCI 0x400000000..0x40fffffff aliases the first 256 MiB of RAM. */
    writel(RASPI4_PCIE_MISC_CTRL,
           readl(RASPI4_PCIE_MISC_CTRL) |
           PCIE_MISC_CTRL_SCB_ACCESS_EN);
    writel(RASPI4_PCIE_RC_BAR2_HI, 4);
    writel(RASPI4_PCIE_RC_BAR2_LO, PCIE_DMA_SIZE_ENCODING_256M);

    memwrite(source, payload, sizeof(payload));
    qtest_memset(global_qtest, destination, 0, sizeof(payload));
    pcie_edu_dma(PCIE_DMA_BASE + source, EDU_DMA_BUFFER,
                 sizeof(payload), 0);
    pcie_edu_dma(EDU_DMA_BUFFER, PCIE_DMA_BASE + destination,
                 sizeof(payload), EDU_DMA_TO_PCI);
    memread(destination, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), payload, sizeof(payload));

    /* An unmapped PCI address must not fall through to CPU system memory. */
    memset(sentinel, 0xa5, sizeof(sentinel));
    memwrite(protected, sentinel, sizeof(sentinel));
    pcie_edu_dma(EDU_DMA_BUFFER, protected, sizeof(payload),
                 EDU_DMA_TO_PCI);
    memread(protected, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), sentinel, sizeof(sentinel));

    /* Asserting PERST removes the inbound window before a queued DMA runs. */
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_SRC, EDU_DMA_BUFFER);
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_DST,
           PCIE_DMA_BASE + protected);
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_COUNT, sizeof(payload));
    writeq(RASPI4_PCIE_EDU_CPU_BAR + EDU_DMA_CMD,
           EDU_DMA_TO_PCI | EDU_DMA_RUN);
    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_PERST);
    qtest_clock_step(global_qtest, 101 * 1000 * 1000);
    memread(protected, actual, sizeof(actual));
    g_assert_cmpmem(actual, sizeof(actual), sentinel, sizeof(sentinel));

    qtest_system_reset(global_qtest);
}

static void test_pcie_edu_msi(void)
{
    const uint32_t vector = 5;
    const uint32_t vector_bit = 1U << vector;
    uint8_t msi_cap;
    uint16_t flags;
    uint16_t command;

    if (!pcie_edu_test_start()) {
        return;
    }

    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_MASK_STATUS), ==, UINT32_MAX);
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_MSI));

    writel(RASPI4_PCIE_MSI_DATA, 0xffe06540);
    writel(RASPI4_PCIE_MSI_BAR_HI, 0);
    writel(RASPI4_PCIE_MSI_BAR_LO, 0xfffffffd);

    /* The doorbell is private to PCI DMA, not CPU physical memory. */
    writel(0xfffffffc, 0x6540 | vector);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, 0);

    msi_cap = pcie_endpoint_find_capability(1, PCI_CAP_ID_MSI);
    g_assert_cmphex(msi_cap, !=, 0);
    flags = pcie_edu_cfg_readw(msi_cap + PCI_MSI_FLAGS);
    g_assert_true(flags & PCI_MSI_FLAGS_64BIT);
    pcie_edu_cfg_writel(msi_cap + PCI_MSI_ADDRESS_LO, 0xfffffffc);
    pcie_edu_cfg_writel(msi_cap + PCI_MSI_ADDRESS_HI, 0);
    pcie_edu_cfg_writew(msi_cap + PCI_MSI_DATA_64, 0x6540 | vector);
    pcie_edu_cfg_writew(msi_cap + PCI_MSI_FLAGS,
                        flags | PCI_MSI_FLAGS_ENABLE);
    command = pcie_edu_cfg_readw(PCI_COMMAND);
    pcie_edu_cfg_writew(PCI_COMMAND, command | PCI_COMMAND_INTX_DISABLE);

    writel(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_RAISE, 1);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, vector_bit);
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_INTX));
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_MSI));

    writel(RASPI4_PCIE_MSI_MASK_CLEAR, vector_bit);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_MASK_STATUS), ==,
                    UINT32_MAX & ~vector_bit);
    g_assert_true(get_irq(RASPI4_PCIE_EDU_GIC_MSI));
    writel(RASPI4_PCIE_MSI_MASK_SET, vector_bit);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, vector_bit);
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_MSI));
    writel(RASPI4_PCIE_MSI_MASK_CLEAR, vector_bit);
    g_assert_true(get_irq(RASPI4_PCIE_EDU_GIC_MSI));
    writel(RASPI4_PCIE_MSI_CLEAR, vector_bit);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, 0);
    g_assert_false(get_irq(RASPI4_PCIE_EDU_GIC_MSI));

    writel(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_ACK, 1);
    qtest_system_reset(global_qtest);
}

static void test_pcie_vl805_config_and_mmio(void)
{
    uint16_t msi_flags;

    pcie_vl805_test_start();

    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_VENDOR_ID), ==,
                    RASPI4_PCIE_VL805_ID);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_CLASS_REVISION), ==,
                    RASPI4_PCIE_VL805_CLASS_REV);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_SUBSYSTEM_VENDOR_ID), ==,
                    RASPI4_PCIE_VL805_ID);
    g_assert_cmphex(pcie_vl805_cfg_readb(PCI_HEADER_TYPE) &
                    PCI_HEADER_TYPE_MASK, ==, PCI_HEADER_TYPE_NORMAL);
    g_assert_cmphex(pcie_vl805_cfg_readb(PCI_INTERRUPT_PIN), ==, 1);
    g_assert_true(pcie_vl805_cfg_readw(PCI_STATUS) & PCI_STATUS_CAP_LIST);

    g_assert_cmphex(pcie_vl805_cfg_readb(PCI_CAPABILITY_LIST), ==,
                    RASPI4_PCIE_VL805_PM_CAP);
    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_PM_CAP +
                                        PCI_CAP_LIST_ID), ==, PCI_CAP_ID_PM);
    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_PM_CAP +
                                        PCI_CAP_LIST_NEXT), ==,
                    RASPI4_PCIE_VL805_MSI_CAP);
    g_assert_cmphex(pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_PM_CAP +
                                        PCI_PM_PMC), ==, 0x89c3);

    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_MSI_CAP +
                                        PCI_CAP_LIST_ID), ==,
                    PCI_CAP_ID_MSI);
    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_MSI_CAP +
                                        PCI_CAP_LIST_NEXT), ==,
                    RASPI4_PCIE_VL805_PCIE_CAP);
    msi_flags = pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_MSI_CAP +
                                     PCI_MSI_FLAGS);
    g_assert_cmphex(msi_flags & (PCI_MSI_FLAGS_ENABLE |
                                PCI_MSI_FLAGS_QMASK |
                                PCI_MSI_FLAGS_QSIZE |
                                PCI_MSI_FLAGS_64BIT |
                                PCI_MSI_FLAGS_MASKBIT), ==,
                    (2U << 1) | PCI_MSI_FLAGS_64BIT);
    g_assert_cmphex(pcie_endpoint_find_capability(0, PCI_CAP_ID_MSIX), ==, 0);

    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_CAP_LIST_ID), ==,
                    PCI_CAP_ID_EXP);
    g_assert_cmphex(pcie_vl805_cfg_readb(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_CAP_LIST_NEXT), ==, 0);
    g_assert_cmphex(pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_FLAGS) &
                    PCI_EXP_FLAGS_VERS, ==, 2);
    g_assert_cmphex(pcie_vl805_cfg_readl(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_DEVCAP), ==, 0x00008001);
    g_assert_cmphex(pcie_vl805_cfg_readl(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_LNKCAP), ==, 0x00065c12);
    g_assert_cmphex(pcie_vl805_cfg_readl(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_DEVCAP2), ==, 0x00000012);
    g_assert_cmphex(pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_LNKCTL2), ==, 0x0022);
    g_assert_cmphex(pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_PCIE_CAP +
                                        PCI_EXP_LNKSTA2), ==, 0x0001);

    g_assert_cmphex(PCI_EXT_CAP_ID(pcie_vl805_cfg_readl(
                        RASPI4_PCIE_VL805_AER_CAP)), ==, PCI_EXT_CAP_ID_ERR);
    g_assert_cmphex(PCI_EXT_CAP_VER(pcie_vl805_cfg_readl(
                        RASPI4_PCIE_VL805_AER_CAP)), ==, 2);
    g_assert_cmphex(pcie_vl805_cfg_readl(RASPI4_PCIE_VL805_AER_CAP +
                                        PCI_ERR_UNCOR_SEVER), ==, 0x00062031);
    g_assert_cmphex(pcie_vl805_cfg_readl(RASPI4_PCIE_VL805_AER_CAP +
                                        PCI_ERR_COR_MASK), ==, 0x00002000);

    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_0), ==,
                    RASPI4_PCIE_VL805_PCI_BAR |
                    PCI_BASE_ADDRESS_MEM_TYPE_64);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_1), ==, 0);

    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x00), ==,
                    0x01000020);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x04), ==,
                    0x05000420);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x08), ==,
                    0xfc000031);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x0c), ==,
                    0x00e70004);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x10), ==,
                    0x002841eb);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x14), ==,
                    VL805_XHCI_DOORBELL);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x18), ==,
                    VL805_XHCI_RUNTIME);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x1c), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xa0), ==,
                    0x00000401);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xb0), ==,
                    0x02000802);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xb8), ==,
                    0x10060101);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xc0), ==,
                    0x01e00023);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xd0), ==,
                    0x03008c02);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xd8), ==,
                    0x10000402);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0xe0), ==,
                    0x00050134);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x300), ==,
                    0x0001000a);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + 0x328), ==,
                    0x000000a0);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD),
                    ==, 0);
    g_assert_true(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                  VL805_XHCI_USBSTS_HCH);

    qtest_system_reset(global_qtest);
}

static void test_pcie_vl805_event_dma_msi(void)
{
    const uint64_t erst_pci = PCIE_DMA_BASE + VL805_XHCI_ERST_CPU;
    const uint64_t event_pci = PCIE_DMA_BASE + VL805_XHCI_EVENT_RING_CPU;
    const uint32_t vector_bit = 1U;
    uint16_t flags;
    uint16_t command;
    uint32_t portsc;

    pcie_vl805_test_start();

    /* PCI 0x400000000..0x40fffffff aliases the first 256 MiB of RAM. */
    writel(RASPI4_PCIE_MISC_CTRL,
           readl(RASPI4_PCIE_MISC_CTRL) |
           PCIE_MISC_CTRL_SCB_ACCESS_EN);
    writel(RASPI4_PCIE_RC_BAR2_HI, 4);
    writel(RASPI4_PCIE_RC_BAR2_LO, PCIE_DMA_SIZE_ENCODING_256M);

    writeq(VL805_XHCI_ERST_CPU, event_pci);
    writel(VL805_XHCI_ERST_CPU + 8, VL805_XHCI_EVENT_RING_TRBS);
    writel(VL805_XHCI_ERST_CPU + 12, 0);
    qtest_memset(global_qtest, VL805_XHCI_EVENT_RING_CPU, 0,
                 VL805_XHCI_EVENT_RING_TRBS * 16);

    writel(RASPI4_PCIE_MSI_DATA, 0xffe06540);
    writel(RASPI4_PCIE_MSI_BAR_HI, 0);
    writel(RASPI4_PCIE_MSI_BAR_LO, 0xfffffffd);
    writel(RASPI4_PCIE_MSI_MASK_CLEAR, vector_bit);

    flags = pcie_vl805_cfg_readw(RASPI4_PCIE_VL805_MSI_CAP +
                                 PCI_MSI_FLAGS);
    pcie_vl805_cfg_writel(RASPI4_PCIE_VL805_MSI_CAP + PCI_MSI_ADDRESS_LO,
                          0xfffffffc);
    pcie_vl805_cfg_writel(RASPI4_PCIE_VL805_MSI_CAP + PCI_MSI_ADDRESS_HI, 0);
    pcie_vl805_cfg_writew(RASPI4_PCIE_VL805_MSI_CAP + PCI_MSI_DATA_64,
                          0x6540);
    pcie_vl805_cfg_writew(RASPI4_PCIE_VL805_MSI_CAP + PCI_MSI_FLAGS,
                          flags | PCI_MSI_FLAGS_ENABLE);
    command = pcie_vl805_cfg_readw(PCI_COMMAND);
    pcie_vl805_cfg_writew(PCI_COMMAND,
                          command | PCI_COMMAND_INTX_DISABLE);

    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTSZ0, 1);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTBA0,
           (uint32_t)erst_pci);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTBA0 + 4,
           erst_pci >> 32);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERDP0,
           (uint32_t)event_pci);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERDP0 + 4,
           event_pci >> 32);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_IMAN0,
           VL805_XHCI_IMAN_IE);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD,
           VL805_XHCI_USBCMD_RUN | VL805_XHCI_USBCMD_INTE);
    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                   VL805_XHCI_USBSTS_HCH);

    qtest_qmp_device_add(global_qtest, "usb-kbd", "vl805-test-kbd",
                         "{'bus': 'vl805.0'}");

    g_assert_cmphex(readq(VL805_XHCI_EVENT_RING_CPU), ==, 1U << 24);
    g_assert_cmphex(readl(VL805_XHCI_EVENT_RING_CPU + 8), ==,
                    VL805_XHCI_CC_SUCCESS << 24);
    g_assert_cmphex(readl(VL805_XHCI_EVENT_RING_CPU + 12), ==,
                    (VL805_XHCI_TRB_PORT_STATUS <<
                     VL805_XHCI_TRB_TYPE_SHIFT) |
                    VL805_XHCI_TRB_CYCLE);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, vector_bit);
    g_assert_true(get_irq(RASPI4_PCIE_VL805_GIC_MSI));

    portsc = readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_PORT1);
    g_assert_true(portsc & VL805_XHCI_PORTSC_CCS);
    g_assert_true(portsc & VL805_XHCI_PORTSC_CSC);
    g_assert_cmphex(portsc & VL805_XHCI_PORTSC_SPEED_MASK, ==,
                    VL805_XHCI_PORTSC_SPEED_HIGH);

    writel(RASPI4_PCIE_MSI_CLEAR, vector_bit);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, 0);
    g_assert_false(get_irq(RASPI4_PCIE_VL805_GIC_MSI));

    qtest_system_reset(global_qtest);
    qtest_qmp_device_del(global_qtest, "vl805-test-kbd");
}

static void test_pcie_vl805_perst(void)
{
    const uint16_t command = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

    pcie_vl805_test_start();
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD,
           VL805_XHCI_USBCMD_RUN);
    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                   VL805_XHCI_USBSTS_HCH);

    writel(RASPI4_PCIE_SW_INIT, PCIE_SW_INIT_PERST);
    pcie_assert_link(false);
    pcie_select_endpoint(0);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_DATA), ==, UINT32_MAX);

    writel(RASPI4_PCIE_SW_INIT, 0);
    pcie_assert_link(true);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_VENDOR_ID), ==,
                    RASPI4_PCIE_VL805_ID);
    g_assert_cmphex(pcie_vl805_cfg_readw(PCI_COMMAND), ==, 0);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_0), ==,
                    PCI_BASE_ADDRESS_MEM_TYPE_64);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_1), ==, 0);

    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_0,
                          RASPI4_PCIE_VL805_PCI_BAR);
    pcie_vl805_cfg_writel(PCI_BASE_ADDRESS_1, 0);
    pcie_vl805_cfg_writew(PCI_COMMAND, command);
    g_assert_true(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                  VL805_XHCI_USBSTS_HCH);

    qtest_system_reset(global_qtest);
}

static void test_pcie_system_reset(void)
{
    const uint32_t index = pcie_cfg_index(1, 31, 7);

    qtest_system_reset(global_qtest);
    writel(RASPI4_PCIE_SW_INIT, 0);
    writel(RASPI4_PCIE_EXT_CFG_INDEX, index);
    writel(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS, 0x5a080700);
    writew(RASPI4_PCIE_BASE + PCI_COMMAND,
           PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
    writel(RASPI4_PCIE_VENDOR_REG1, 0xffffffff);
    writel(RASPI4_PCIE_ID_VAL3, 0xffffffff);
    writel(RASPI4_PCIE_PRIV_LINK_CAP, 0xffffffff);
    writel(RASPI4_PCIE_PRIV_ROOT_CAP, 0xffffffff);
    g_assert_cmphex(readl(RASPI4_PCIE_VENDOR_REG1), ==, 0x0c);
    g_assert_cmphex(readl(RASPI4_PCIE_ID_VAL3), ==, 0x00ffffff);
    g_assert_cmphex(readl(RASPI4_PCIE_PRIV_LINK_CAP), ==, 0x1f0);
    g_assert_cmphex(readl(RASPI4_PCIE_PRIV_ROOT_CAP), ==, 0xf8);

    writel(RASPI4_PCIE_OUT_LO, 0xf8000000);
    writel(RASPI4_PCIE_OUT_HI, 0);
    writel(RASPI4_PCIE_BASE_LIMIT, 0x03f00000);
    writel(RASPI4_PCIE_BASE_HI, 6);
    writel(RASPI4_PCIE_LIMIT_HI, 6);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0xffffffff);

    pcie_assert_link(true);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_INDEX), ==, index);

    qtest_system_reset(global_qtest);
    pcie_assert_root_identity();
    g_assert_cmphex(readl(RASPI4_PCIE_SW_INIT), ==, PCIE_SW_INIT_MASK);
    pcie_assert_link(false);
    g_assert_cmphex(readl(RASPI4_PCIE_EXT_CFG_INDEX), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_BASE + PCI_PRIMARY_BUS), ==, 0);
    g_assert_cmphex(readw(RASPI4_PCIE_BASE + PCI_COMMAND), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_CPU_WINDOW), ==, 0);
    pcie_assert_qmp_root(0, 0, 0);
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

static void assert_no_reset_event(void)
{
    QDict *response;

    response = qtest_qmp(global_qtest, "{ 'execute': 'query-status' }");
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);
    g_assert_null(qtest_qmp_event_ref(global_qtest, "RESET"));
}

static void test_powermgt_watchdog(void)
{
    const uint32_t timeout = 3 * RASPI4_PM_WDOG_HZ;
    const uint32_t marker = 0x4321;

    qtest_system_reset(global_qtest);
    g_assert_cmphex(readl(RASPI4_PM_RSTC), ==, RASPI4_PM_RSTC_STOP);
    g_assert_cmphex(readl(RASPI4_PM_RSTS), ==, RASPI4_PM_RSTS_DEFAULT);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, 0);

    /* PM_WDOG only takes effect once reset mode has armed the watchdog. */
    writel(RASPI4_PM_RSTS, RASPI4_PM_PASSWORD | marker);
    writel(RASPI4_PM_WDOG, RASPI4_PM_PASSWORD | timeout);
    qtest_clock_step(global_qtest, 4 * NANOSECONDS_PER_SECOND);
    assert_no_reset_event();
    g_assert_cmphex(readl(RASPI4_PM_RSTS), ==, marker);

    writel(RASPI4_PM_RSTS,
           RASPI4_PM_PASSWORD | RASPI4_PM_RSTS_DEFAULT);
    writel(RASPI4_PM_RSTC, RASPI4_PM_PASSWORD | RASPI4_PM_RSTC_FULL);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, timeout);
    qtest_clock_step(global_qtest, NANOSECONDS_PER_SECOND);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==,
                    timeout - RASPI4_PM_WDOG_HZ);

    /* A PM_WDOG write while armed is the driver's ping operation. */
    writel(RASPI4_PM_WDOG, RASPI4_PM_PASSWORD | timeout);
    qtest_clock_step(global_qtest, 2 * NANOSECONDS_PER_SECOND);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, RASPI4_PM_WDOG_HZ);

    /* Clearing the reset mode stops the watchdog. */
    writel(RASPI4_PM_RSTC, RASPI4_PM_PASSWORD | RASPI4_PM_RSTC_STOP);
    writel(RASPI4_PM_RSTS, RASPI4_PM_PASSWORD | marker);
    qtest_clock_step(global_qtest, 2 * NANOSECONDS_PER_SECOND);
    assert_no_reset_event();
    g_assert_cmphex(readl(RASPI4_PM_RSTS), ==, marker);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, 0);

    /* Expiry requests a normal guest reset, rather than resetting on arm. */
    writel(RASPI4_PM_RSTS,
           RASPI4_PM_PASSWORD | RASPI4_PM_RSTS_DEFAULT);
    writel(RASPI4_PM_WDOG,
           RASPI4_PM_PASSWORD | RASPI4_PM_WDOG_HZ);
    writel(RASPI4_PM_RSTC, RASPI4_PM_PASSWORD | RASPI4_PM_RSTC_FULL);
    qtest_clock_step(global_qtest, NANOSECONDS_PER_SECOND - 1);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, 1);
    assert_no_reset_event();
    qtest_clock_step(global_qtest, 1);
    qtest_qmp_eventwait(global_qtest, "RESET");
    g_assert_cmphex(readl(RASPI4_PM_RSTC), ==, RASPI4_PM_RSTC_STOP);
    g_assert_cmphex(readl(RASPI4_PM_RSTS), ==, RASPI4_PM_RSTS_DEFAULT);
    g_assert_cmphex(readl(RASPI4_PM_WDOG), ==, 0);
}

static void test_asb_bridge_ids(void)
{
    g_assert_cmphex(readl(RASPI4_ASB_BASE), ==, 0);
    g_assert_cmphex(readl(RASPI4_ASB_BASE + ASB_AXI_BRDG_ID), ==,
                    BCM2835_BRDG_ID);
    g_assert_cmphex(readl(RASPI4_RPIVID_ASB_BASE), ==, 0);
    g_assert_cmphex(readl(RASPI4_RPIVID_ASB_BASE + ASB_AXI_BRDG_ID), ==,
                    BCM2835_BRDG_ID);
}

static void test_system_timer_interrupts(void)
{
    for (unsigned int i = 0; i < 4; i++) {
        uint32_t now = readl(RASPI4_SYSTIMER_CLO);

        writel(RASPI4_SYSTIMER_COMPARE_0 + i * 4, now + 100);
        qtest_clock_step(global_qtest, 200 * 1000);

        g_assert_true(readl(RASPI4_SYSTIMER_CS) & (1U << i));
        g_assert_true(get_irq(RASPI4_SYSTIMER_GIC_IRQ_0 + i));

        writel(RASPI4_SYSTIMER_CS, 1U << i);
        g_assert_false(get_irq(RASPI4_SYSTIMER_GIC_IRQ_0 + i));
    }

    writel(RASPI4_SYSTIMER_COMPARE_0,
           readl(RASPI4_SYSTIMER_CLO) + 100);
    qtest_clock_step(global_qtest, 200 * 1000);
    g_assert_true(get_irq(RASPI4_SYSTIMER_GIC_IRQ_0));

    qtest_system_reset(global_qtest);
    g_assert_false(get_irq(RASPI4_SYSTIMER_GIC_IRQ_0));
    g_assert_cmphex(readl(RASPI4_SYSTIMER_CS), ==, 0);
    qtest_clock_step(global_qtest, 200 * 1000);
    g_assert_false(get_irq(RASPI4_SYSTIMER_GIC_IRQ_0));
}

static void test_spi0_interrupt(void)
{
    writel(RASPI4_SPI0_CS, SPI0_CS_INTD | SPI0_CS_TA);
    g_assert_true(readl(RASPI4_SPI0_CS) & SPI0_CS_DONE);
    g_assert_true(get_irq(RASPI4_SPI0_GIC_IRQ));

    qtest_system_reset(global_qtest);
    g_assert_false(readl(RASPI4_SPI0_CS) & SPI0_CS_DONE);
    g_assert_false(get_irq(RASPI4_SPI0_GIC_IRQ));

    writel(RASPI4_SPI0_CS, SPI0_CS_INTD | SPI0_CS_TA);
    g_assert_true(get_irq(RASPI4_SPI0_GIC_IRQ));
    writel(RASPI4_SPI0_CS, 0);
    g_assert_false(get_irq(RASPI4_SPI0_GIC_IRQ));
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
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
#ifndef _WIN32
    int test_sockets[2];
#endif

    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/raspi4b/asb/bridge_ids", test_asb_bridge_ids);
    qtest_add_func("/raspi4b/cpu/configuration", test_cpu_configuration);
    qtest_add_func("/raspi4b/powermgt/watchdog", test_powermgt_watchdog);
    qtest_add_func("/raspi4b/interrupts/system_timer",
                   test_system_timer_interrupts);
    qtest_add_func("/raspi4b/interrupts/spi0", test_spi0_interrupt);
    qtest_add_func("/raspi4b/sd/card_on_emmc2", test_sd_card_on_emmc2);
    qtest_add_func("/raspi4b/firmware_gpio", test_firmware_gpio);
    qtest_add_func("/raspi4b/firmware_dma_channels",
                   test_firmware_dma_channels);
    qtest_add_func("/raspi4b/pcie/root_config", test_pcie_root_config);
    qtest_add_func("/raspi4b/pcie/reset_link_and_mdio",
                   test_pcie_reset_link_and_mdio);
    qtest_add_func("/raspi4b/pcie/indirect_absent",
                   test_pcie_indirect_absent);
    qtest_add_func("/raspi4b/pcie/outbound_windows",
                   test_pcie_outbound_windows);
    qtest_add_func("/raspi4b/pcie/edu/config_and_mmio",
                   test_pcie_edu_config_and_mmio);
    qtest_add_func("/raspi4b/pcie/edu/intx", test_pcie_edu_intx);
    qtest_add_func("/raspi4b/pcie/edu/inbound_dma",
                   test_pcie_edu_inbound_dma);
    qtest_add_func("/raspi4b/pcie/edu/msi", test_pcie_edu_msi);
    qtest_add_func("/raspi4b/pcie/vl805/config_and_mmio",
                   test_pcie_vl805_config_and_mmio);
    qtest_add_func("/raspi4b/pcie/vl805/event_dma_msi",
                   test_pcie_vl805_event_dma_msi);
    qtest_add_func("/raspi4b/pcie/vl805/perst",
                   test_pcie_vl805_perst);
    qtest_add_func("/raspi4b/pcie/system_reset", test_pcie_system_reset);
    qtest_add_func("/raspi4b/genet/registers_and_mdio",
                   test_genet_registers_and_mdio);
#ifndef _WIN32
    qtest_add_func("/raspi4b/genet/packet_dma", test_genet_packet_dma);
#endif

    pcie_has_edu = qtest_has_device("edu");
    if (pcie_has_edu) {
        g_string_append(cmd_line,
                        " -device edu,id=edu0,bus=pcie.1,addr=1,"
                        "dma_mask=0xffffffffffffffff");
    }

#ifndef _WIN32
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, test_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(cmd_line, " -nic socket,fd=%d,model=genet",
                           test_sockets[1]);
    genet_test_socket = test_sockets[0];
    qtest_start(cmd_line->str);
    close(test_sockets[1]);
#else
    qtest_start(cmd_line->str);
#endif

    qtest_irq_intercept_in(global_qtest, "/machine/soc/peripherals");
    ret = g_test_run();
    qtest_end();
#ifndef _WIN32
    close(genet_test_socket);
#endif

    return ret;
}
