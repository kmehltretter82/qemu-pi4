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
#include "hw/usb/dwc2-regs.h"
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

#define RASPI4_DMA5_BASE           0xfe007500
#define RASPI4_DMA_CS              (RASPI4_DMA5_BASE + 0x00)
#define RASPI4_DMA_ADDR            (RASPI4_DMA5_BASE + 0x04)
#define RASPI4_DMA_TXFR_LEN        (RASPI4_DMA5_BASE + 0x14)
#define DMA_ACTIVE                 (1U << 0)
#define DMA_END                    (1U << 1)
#define DMA_ISPAUSED               (1U << 4)
#define DMA_S_INC                  (1U << 8)
#define DMA_D_INC                  (1U << 4)

#define RASPI4_DWC2_BASE          0xfe980000
#define RASPI4_DWC2_REG(_reg)     (RASPI4_DWC2_BASE + (_reg))
#define RASPI4_DWC2_GIC_IRQ       73

#define RASPI4_AON_BASE            0xfef00100
#define RASPI4_AON_CPU_STATUS      (RASPI4_AON_BASE + 0x00)
#define RASPI4_AON_CPU_SET         (RASPI4_AON_BASE + 0x04)
#define RASPI4_AON_CPU_CLEAR       (RASPI4_AON_BASE + 0x08)
#define RASPI4_AON_CPU_MASK_STATUS (RASPI4_AON_BASE + 0x0c)
#define RASPI4_AON_CPU_MASK_SET    (RASPI4_AON_BASE + 0x10)
#define RASPI4_AON_CPU_MASK_CLEAR  (RASPI4_AON_BASE + 0x14)
#define RASPI4_AON_PCI_STATUS      (RASPI4_AON_BASE + 0x18)
#define RASPI4_AON_PCI_SET         (RASPI4_AON_BASE + 0x1c)
#define RASPI4_AON_PCI_CLEAR       (RASPI4_AON_BASE + 0x20)
#define RASPI4_AON_PCI_MASK_STATUS (RASPI4_AON_BASE + 0x24)
#define RASPI4_AON_PCI_MASK_SET    (RASPI4_AON_BASE + 0x28)
#define RASPI4_AON_PCI_MASK_CLEAR  (RASPI4_AON_BASE + 0x2c)
#define RASPI4_AON_QOM_PATH        "/machine/soc/peripherals/aon-intr"
#define RASPI4_AON_GIC_IRQ         96
#define AON_VALID_MASK             0x00000fffU
#define RASPI4_GIC_ISPENDR4        0xff841210
#define GIC_PENDING_GPIO_IRQ1      (1U << 18)
#define GIC_PENDING_GPIO_ALL       (1U << 20)

#define RASPI4_SPI0_BASE          0xfe204000
#define RASPI4_SPI0_CS            RASPI4_SPI0_BASE
#define RASPI4_SPI0_GIC_IRQ       118
#define SPI0_CS_DONE              (1U << 16)
#define SPI0_CS_INTD              (1U << 9)
#define SPI0_CS_TA                (1U << 7)

#define RASPI4_AUX_BASE           0xfe215000
#define RASPI4_AUX_IRQ            (RASPI4_AUX_BASE + 0x00)
#define RASPI4_AUX_ENABLES        (RASPI4_AUX_BASE + 0x04)
#define RASPI4_AUX_MU_IER         (RASPI4_AUX_BASE + 0x44)
#define RASPI4_AUX_MU_MCR         (RASPI4_AUX_BASE + 0x50)
#define RASPI4_AUX_MU_MSR         (RASPI4_AUX_BASE + 0x58)
#define RASPI4_AUX_MU_SCRATCH     (RASPI4_AUX_BASE + 0x5c)
#define RASPI4_AUX_GIC_IRQ        93
#define AUX_ENABLE_UART           (1U << 0)
#define AUX_IER_TX_INT            (1U << 1)
#define AUX_MCR_RTS               (1U << 1)
#define AUX_MSR_CTS               (1U << 4)

#define RASPI4_GPIO_BASE          0xfe200000
#define RASPI4_GPIO_GPFSEL5       (RASPI4_GPIO_BASE + 0x14)
#define RASPI4_GPIO_GPSET1        (RASPI4_GPIO_BASE + 0x20)
#define RASPI4_GPIO_GPCLR1        (RASPI4_GPIO_BASE + 0x2c)
#define RASPI4_GPIO_GPLEV0        (RASPI4_GPIO_BASE + 0x34)
#define RASPI4_GPIO_GPLEV1        (RASPI4_GPIO_BASE + 0x38)
#define RASPI4_GPIO_GPEDS0        (RASPI4_GPIO_BASE + 0x40)
#define RASPI4_GPIO_GPEDS1        (RASPI4_GPIO_BASE + 0x44)
#define RASPI4_GPIO_GPREN0        (RASPI4_GPIO_BASE + 0x4c)
#define RASPI4_GPIO_GPREN1        (RASPI4_GPIO_BASE + 0x50)
#define RASPI4_GPIO_GPFEN0        (RASPI4_GPIO_BASE + 0x58)
#define RASPI4_GPIO_GPHEN0        (RASPI4_GPIO_BASE + 0x64)
#define RASPI4_GPIO_GPLEN0        (RASPI4_GPIO_BASE + 0x70)
#define RASPI4_GPIO_GPAREN0       (RASPI4_GPIO_BASE + 0x7c)
#define RASPI4_GPIO_GPAFEN0       (RASPI4_GPIO_BASE + 0x88)
#define RASPI4_GPIO_GPAFEN1       (RASPI4_GPIO_BASE + 0x8c)
#define RASPI4_GPIO_QOM_PATH      "/machine/soc/peripherals/gpio"
#define RASPI4_GPIO_GIC_IRQ0      113
#define RASPI4_GPIO_GIC_IRQ1      114
#define RASPI4_GPIO_GIC_IRQ2      115
#define RASPI4_GPIO_GIC_IRQ_ALL   116
#define GPIO_PIN5                 (1U << 5)
#define GPIO_PIN30                (1U << 30)
#define GPIO_PIN50_BANK1          (1U << (50 - 32))
#define GPIO_PIN57_BANK1          (1U << (57 - 32))
#define GPIO_FSEL5_PIN57_OUTPUT   (1U << ((57 - 50) * 3))
#define GPIO_BANK1_VALID_MASK     0x03ffffffU

#define RASPI4_RNG200_BASE        0xfe104000
#define RASPI4_RNG200_CTRL        (RASPI4_RNG200_BASE + 0x00)
#define RASPI4_RNG200_SOFT_RESET  (RASPI4_RNG200_BASE + 0x04)
#define RASPI4_RNG200_TOTAL_COUNT (RASPI4_RNG200_BASE + 0x0c)
#define RASPI4_RNG200_TOTAL_LIMIT (RASPI4_RNG200_BASE + 0x10)
#define RASPI4_RNG200_REVISION    (RASPI4_RNG200_BASE + 0x14)
#define RASPI4_RNG200_INT_STATUS  (RASPI4_RNG200_BASE + 0x18)
#define RASPI4_RNG200_INT_ENABLE  (RASPI4_RNG200_BASE + 0x1c)
#define RASPI4_RNG200_FIFO_DATA   (RASPI4_RNG200_BASE + 0x20)
#define RASPI4_RNG200_FIFO_COUNT  (RASPI4_RNG200_BASE + 0x24)
#define RASPI4_RNG200_GIC_IRQ     125
#define RNG200_CTRL_ENABLE        (1U << 0)
#define RNG200_CTRL_RING12        (1U << 12)
#define RNG200_CTRL_RATE_1MHZ     (3U << 13)
#define RNG200_INT_TOTAL_LIMIT    (1U << 0)
#define RNG200_INT_FIFO_FULL      (1U << 2)
#define RNG200_INT_FIFO_UNDERRUN  (1U << 4)
#define RNG200_INT_STARTUP        (1U << 17)
#define RNG200_FIFO_THRESHOLD     (16U << 8)
#define RNG200_FIFO_FULL          (1U << 30)
#define RNG200_FIFO_EMPTY         (1U << 31)
#define RNG200_REFILL_8MHZ_NS     4000
#define RNG200_REFILL_1MHZ_NS     32000

#define RASPI4_THERMAL_STATUS     0xfd5d2200
#define RASPI4_THERMAL_QOM_PATH   "/machine/soc/peripherals/thermal"
#define THERMAL_STATUS_VALID      ((1U << 16) | (1U << 10))
#define THERMAL_DEFAULT_RAW       770
#define THERMAL_DEFAULT_MC        35050
#define THERMAL_TEST_RAW          760
#define THERMAL_TEST_MC           39920

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
#define VL805_XHCI_CONFIG             (VL805_XHCI_CAPLENGTH + 0x38)
#define VL805_XHCI_IMAN0              (VL805_XHCI_RUNTIME + 0x20)
#define VL805_XHCI_ERSTSZ0            (VL805_XHCI_RUNTIME + 0x28)
#define VL805_XHCI_ERSTBA0            (VL805_XHCI_RUNTIME + 0x30)
#define VL805_XHCI_ERDP0              (VL805_XHCI_RUNTIME + 0x38)
#define VL805_XHCI_USBCMD_RUN         (1U << 0)
#define VL805_XHCI_USBCMD_INTE        (1U << 2)
#define VL805_XHCI_USBSTS_HCH         (1U << 0)
#define VL805_XHCI_USBSTS_HCE         (1U << 12)
#define VL805_XHCI_IMAN_IE            (1U << 1)
#define VL805_XHCI_ERDP_EHB           (1U << 3)
#define VL805_XHCI_PORTSC_CCS         (1U << 0)
#define VL805_XHCI_PORTSC_PR          (1U << 4)
#define VL805_XHCI_PORTSC_SPEED_MASK  (0xfU << 10)
#define VL805_XHCI_PORTSC_SPEED_HIGH  (3U << 10)
#define VL805_XHCI_PORTSC_CSC         (1U << 17)
#define VL805_XHCI_PORTSC_PRC         (1U << 21)
#define VL805_XHCI_TRB_CYCLE          (1U << 0)
#define VL805_XHCI_TRB_TYPE_SHIFT     10
#define VL805_XHCI_TRB_PORT_STATUS    34U
#define VL805_XHCI_CC_SUCCESS         1U
#define VL805_XHCI_ERST_CPU           0x20000ULL
#define VL805_XHCI_EVENT_RING_CPU     0x21000ULL
#define VL805_XHCI_EVENT_RING2_CPU    0x22000ULL
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

static void gpio_set_input(unsigned int pin, int level);
static void rng200_refill_words(unsigned int words);
static void property_request_qtest(QTestState *qts, uint32_t tag,
                                   const uint32_t *payload, size_t words,
                                   size_t response_bytes);
static uint32_t property_payload_qtest(QTestState *qts, size_t word);

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

static void pcie_vl805_enable_dma(void)
{
    /* PCI 0x400000000..0x40fffffff aliases the first 256 MiB of RAM. */
    writel(RASPI4_PCIE_MISC_CTRL,
           readl(RASPI4_PCIE_MISC_CTRL) |
           PCIE_MISC_CTRL_SCB_ACCESS_EN);
    writel(RASPI4_PCIE_RC_BAR2_HI, 4);
    writel(RASPI4_PCIE_RC_BAR2_LO, PCIE_DMA_SIZE_ENCODING_256M);
}

static void pcie_assert_vl805_port_event_qtest(QTestState *qts,
                                               uint64_t addr, bool cycle)
{
    uint32_t control = VL805_XHCI_TRB_PORT_STATUS <<
                       VL805_XHCI_TRB_TYPE_SHIFT;

    if (cycle) {
        control |= VL805_XHCI_TRB_CYCLE;
    }
    g_assert_cmphex(qtest_readq(qts, addr), ==, 1U << 24);
    g_assert_cmphex(qtest_readl(qts, addr + 8), ==,
                    VL805_XHCI_CC_SUCCESS << 24);
    g_assert_cmphex(qtest_readl(qts, addr + 12), ==, control);
}

static void pcie_assert_vl805_port_event(uint64_t addr, bool cycle)
{
    pcie_assert_vl805_port_event_qtest(global_qtest, addr, cycle);
}

static void pcie_vl805_reset_usb2_port_qtest(QTestState *qts)
{
    uint64_t port = RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_PORT1;
    uint32_t portsc = qtest_readl(qts, port);

    /* Clear the existing change flags before asking for another reset. */
    qtest_writel(qts, port, portsc);
    portsc = qtest_readl(qts, port);
    qtest_writel(qts, port, portsc | VL805_XHCI_PORTSC_PR);
}

static void pcie_vl805_reset_usb2_port(void)
{
    pcie_vl805_reset_usb2_port_qtest(global_qtest);
}

static void raspi4b_watchdog_reset(void)
{
    writel(RASPI4_PM_RSTS,
           RASPI4_PM_PASSWORD | RASPI4_PM_RSTS_DEFAULT);
    writel(RASPI4_PM_WDOG,
           RASPI4_PM_PASSWORD | RASPI4_PM_WDOG_HZ);
    writel(RASPI4_PM_RSTC,
           RASPI4_PM_PASSWORD | RASPI4_PM_RSTC_FULL);
    qtest_clock_step(global_qtest, NANOSECONDS_PER_SECOND);
    qtest_qmp_eventwait(global_qtest, "RESET");
}

static void aon_set_input_qtest(QTestState *qts, unsigned int line, int level)
{
    qtest_set_irq_in(qts, RASPI4_AON_QOM_PATH, NULL, line, level);
}

static void aon_set_input(unsigned int line, int level)
{
    aon_set_input_qtest(global_qtest, line, level);
}

static void test_aon_interrupts_and_reset(void)
{
    const uint32_t line1 = 1U << 1;
    const uint32_t line2 = 1U << 2;
    const uint32_t line4 = 1U << 4;

    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_CPU_MASK_STATUS), ==,
                    AON_VALID_MASK);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_MASK_STATUS), ==,
                    AON_VALID_MASK);
    g_assert_false(get_irq(RASPI4_AON_GIC_IRQ));

    /* Action registers read as zero on the Pi 400. */
    g_assert_cmphex(readl(RASPI4_AON_CPU_SET), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_CPU_CLEAR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_CPU_MASK_SET), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_CPU_MASK_CLEAR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_SET), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_CLEAR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_MASK_SET), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_MASK_CLEAR), ==, 0);

    /* A physical rising edge latches both independently masked banks. */
    aon_set_input(4, 1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, line4);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, line4);
    g_assert_false(get_irq(RASPI4_AON_GIC_IRQ));

    /* Pending state survives masking and asserts as soon as it is unmasked. */
    writel(RASPI4_AON_CPU_MASK_CLEAR, line4);
    g_assert_cmphex(readl(RASPI4_AON_CPU_MASK_STATUS), ==,
                    AON_VALID_MASK & ~line4);
    g_assert_true(get_irq(RASPI4_AON_GIC_IRQ));
    writel(RASPI4_AON_CPU_CLEAR, line4);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, line4);
    g_assert_false(get_irq(RASPI4_AON_GIC_IRQ));

    /* Clearing a high input does not relatch it without another edge. */
    aon_set_input(4, 1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 0);
    writel(RASPI4_AON_PCI_CLEAR, line4);
    aon_set_input(4, 0);
    aon_set_input(4, 1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, line4);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, line4);
    g_assert_true(get_irq(RASPI4_AON_GIC_IRQ));
    writel(RASPI4_AON_CPU_CLEAR, line4);
    writel(RASPI4_AON_PCI_CLEAR, line4);

    /* Software SET/CLEAR operations affect only their selected bank. */
    writel(RASPI4_AON_CPU_SET, line1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, line1);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, 0);
    writel(RASPI4_AON_PCI_SET, line2);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, line1);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, line2);
    writel(RASPI4_AON_CPU_CLEAR, line1);
    writel(RASPI4_AON_PCI_CLEAR, line2);

    /* Only the twelve HDMI interrupt lines are implemented. */
    writel(RASPI4_AON_CPU_SET, UINT32_MAX);
    writel(RASPI4_AON_PCI_SET, UINT32_MAX);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, AON_VALID_MASK);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, AON_VALID_MASK);
    writel(RASPI4_AON_CPU_CLEAR, UINT32_MAX);
    writel(RASPI4_AON_PCI_CLEAR, UINT32_MAX);
    writel(RASPI4_AON_CPU_MASK_SET, UINT32_MAX);

    /* Reset clears both latches and masks every valid line. */
    writel(RASPI4_AON_CPU_SET, line4);
    writel(RASPI4_AON_PCI_SET, line2);
    writel(RASPI4_AON_CPU_MASK_CLEAR, line4);
    g_assert_true(get_irq(RASPI4_AON_GIC_IRQ));
    raspi4b_watchdog_reset();
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_CPU_MASK_STATUS), ==,
                    AON_VALID_MASK);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_AON_PCI_MASK_STATUS), ==,
                    AON_VALID_MASK);
    g_assert_false(get_irq(RASPI4_AON_GIC_IRQ));

    /* The externally held-high level survives reset without a false edge. */
    aon_set_input(4, 1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 0);
    aon_set_input(4, 0);
    aon_set_input(4, 1);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, line4);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, line4);
    aon_set_input(4, 0);
}

static void test_aux_uart_modem_and_reset(void)
{
    g_assert_cmphex(readl(RASPI4_AUX_ENABLES), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_IER), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MSR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_SCRATCH), ==, 0);
    g_assert_false(get_irq(RASPI4_AUX_GIC_IRQ));

    /* Pi 400 silicon retains and reads back the whole low byte. */
    writel(RASPI4_AUX_ENABLES, UINT32_MAX);
    g_assert_cmphex(readl(RASPI4_AUX_ENABLES), ==, UINT8_MAX);
    writel(RASPI4_AUX_ENABLES, 0);

    /*
     * Pi 400 control writes are retained while disabled, but the UART bank
     * reads as zero.  Its interrupt status and GIC line are not gated.
     */
    writel(RASPI4_AUX_MU_MCR, UINT32_MAX);
    writel(RASPI4_AUX_MU_IER, AUX_IER_TX_INT);
    writel(RASPI4_AUX_MU_SCRATCH, 0x1a5);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_IER), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_SCRATCH), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 1);
    g_assert_true(get_irq(RASPI4_AUX_GIC_IRQ));

    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_UART);
    g_assert_cmphex(readl(RASPI4_AUX_ENABLES), ==, AUX_ENABLE_UART);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, AUX_MCR_RTS);
    g_assert_cmphex(readl(RASPI4_AUX_MU_IER), ==, AUX_IER_TX_INT);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MSR), ==, AUX_MSR_CTS);
    g_assert_cmphex(readl(RASPI4_AUX_MU_SCRATCH), ==, 0xa5);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 1);
    g_assert_true(get_irq(RASPI4_AUX_GIC_IRQ));

    writel(RASPI4_AUX_MU_MCR, 1);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, 0);
    writel(RASPI4_AUX_MU_MCR, AUX_MCR_RTS);

    raspi4b_watchdog_reset();
    g_assert_cmphex(readl(RASPI4_AUX_ENABLES), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_IER), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MSR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_SCRATCH), ==, 0);
    g_assert_false(get_irq(RASPI4_AUX_GIC_IRQ));

    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_UART);
    g_assert_cmphex(readl(RASPI4_AUX_MU_IER), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MCR), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_MU_MSR), ==, AUX_MSR_CTS);
    g_assert_cmphex(readl(RASPI4_AUX_MU_SCRATCH), ==, 0);
    writel(RASPI4_AUX_MU_IER, AUX_IER_TX_INT);
    g_assert_true(get_irq(RASPI4_AUX_GIC_IRQ));
    writel(RASPI4_AUX_MU_IER, 0);
    g_assert_false(get_irq(RASPI4_AUX_GIC_IRQ));
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
}

static void test_pcie_reset_link_and_mdio(void)
{
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
}

static void test_pcie_indirect_absent(void)
{
    const uint32_t index = pcie_cfg_index(1, 31, 7);

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
}

static void test_pcie_outbound_windows(void)
{
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
    pcie_vl805_enable_dma();

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

    portsc = readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_PORT1);
    g_assert_true(portsc & VL805_XHCI_PORTSC_CCS);
    g_assert_cmphex(portsc & VL805_XHCI_PORTSC_SPEED_MASK, ==,
                    VL805_XHCI_PORTSC_SPEED_HIGH);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_PORT1,
           portsc | VL805_XHCI_PORTSC_PR);

    pcie_assert_vl805_port_event(VL805_XHCI_EVENT_RING_CPU, true);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, vector_bit);
    g_assert_true(get_irq(RASPI4_PCIE_VL805_GIC_MSI));

    portsc = readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_PORT1);
    g_assert_true(portsc & VL805_XHCI_PORTSC_CCS);
    g_assert_true(portsc & VL805_XHCI_PORTSC_CSC);
    g_assert_true(portsc & VL805_XHCI_PORTSC_PRC);
    g_assert_cmphex(portsc & VL805_XHCI_PORTSC_SPEED_MASK, ==,
                    VL805_XHCI_PORTSC_SPEED_HIGH);

    writel(RASPI4_PCIE_MSI_CLEAR, vector_bit);
    g_assert_cmphex(readl(RASPI4_PCIE_MSI_STATUS), ==, 0);
    g_assert_false(get_irq(RASPI4_PCIE_VL805_GIC_MSI));
}

static void pcie_vl805_setup_multisegment_event_ring(void)
{
    const uint64_t erst_pci = PCIE_DMA_BASE + VL805_XHCI_ERST_CPU;
    const uint64_t event1_pci = PCIE_DMA_BASE +
                                VL805_XHCI_EVENT_RING_CPU;
    const uint64_t event2_pci = PCIE_DMA_BASE +
                                VL805_XHCI_EVENT_RING2_CPU;

    pcie_vl805_test_start();
    pcie_vl805_enable_dma();

    writeq(VL805_XHCI_ERST_CPU, event1_pci);
    writel(VL805_XHCI_ERST_CPU + 8, VL805_XHCI_EVENT_RING_TRBS);
    writel(VL805_XHCI_ERST_CPU + 12, 0);
    writeq(VL805_XHCI_ERST_CPU + 16, event2_pci);
    writel(VL805_XHCI_ERST_CPU + 24, VL805_XHCI_EVENT_RING_TRBS);
    writel(VL805_XHCI_ERST_CPU + 28, 0);
    qtest_memset(global_qtest, VL805_XHCI_EVENT_RING_CPU, 0,
                 VL805_XHCI_EVENT_RING_TRBS * 16);
    qtest_memset(global_qtest, VL805_XHCI_EVENT_RING2_CPU, 0,
                 VL805_XHCI_EVENT_RING_TRBS * 16);

    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTSZ0, 2);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTBA0,
           (uint32_t)erst_pci);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERSTBA0 + 4,
           erst_pci >> 32);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERDP0,
           (uint32_t)event1_pci);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERDP0 + 4,
           event1_pci >> 32);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD,
           VL805_XHCI_USBCMD_RUN);

    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR +
                         VL805_XHCI_USBSTS) & VL805_XHCI_USBSTS_HCE);
}

static void test_pcie_vl805_multisegment_event_ring(void)
{
    const uint64_t event2_pci = PCIE_DMA_BASE +
                                VL805_XHCI_EVENT_RING2_CPU;
    unsigned int i;

    pcie_vl805_setup_multisegment_event_ring();

    /* Fill segment zero and place one event in non-contiguous segment one. */
    for (i = 0; i < VL805_XHCI_EVENT_RING_TRBS + 1; i++) {
        pcie_vl805_reset_usb2_port();
    }
    pcie_assert_vl805_port_event(VL805_XHCI_EVENT_RING_CPU, true);
    pcie_assert_vl805_port_event(
        VL805_XHCI_EVENT_RING_CPU +
        (VL805_XHCI_EVENT_RING_TRBS - 1) * 16, true);
    pcie_assert_vl805_port_event(VL805_XHCI_EVENT_RING2_CPU, true);
    g_assert_cmphex(readq(VL805_XHCI_EVENT_RING2_CPU + 16), ==, 0);

    /* Consumer and producer now agree on segment one, entry one. */
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_ERDP0,
           (uint32_t)(event2_pci + 16) | VL805_XHCI_ERDP_EHB | 1U);

    /* Traverse the rest of segment one and wrap; PCS toggles only here. */
    for (i = 0; i < VL805_XHCI_EVENT_RING_TRBS; i++) {
        pcie_vl805_reset_usb2_port();
    }
    pcie_assert_vl805_port_event(
        VL805_XHCI_EVENT_RING2_CPU +
        (VL805_XHCI_EVENT_RING_TRBS - 1) * 16, true);
    pcie_assert_vl805_port_event(VL805_XHCI_EVENT_RING_CPU, false);
    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR +
                         VL805_XHCI_USBSTS) & VL805_XHCI_USBSTS_HCE);
}

#ifndef _WIN32
static void raspi4b_wait_for_migration(QTestState *qts)
{
    int64_t deadline = g_get_monotonic_time() + 30 * G_TIME_SPAN_SECOND;

    while (g_get_monotonic_time() < deadline) {
        g_autoptr(QDict) response = qtest_qmp(
            qts, "{ 'execute': 'query-migrate' }");
        QDict *result = qdict_get_qdict(response, "return");
        const char *status = qdict_get_str(result, "status");

        if (g_str_equal(status, "completed")) {
            return;
        }
        g_assert_false(g_str_equal(status, "failed"));
        g_assert_false(g_str_equal(status, "cancelled"));
        g_usleep(1000);
    }

    g_error("timed out waiting for Raspberry Pi 4 migration");
}

static void test_pcie_vl805_multisegment_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
    QTestState *source = global_qtest;
    QTestState *destination;
    QDict *result;
    QDict *status;
    bool source_was_running;
    int net_sockets[2];
    int ret;

    pcie_vl805_setup_multisegment_event_ring();

    /* Leave the producer at segment one, entry one. */
    for (unsigned int i = 0; i < VL805_XHCI_EVENT_RING_TRBS + 1; i++) {
        pcie_vl805_reset_usb2_port();
    }
    pcie_assert_vl805_port_event(VL805_XHCI_EVENT_RING2_CPU, true);
    g_assert_cmphex(readq(VL805_XHCI_EVENT_RING2_CPU + 16), ==, 0);

    result = qtest_qmp(source, "{ 'execute': 'query-status' }");
    g_assert_true(qdict_haskey(result, "return"));
    status = qdict_get_qdict(result, "return");
    source_was_running = qdict_get_bool(status, "running");
    qobject_unref(result);
    if (source_was_running) {
        qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    }

    tmpdir = g_dir_make_tmp("raspi4b-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);

    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    raspi4b_wait_for_migration(source);

    if (pcie_has_edu) {
        g_string_append(cmd_line,
                        " -device edu,id=edu0,bus=pcie.1,addr=1,"
                        "dma_mask=0xffffffffffffffff");
    }
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, net_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(cmd_line, " -nic socket,fd=%d,model=genet",
                           net_sockets[1]);
    g_string_append_printf(cmd_line, " -incoming %s", uri);
    destination = qtest_init(cmd_line->str);
    close(net_sockets[1]);
    raspi4b_wait_for_migration(destination);

    /* The next event must use the migrated segment and producer index. */
    g_assert_cmphex(qtest_readq(destination,
                               VL805_XHCI_EVENT_RING2_CPU + 16), ==, 0);
    pcie_vl805_reset_usb2_port_qtest(destination);
    pcie_assert_vl805_port_event_qtest(
        destination, VL805_XHCI_EVENT_RING2_CPU + 16, true);
    g_assert_false(qtest_readl(destination,
                              RASPI4_PCIE_VL805_CPU_BAR +
                              VL805_XHCI_USBSTS) & VL805_XHCI_USBSTS_HCE);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_soc_peripheral_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
    QTestState *source = global_qtest;
    QTestState *destination;
    uint32_t source_word;
    uint32_t destination_word;
    const uint32_t dma_cb = 0x8000;
    const uint32_t dma_source = 0x9000;
    const uint32_t dma_destination = 0xa000;
    const uint32_t dma_length = 2048;
    int64_t dma_clock;
    QDict *response;
    unsigned offset;
    int net_sockets[2];
    int ret;

    const uint32_t clock_off[] = {
        RPI_FIRMWARE_ARM_CLK_ID, 0,
    };
    const uint32_t clock_rate[] = {
        RPI_FIRMWARE_CORE_CLK_ID, 480000000, 0,
    };
    const uint32_t domain_on[] = {
        RPI_FIRMWARE_V3D_DOMAIN_ID, RPI_FIRMWARE_STATE_ENABLE,
    };
    const uint32_t get_clock[] = {
        RPI_FIRMWARE_ARM_CLK_ID, UINT32_MAX,
    };
    const uint32_t get_clock_rate[] = {
        RPI_FIRMWARE_CORE_CLK_ID, UINT32_MAX,
    };
    const uint32_t get_domain[] = {
        RPI_FIRMWARE_V3D_DOMAIN_ID, UINT32_MAX,
    };

    writel(RASPI4_GPIO_GPAREN0, GPIO_PIN30);
    writel(RASPI4_GPIO_GPAFEN0, GPIO_PIN30);
    gpio_set_input(30, 1);
    g_assert_cmphex(readl(RASPI4_GPIO_GPLEV0), ==, GPIO_PIN30);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN30);
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));

    writel(RASPI4_RNG200_TOTAL_LIMIT, 64);
    writel(RASPI4_RNG200_INT_ENABLE, RNG200_INT_STARTUP);
    writel(RASPI4_RNG200_CTRL,
           RNG200_CTRL_RATE_1MHZ | RNG200_CTRL_ENABLE);
    rng200_refill_words(4);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 4);
    g_assert_cmphex(readl(RASPI4_RNG200_TOTAL_COUNT), ==, 128);

    response = qtest_qmp(source,
        "{ 'execute': 'qom-set', 'arguments': { 'path': %s, "
        "'property': 'temperature', 'value': %d } }",
        RASPI4_THERMAL_QOM_PATH, THERMAL_TEST_MC);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);

    property_request_qtest(source, RPI_FWREQ_SET_CLOCK_STATE, clock_off,
                           G_N_ELEMENTS(clock_off), sizeof(clock_off));
    property_request_qtest(source, RPI_FWREQ_SET_CLOCK_RATE, clock_rate,
                           G_N_ELEMENTS(clock_rate),
                           2 * sizeof(uint32_t));
    property_request_qtest(source, RPI_FWREQ_SET_DOMAIN_STATE, domain_on,
                           G_N_ELEMENTS(domain_on), sizeof(domain_on));
    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_UART);
    writel(RASPI4_AUX_MU_MCR, AUX_MCR_RTS);
    writel(RASPI4_AUX_MU_SCRATCH, 0xa5);
    writel(RASPI4_AUX_ENABLES, 0);

    /* Migrate distinct AON bank state and a held-high physical source. */
    aon_set_input(8, 1);
    writel(RASPI4_AON_PCI_CLEAR, 1U << 8);
    writel(RASPI4_AON_PCI_SET, 1U << 6);
    writel(RASPI4_AON_CPU_MASK_CLEAR, 1U << 8);
    writel(RASPI4_AON_PCI_MASK_CLEAR, 1U << 6);
    g_assert_cmphex(readl(RASPI4_AON_CPU_STATUS), ==, 1U << 8);
    g_assert_cmphex(readl(RASPI4_AON_PCI_STATUS), ==, 1U << 6);
    g_assert_true(get_irq(RASPI4_AON_GIC_IRQ));

    /* Leave a bounded DMA transfer half complete at the migration boundary. */
    writel(dma_cb, DMA_S_INC | DMA_D_INC);
    writel(dma_cb + 4, dma_source);
    writel(dma_cb + 8, dma_destination);
    writel(dma_cb + 12, dma_length);
    writel(dma_cb + 16, 0);
    writel(dma_cb + 20, 0);
    for (offset = 0; offset < dma_length; offset += sizeof(uint32_t)) {
        writel(dma_source + offset, 0x12340000 + offset);
        writel(dma_destination + offset, 0);
    }
    writel(RASPI4_DMA_ADDR, dma_cb);
    writel(RASPI4_DMA_CS, DMA_ACTIVE);
    g_assert_cmphex(readl(dma_destination + 1020), ==, 0x123403fc);
    g_assert_cmphex(readl(dma_destination + 1024), ==, 0);
    g_assert_cmphex(readl(RASPI4_DMA_TXFR_LEN), ==, 1024);

    /* Consume 400 ns of the 1 us continuation delay before migration. */
    dma_clock = qtest_clock_step(source, 400);
    g_assert_cmphex(readl(dma_destination + 1024), ==, 0);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-soc-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);

    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    raspi4b_wait_for_migration(source);

    if (pcie_has_edu) {
        g_string_append(cmd_line,
                        " -device edu,id=edu0,bus=pcie.1,addr=1,"
                        "dma_mask=0xffffffffffffffff");
    }
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, net_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(cmd_line, " -nic socket,fd=%d,model=genet",
                           net_sockets[1]);
    g_string_append(cmd_line, " -incoming defer");
    destination = qtest_init(cmd_line->str);
    close(net_sockets[1]);
    qtest_irq_intercept_out_named(destination, RASPI4_AON_QOM_PATH,
                                  "sysbus-irq");
    qtest_writel(destination, RASPI4_AON_CPU_SET, 1U);
    qtest_writel(destination, RASPI4_AON_CPU_MASK_CLEAR, 1U);
    g_assert_true(qtest_get_irq(destination, 0));
    qtest_writel(destination, RASPI4_AON_CPU_CLEAR, 1U);
    qtest_writel(destination, RASPI4_AON_CPU_MASK_SET, 1U);
    g_assert_false(qtest_get_irq(destination, 0));
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    g_assert_cmphex(qtest_readl(destination, RASPI4_GPIO_GPAREN0), ==,
                    GPIO_PIN30);
    g_assert_cmphex(qtest_readl(destination, RASPI4_GPIO_GPAFEN0), ==,
                    GPIO_PIN30);
    g_assert_cmphex(qtest_readl(destination, RASPI4_GPIO_GPLEV0), ==,
                    GPIO_PIN30);
    g_assert_cmphex(qtest_readl(destination, RASPI4_GPIO_GPEDS0), ==,
                    GPIO_PIN30);

    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_CPU_STATUS), ==,
                    1U << 8);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_CPU_MASK_STATUS), ==,
                    AON_VALID_MASK & ~(1U << 8));
    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_PCI_STATUS), ==,
                    1U << 6);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_PCI_MASK_STATUS), ==,
                    AON_VALID_MASK & ~(1U << 6));
    g_assert_true(qtest_get_irq(destination, 0));
    g_assert_true(qtest_get_irq(destination, 1));
    qtest_writel(destination, RASPI4_AON_CPU_CLEAR, 1U << 8);
    g_assert_false(qtest_get_irq(destination, 0));
    g_assert_true(qtest_get_irq(destination, 1));

    /* Migrated input level prevents a duplicate high-to-high edge. */
    aon_set_input_qtest(destination, 8, 1);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_CPU_STATUS), ==, 0);
    aon_set_input_qtest(destination, 8, 0);
    aon_set_input_qtest(destination, 8, 1);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AON_CPU_STATUS), ==,
                    1U << 8);
    g_assert_true(qtest_get_irq(destination, 0));
    g_assert_true(qtest_get_irq(destination, 1));
    qtest_writel(destination, RASPI4_AON_PCI_CLEAR, 1U << 6);
    g_assert_false(qtest_get_irq(destination, 1));

    qtest_writel(destination, RASPI4_GPIO_GPEDS0, GPIO_PIN30);
    g_assert_false(qtest_readl(destination, RASPI4_GIC_ISPENDR4) &
                   GIC_PENDING_GPIO_IRQ1);
    g_assert_false(qtest_readl(destination, RASPI4_GIC_ISPENDR4) &
                   GIC_PENDING_GPIO_ALL);
    qtest_set_irq_in(destination, RASPI4_GPIO_QOM_PATH, NULL, 30, 0);
    g_assert_cmphex(qtest_readl(destination, RASPI4_GPIO_GPEDS0), ==,
                    GPIO_PIN30);
    g_assert_true(qtest_readl(destination, RASPI4_GIC_ISPENDR4) &
                  GIC_PENDING_GPIO_IRQ1);
    g_assert_true(qtest_readl(destination, RASPI4_GIC_ISPENDR4) &
                  GIC_PENDING_GPIO_ALL);

    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_CTRL), ==,
                    RNG200_CTRL_RATE_1MHZ | RNG200_CTRL_ENABLE);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_TOTAL_COUNT), ==,
                    128);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_TOTAL_LIMIT), ==,
                    64);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_INT_ENABLE), ==,
                    RNG200_INT_STARTUP);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 4);
    response = qtest_qmp(destination,
        "{ 'execute': 'qom-get', 'arguments': { 'path': %s, "
        "'property': 'temperature' } }", RASPI4_THERMAL_QOM_PATH);
    g_assert(qdict_haskey(response, "return"));
    g_assert_cmpint(qdict_get_int(response, "return"), ==, THERMAL_TEST_MC);
    qobject_unref(response);
    g_assert_cmphex(qtest_readl(destination, RASPI4_THERMAL_STATUS), ==,
                    THERMAL_STATUS_VALID | THERMAL_TEST_RAW);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_ENABLES), ==, 0);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_MCR), ==, 0);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_SCRATCH), ==, 0);
    qtest_writel(destination, RASPI4_AUX_ENABLES, AUX_ENABLE_UART);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_MCR), ==,
                    AUX_MCR_RTS);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_SCRATCH), ==,
                    0xa5);
    g_assert_cmphex(qtest_readl(destination, RASPI4_DMA_CS) & DMA_ACTIVE,
                    ==, DMA_ACTIVE);
    g_assert_cmphex(qtest_readl(destination, RASPI4_DMA_TXFR_LEN), ==, 1024);
    g_assert_cmphex(qtest_readl(destination, dma_destination + 1020), ==,
                    0x123403fc);
    g_assert_cmphex(qtest_readl(destination, dma_destination + 1024), ==, 0);

    property_request_qtest(destination, RPI_FWREQ_GET_CLOCK_STATE, get_clock,
                           G_N_ELEMENTS(get_clock), sizeof(get_clock));
    g_assert_cmphex(property_payload_qtest(destination, 1), ==, 0);
    property_request_qtest(destination, RPI_FWREQ_GET_CLOCK_RATE,
                           get_clock_rate, G_N_ELEMENTS(get_clock_rate),
                           sizeof(get_clock_rate));
    g_assert_cmpuint(property_payload_qtest(destination, 1), ==, 480000000);
    property_request_qtest(destination, RPI_FWREQ_GET_DOMAIN_STATE, get_domain,
                           G_N_ELEMENTS(get_domain), sizeof(get_domain));
    g_assert_cmphex(property_payload_qtest(destination, 1), ==,
                    RPI_FIRMWARE_STATE_ENABLE);

    /* The FIFO payload itself, not just its count, must migrate. */
    source_word = qtest_readl(source, RASPI4_RNG200_FIFO_DATA);
    destination_word = qtest_readl(destination, RASPI4_RNG200_FIFO_DATA);
    g_assert_cmphex(destination_word, ==, source_word);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 3);

    /* In-flight DMA progress and its continuation deadline must migrate. */
    qtest_qmp_assert_success(destination, "{ 'execute': 'cont' }");
    g_assert_cmpint(qtest_clock_step_next(destination), ==, dma_clock + 600);
    g_assert_cmphex(qtest_readl(destination,
                               dma_destination + dma_length - 4), ==,
                    0x12340000 + dma_length - 4);
    g_assert_cmphex(qtest_readl(destination, RASPI4_DMA_CS) & DMA_ACTIVE,
                    ==, 0);
    g_assert_cmphex(qtest_readl(destination, RASPI4_DMA_CS) &
                    (DMA_END | DMA_ISPAUSED), ==,
                    DMA_END | DMA_ISPAUSED);

    /* The pending one-word RNG refill deadline must migrate too. */
    qtest_clock_step_next(destination);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 4);
    g_assert_cmphex(qtest_readl(destination, RASPI4_RNG200_TOTAL_COUNT), ==,
                    160);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

static void test_pcie_vl805_usb_topology(void)
{
    g_autofree char *usb = qtest_hmp(global_qtest, "info usb");
    g_autofree char *qtree = qtest_hmp(global_qtest, "info qtree");

    g_assert_nonnull(strstr(usb,
        "Port 1, Speed 480 Mb/s, Product USB2.0 Hub"));
    g_assert_null(strstr(usb, "Raspberry Pi Internal Keyboard"));
    g_assert_nonnull(strstr(qtree, "dev: usb-via-3431-hub"));
    g_assert_null(strstr(qtree, "dev: usb-pi400-keyboard"));
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
}

static void test_pcie_system_reset(void)
{
    const uint32_t index = pcie_cfg_index(1, 31, 7);

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

    raspi4b_watchdog_reset();
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

    raspi4b_watchdog_reset();
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

    raspi4b_watchdog_reset();
    g_assert_false(readl(RASPI4_SPI0_CS) & SPI0_CS_DONE);
    g_assert_false(get_irq(RASPI4_SPI0_GIC_IRQ));

    writel(RASPI4_SPI0_CS, SPI0_CS_INTD | SPI0_CS_TA);
    g_assert_true(get_irq(RASPI4_SPI0_GIC_IRQ));
    writel(RASPI4_SPI0_CS, 0);
    g_assert_false(get_irq(RASPI4_SPI0_GIC_IRQ));
}

static void test_dwc2_reset_and_fifo_flush(void)
{
    const uint32_t gahbcfg = GAHBCFG_DMA_EN | GAHBCFG_GLBL_INTR_EN;
    const uint32_t gusbcfg = GUSBCFG_FORCEHOSTMODE |
                             (5 << GUSBCFG_USBTRDTIM_SHIFT);
    const uint32_t gnptxfsiz = (768 << FIFOSIZE_DEPTH_SHIFT) | 256;
    const uint32_t hptxfsiz = (256 << FIFOSIZE_DEPTH_SHIFT) | 1024;
    const uint32_t hcfg = HCFG_DESCDMA | HCFG_FSLSSUPP;
    const uint32_t hcchar = HCCHAR_CHENA | 64;

    g_assert_cmphex(readl(RASPI4_DWC2_REG(GRSTCTL)), ==,
                    GRSTCTL_AHBIDLE);

    writel(RASPI4_DWC2_REG(GAHBCFG), gahbcfg);
    writel(RASPI4_DWC2_REG(GUSBCFG), gusbcfg);
    writel(RASPI4_DWC2_REG(GNPTXFSIZ), gnptxfsiz);
    writel(RASPI4_DWC2_REG(HPTXFSIZ), hptxfsiz);
    writel(RASPI4_DWC2_REG(HCFG), hcfg);
    writel(RASPI4_DWC2_REG(HAINTMSK), 1);
    writel(RASPI4_DWC2_REG(HCINTMSK(0)), HCINTMSK_XFERCOMPL);
    writel(RASPI4_DWC2_REG(HCCHAR(0)), hcchar);

    /* A pending status bit proves that core reset clears masks, not status. */
    g_assert_true(readl(RASPI4_DWC2_REG(GINTSTS)) & GINTSTS_PTXFEMP);
    writel(RASPI4_DWC2_REG(GINTMSK), GINTSTS_PTXFEMP);
    g_assert_true(get_irq(RASPI4_DWC2_GIC_IRQ));

    writel(RASPI4_DWC2_REG(GRSTCTL),
           readl(RASPI4_DWC2_REG(GRSTCTL)) | GRSTCTL_CSFTRST);

    g_assert_cmphex(readl(RASPI4_DWC2_REG(GRSTCTL)), ==,
                    GRSTCTL_AHBIDLE);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GINTMSK)), ==, 0);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(HAINTMSK)), ==, 0);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(HCINTMSK(0))), ==, 0);
    g_assert_false(readl(RASPI4_DWC2_REG(HCCHAR(0))) & HCCHAR_CHENA);
    g_assert_false(get_irq(RASPI4_DWC2_GIC_IRQ));
    g_assert_true(readl(RASPI4_DWC2_REG(GINTSTS)) & GINTSTS_PTXFEMP);

    /* Configuration registers survive a core soft reset. */
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GAHBCFG)), ==, gahbcfg);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GUSBCFG)), ==, gusbcfg);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GNPTXFSIZ)), ==, gnptxfsiz);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(HPTXFSIZ)), ==, hptxfsiz);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(HCFG)), ==, hcfg);

    writel(RASPI4_DWC2_REG(GRSTCTL),
           GRSTCTL_TXFNUM(0x10) | GRSTCTL_TXFFLSH);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GRSTCTL)), ==,
                    GRSTCTL_AHBIDLE | GRSTCTL_TXFNUM(0x10));
    writel(RASPI4_DWC2_REG(GRSTCTL), GRSTCTL_RXFFLSH);
    g_assert_cmphex(readl(RASPI4_DWC2_REG(GRSTCTL)), ==,
                    GRSTCTL_AHBIDLE);
}

static void gpio_set_input(unsigned int pin, int level)
{
    qtest_set_irq_in(global_qtest, RASPI4_GPIO_QOM_PATH, NULL, pin, level);
}

static void test_gpio_events_and_interrupts(void)
{
    g_assert_cmphex(readl(RASPI4_GPIO_GPLEV0), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPLEV1), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS1), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPREN0), ==, 0);
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ2));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));

    /* A rising edge in GPIO bank 0 drives its bank and all-bank IRQs. */
    writel(RASPI4_GPIO_GPREN0, GPIO_PIN5);
    gpio_set_input(5, 1);
    g_assert_true(readl(RASPI4_GPIO_GPLEV0) & GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ2));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, 0);
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));

    /* Synchronous falling-edge detection uses the same event latch. */
    writel(RASPI4_GPIO_GPREN0, 0);
    writel(RASPI4_GPIO_GPFEN0, GPIO_PIN5);
    gpio_set_input(5, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);

    /* GPIOs 28-45 use the second bank interrupt. */
    writel(RASPI4_GPIO_GPFEN0, 0);
    writel(RASPI4_GPIO_GPAREN0, GPIO_PIN30);
    gpio_set_input(30, 1);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN30);
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ2));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN30);

    /* GPIOs 46-57 use the third bank interrupt. */
    gpio_set_input(50, 1);
    writel(RASPI4_GPIO_GPAFEN1, GPIO_PIN50_BANK1);
    gpio_set_input(50, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS1), ==, GPIO_PIN50_BANK1);
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ2));
    g_assert_true(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));
    writel(RASPI4_GPIO_GPEDS1, GPIO_PIN50_BANK1);

    /* Writes to reserved bits above GPIO57 are ignored. */
    writel(RASPI4_GPIO_GPREN1, UINT32_MAX);
    g_assert_cmphex(readl(RASPI4_GPIO_GPREN1), ==,
                    GPIO_BANK1_VALID_MASK);
    writel(RASPI4_GPIO_GPREN1, 0);

    /* An active level prevents its W1C status bit from clearing. */
    writel(RASPI4_GPIO_GPAREN0, 0);
    gpio_set_input(5, 1);
    writel(RASPI4_GPIO_GPHEN0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    gpio_set_input(5, 0);
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, 0);
    writel(RASPI4_GPIO_GPHEN0, 0);

    writel(RASPI4_GPIO_GPLEN0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, GPIO_PIN5);
    gpio_set_input(5, 1);
    writel(RASPI4_GPIO_GPEDS0, GPIO_PIN5);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, 0);
    writel(RASPI4_GPIO_GPLEN0, 0);

    /* GPSET retains an output latch while the pin remains an input. */
    writel(RASPI4_GPIO_GPSET1, GPIO_PIN57_BANK1);
    g_assert_false(readl(RASPI4_GPIO_GPLEV1) & GPIO_PIN57_BANK1);
    writel(RASPI4_GPIO_GPFSEL5, GPIO_FSEL5_PIN57_OUTPUT);
    g_assert_true(readl(RASPI4_GPIO_GPLEV1) & GPIO_PIN57_BANK1);
    writel(RASPI4_GPIO_GPCLR1, GPIO_PIN57_BANK1);
    g_assert_false(readl(RASPI4_GPIO_GPLEV1) & GPIO_PIN57_BANK1);

    qtest_system_reset(global_qtest);
    g_assert_cmphex(readl(RASPI4_GPIO_GPFSEL5), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPLEV0), ==,
                    GPIO_PIN5 | GPIO_PIN30);
    g_assert_cmphex(readl(RASPI4_GPIO_GPLEV1), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPREN0), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS0), ==, 0);
    g_assert_cmphex(readl(RASPI4_GPIO_GPEDS1), ==, 0);
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ0));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ1));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ2));
    g_assert_false(get_irq(RASPI4_GPIO_GIC_IRQ_ALL));
}

static void rng200_refill_words(unsigned int words)
{
    for (unsigned int i = 0; i < words; i++) {
        qtest_clock_step(global_qtest, RNG200_REFILL_1MHZ_NS);
    }
}

static void test_rng200_fifo_and_interrupts(void)
{
    uint32_t last_word = 0;

    g_assert_cmphex(readl(RASPI4_RNG200_CTRL), ==, 0);
    g_assert_cmphex(readl(RASPI4_RNG200_REVISION), ==, 0x00040001);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_EMPTY | RNG200_FIFO_THRESHOLD);

    /* Empty reads return the stale data latch and record an underrun. */
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_DATA), ==, 0);
    g_assert_true(readl(RASPI4_RNG200_INT_STATUS) &
                  RNG200_INT_FIFO_UNDERRUN);
    writel(RASPI4_RNG200_INT_STATUS, RNG200_INT_FIFO_UNDERRUN);

    writel(RASPI4_RNG200_TOTAL_LIMIT, 64);
    writel(RASPI4_RNG200_INT_ENABLE, RNG200_INT_TOTAL_LIMIT);
    writel(RASPI4_RNG200_CTRL,
           RNG200_CTRL_RATE_1MHZ | RNG200_CTRL_ENABLE);

    rng200_refill_words(1);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 1);
    g_assert_cmphex(readl(RASPI4_RNG200_TOTAL_COUNT), ==, 32);
    g_assert_true(readl(RASPI4_RNG200_INT_STATUS) & RNG200_INT_STARTUP);
    g_assert_false(get_irq(RASPI4_RNG200_GIC_IRQ));

    rng200_refill_words(1);
    g_assert_cmphex(readl(RASPI4_RNG200_TOTAL_COUNT), ==, 64);
    g_assert_true(readl(RASPI4_RNG200_INT_STATUS) &
                  RNG200_INT_TOTAL_LIMIT);
    g_assert_true(get_irq(RASPI4_RNG200_GIC_IRQ));
    writel(RASPI4_RNG200_INT_STATUS, RNG200_INT_TOTAL_LIMIT);
    g_assert_false(get_irq(RASPI4_RNG200_GIC_IRQ));

    rng200_refill_words(14);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_FULL | RNG200_FIFO_THRESHOLD | 16);
    g_assert_true(readl(RASPI4_RNG200_INT_STATUS) & RNG200_INT_FIFO_FULL);
    writel(RASPI4_RNG200_INT_ENABLE, RNG200_INT_FIFO_FULL);
    g_assert_true(get_irq(RASPI4_RNG200_GIC_IRQ));
    writel(RASPI4_RNG200_INT_STATUS, RNG200_INT_FIFO_FULL);
    g_assert_false(get_irq(RASPI4_RNG200_GIC_IRQ));

    for (unsigned int i = 0; i < 16; i++) {
        last_word = readl(RASPI4_RNG200_FIFO_DATA);
    }
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_EMPTY | RNG200_FIFO_THRESHOLD);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_DATA), ==, last_word);
    g_assert_true(readl(RASPI4_RNG200_INT_STATUS) &
                  RNG200_INT_FIFO_UNDERRUN);

    writel(RASPI4_RNG200_SOFT_RESET, 1);
    g_assert_cmphex(readl(RASPI4_RNG200_CTRL), ==, 0);
    g_assert_cmphex(readl(RASPI4_RNG200_TOTAL_COUNT), ==, 0);
    g_assert_cmphex(readl(RASPI4_RNG200_INT_STATUS), ==, 0);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_EMPTY | RNG200_FIFO_THRESHOLD);
}

static void test_rng200_rate_and_enable_mask(void)
{
    /* Any of the 13 ring-generator bits enables output. */
    writel(RASPI4_RNG200_CTRL, RNG200_CTRL_RING12);
    qtest_clock_step(global_qtest, RNG200_REFILL_8MHZ_NS - 1);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_EMPTY | RNG200_FIFO_THRESHOLD);
    qtest_clock_step(global_qtest, 1);
    g_assert_cmphex(readl(RASPI4_RNG200_FIFO_COUNT), ==,
                    RNG200_FIFO_THRESHOLD | 1);
    g_assert_cmphex(readl(RASPI4_RNG200_TOTAL_COUNT), ==, 32);
}

static int64_t thermal_qom_get_temperature(void)
{
    QDict *response;
    int64_t temperature;

    response = qmp("{ 'execute': 'qom-get', 'arguments': { 'path': %s, "
                   "'property': 'temperature' } }",
                   RASPI4_THERMAL_QOM_PATH);
    g_assert(qdict_haskey(response, "return"));
    temperature = qdict_get_int(response, "return");
    qobject_unref(response);
    return temperature;
}

static void thermal_qom_set_temperature(int64_t temperature)
{
    QDict *response;

    response = qmp("{ 'execute': 'qom-set', 'arguments': { 'path': %s, "
                   "'property': 'temperature', 'value': %" PRId64 " } }",
                   RASPI4_THERMAL_QOM_PATH, temperature);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);
}

static void test_thermal_temperature(void)
{
    g_assert_cmphex(readl(RASPI4_THERMAL_STATUS), ==,
                    THERMAL_STATUS_VALID | THERMAL_DEFAULT_RAW);
    g_assert_cmpint(thermal_qom_get_temperature(), ==, THERMAL_DEFAULT_MC);

    thermal_qom_set_temperature(THERMAL_TEST_MC);
    g_assert_cmpint(thermal_qom_get_temperature(), ==, THERMAL_TEST_MC);
    g_assert_cmphex(readl(RASPI4_THERMAL_STATUS), ==,
                    THERMAL_STATUS_VALID | THERMAL_TEST_RAW);

    {
        QDict *response = qmp(
            "{ 'execute': 'qom-set', 'arguments': { 'path': %s, "
            "'property': 'temperature', 'value': 500000 } }",
            RASPI4_THERMAL_QOM_PATH);

        g_assert(qdict_haskey(response, "error"));
        qobject_unref(response);
    }
    g_assert_cmpint(thermal_qom_get_temperature(), ==, THERMAL_TEST_MC);

    /* The guest-visible AVS status register is read-only. */
    writel(RASPI4_THERMAL_STATUS, 0);
    g_assert_cmphex(readl(RASPI4_THERMAL_STATUS), ==,
                    THERMAL_STATUS_VALID | THERMAL_TEST_RAW);
}

static void test_sd_card_on_emmc2(void)
{
    g_assert_true(qom_bus_has_sd_card(
        "/machine/soc/peripherals/emmc2/sd-bus"));
    g_assert_false(qom_bus_has_sd_card(
        "/machine/soc/peripherals/sdhci/sd-bus"));
}

static void property_request_qtest(QTestState *qts, uint32_t tag,
                                   const uint32_t *payload, size_t words,
                                   size_t response_bytes)
{
    uint32_t payload_bytes = words * sizeof(*payload);
    uint32_t total_bytes = 24 + payload_bytes;
    uint32_t response;
    size_t i;

    qtest_writel(qts, RASPI4_PROPERTY_BUFFER, total_bytes);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 4, 0);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 8, tag);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 12, payload_bytes);
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 16, 0);
    for (i = 0; i < words; i++) {
        qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 20 + i * 4,
                     payload[i]);
    }
    qtest_writel(qts, RASPI4_PROPERTY_BUFFER + 20 + payload_bytes, 0);

    qtest_writel(qts, RASPI4_MBOX_WRITE,
                 RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    response = qtest_readl(qts, RASPI4_MBOX_READ);

    g_assert_cmphex(response, ==,
                    RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 4), ==,
                    0x80000000);
    g_assert_cmphex(qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 16), ==,
                    0x80000000 | response_bytes);
}

static void property_request(uint32_t tag, const uint32_t *payload,
                             size_t words, size_t response_bytes)
{
    property_request_qtest(global_qtest, tag, payload, words, response_bytes);
}

static uint32_t property_payload_qtest(QTestState *qts, size_t word)
{
    return qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 20 + word * 4);
}

static uint32_t property_payload(size_t word)
{
    return property_payload_qtest(global_qtest, word);
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

static void test_firmware_clocks(void)
{
    static const uint32_t expected_current[RPI_FIRMWARE_NUM_CLK_ID] = {
        [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
        [RPI_FIRMWARE_UART_CLK_ID] = 48000000,
        [RPI_FIRMWARE_ARM_CLK_ID] = 1800000000,
        [RPI_FIRMWARE_CORE_CLK_ID] = 200000000,
        [RPI_FIRMWARE_V3D_CLK_ID] = 250000000,
        [RPI_FIRMWARE_H264_CLK_ID] = 250000000,
        [RPI_FIRMWARE_ISP_CLK_ID] = 250000000,
        [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
        [RPI_FIRMWARE_HEVC_CLK_ID] = 250000000,
        [RPI_FIRMWARE_M2MC_CLK_ID] = 120000000,
        [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 75000000,
    };
    static const uint32_t expected_min[RPI_FIRMWARE_NUM_CLK_ID] = {
        [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
        [RPI_FIRMWARE_ARM_CLK_ID] = 600000000,
        [RPI_FIRMWARE_CORE_CLK_ID] = 200000000,
        [RPI_FIRMWARE_V3D_CLK_ID] = 250000000,
        [RPI_FIRMWARE_H264_CLK_ID] = 250000000,
        [RPI_FIRMWARE_ISP_CLK_ID] = 250000000,
        [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
        [RPI_FIRMWARE_HEVC_CLK_ID] = 250000000,
        [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 75000000,
    };
    static const uint32_t expected_max[RPI_FIRMWARE_NUM_CLK_ID] = {
        [RPI_FIRMWARE_EMMC_CLK_ID] = 250000000,
        [RPI_FIRMWARE_UART_CLK_ID] = 1000000000,
        [RPI_FIRMWARE_ARM_CLK_ID] = 1800000000,
        [RPI_FIRMWARE_CORE_CLK_ID] = 500000000,
        [RPI_FIRMWARE_V3D_CLK_ID] = 500000000,
        [RPI_FIRMWARE_H264_CLK_ID] = 500000000,
        [RPI_FIRMWARE_ISP_CLK_ID] = 500000000,
        [RPI_FIRMWARE_SDRAM_CLK_ID] = 400000000,
        [RPI_FIRMWARE_PIXEL_CLK_ID] = 2400000000,
        [RPI_FIRMWARE_PWM_CLK_ID] = 500000000,
        [RPI_FIRMWARE_HEVC_CLK_ID] = 500000000,
        [RPI_FIRMWARE_EMMC2_CLK_ID] = 500000000,
        [RPI_FIRMWARE_M2MC_CLK_ID] = 600000000,
        [RPI_FIRMWARE_PIXEL_BVB_CLK_ID] = 324000000,
        [RPI_FIRMWARE_VEC_CLK_ID] = 108000000,
    };
    static const uint32_t query_tags[] = {
        RPI_FWREQ_GET_CLOCK_RATE,
        RPI_FWREQ_GET_MIN_CLOCK_RATE,
        RPI_FWREQ_GET_MAX_CLOCK_RATE,
    };
    static const uint32_t *const expected_rates[] = {
        expected_current,
        expected_min,
        expected_max,
    };
    uint32_t payload[RPI_FIRMWARE_NUM_CLK_ID * 2] = { 0 };
    unsigned int id;

    property_request(RPI_FWREQ_GET_CLOCKS, payload,
                     G_N_ELEMENTS(payload),
                     (RPI_FIRMWARE_DISP_CLK_ID - 1) * 2 * sizeof(uint32_t));

    for (id = 1; id < RPI_FIRMWARE_DISP_CLK_ID; id++) {
        size_t word = (id - 1) * 2;

        g_assert_cmphex(property_payload(word), ==, 0);
        g_assert_cmphex(property_payload(word + 1), ==, id);
    }

    /* The deliberately oversized request retains its zero sentinel. */
    g_assert_cmphex(property_payload((RPI_FIRMWARE_DISP_CLK_ID - 1) * 2),
                    ==, 0);
    g_assert_cmphex(property_payload((RPI_FIRMWARE_DISP_CLK_ID - 1) * 2 + 1),
                    ==, 0);

    for (unsigned int query = 0; query < G_N_ELEMENTS(query_tags); query++) {
        for (id = 1; id < RPI_FIRMWARE_DISP_CLK_ID; id++) {
            const uint32_t rate_query[] = { id, UINT32_MAX };

            property_request(query_tags[query], rate_query,
                             G_N_ELEMENTS(rate_query), sizeof(rate_query));
            g_assert_cmphex(property_payload(0), ==, id);
            g_assert_cmpuint(property_payload(1), ==,
                             expected_rates[query][id]);
        }
    }

    {
        const uint32_t invalid_query[] = {
            RPI_FIRMWARE_DISP_CLK_ID, UINT32_MAX,
        };
        const uint32_t set_core[] = {
            RPI_FIRMWARE_CORE_CLK_ID, 480000000, 0,
        };
        const uint32_t set_core_above_max[] = {
            RPI_FIRMWARE_CORE_CLK_ID, 600000000, 0,
        };
        const uint32_t get_core[] = {
            RPI_FIRMWARE_CORE_CLK_ID, UINT32_MAX,
        };

        property_request(RPI_FWREQ_GET_CLOCK_RATE, invalid_query,
                         G_N_ELEMENTS(invalid_query), sizeof(invalid_query));
        g_assert_cmphex(property_payload(1), ==, 0);

        property_request(RPI_FWREQ_SET_CLOCK_RATE, set_core,
                         G_N_ELEMENTS(set_core), 2 * sizeof(uint32_t));
        g_assert_cmpuint(property_payload(1), ==, 480000000);
        property_request(RPI_FWREQ_GET_CLOCK_RATE, get_core,
                         G_N_ELEMENTS(get_core), sizeof(get_core));
        g_assert_cmpuint(property_payload(1), ==, 480000000);

        property_request(RPI_FWREQ_SET_CLOCK_RATE, set_core_above_max,
                         G_N_ELEMENTS(set_core_above_max),
                         2 * sizeof(uint32_t));
        g_assert_cmpuint(property_payload(1), ==, 500000000);
    }
}

static void test_firmware_state_and_reboot(void)
{
    const uint32_t get_arm_clock[] = {
        RPI_FIRMWARE_ARM_CLK_ID, UINT32_MAX,
    };
    const uint32_t set_arm_clock_off[] = {
        RPI_FIRMWARE_ARM_CLK_ID, 0,
    };
    const uint32_t set_arm_clock_reserved[] = {
        RPI_FIRMWARE_ARM_CLK_ID, ~RPI_FIRMWARE_STATE_ENABLE,
    };
    const uint32_t get_invalid_clock[] = {
        RPI_FIRMWARE_DISP_CLK_ID, UINT32_MAX,
    };
    const uint32_t get_arm_domain[] = {
        RPI_FIRMWARE_ARM_DOMAIN_ID, UINT32_MAX,
    };
    const uint32_t get_v3d_domain[] = {
        RPI_FIRMWARE_V3D_DOMAIN_ID, UINT32_MAX,
    };
    const uint32_t set_v3d_domain_on[] = {
        RPI_FIRMWARE_V3D_DOMAIN_ID, RPI_FIRMWARE_STATE_ENABLE,
    };
    const uint32_t set_v3d_domain_off[] = {
        RPI_FIRMWARE_V3D_DOMAIN_ID, 0,
    };
    const uint32_t set_invalid_domain_on[] = {
        0, RPI_FIRMWARE_STATE_ENABLE,
    };
    const uint32_t get_invalid_domain[] = {
        RPI_FIRMWARE_NUM_DOMAIN_ID, UINT32_MAX,
    };

    /* A reset must discard a child response stalled behind a full mailbox. */
    writel(RASPI4_PROPERTY_BUFFER, 32);
    writel(RASPI4_PROPERTY_BUFFER + 8, RPI_FWREQ_GET_CLOCK_STATE);
    writel(RASPI4_PROPERTY_BUFFER + 12, 8);
    writel(RASPI4_PROPERTY_BUFFER + 20, RPI_FIRMWARE_ARM_CLK_ID);
    writel(RASPI4_PROPERTY_BUFFER + 24, UINT32_MAX);
    writel(RASPI4_PROPERTY_BUFFER + 28, RPI_FWREQ_PROPERTY_END);
    for (unsigned int i = 0; i <= MBOX_SIZE; i++) {
        writel(RASPI4_PROPERTY_BUFFER + 4, 0);
        writel(RASPI4_PROPERTY_BUFFER + 16, 0);
        writel(RASPI4_MBOX_WRITE,
               RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    }
    raspi4b_watchdog_reset();

    property_request(RPI_FWREQ_GET_CLOCK_STATE, get_arm_clock,
                     G_N_ELEMENTS(get_arm_clock), sizeof(get_arm_clock));
    g_assert_cmphex(property_payload(1), ==, RPI_FIRMWARE_STATE_ENABLE);

    property_request(RPI_FWREQ_SET_CLOCK_STATE, set_arm_clock_off,
                     G_N_ELEMENTS(set_arm_clock_off),
                     sizeof(set_arm_clock_off));
    g_assert_cmphex(property_payload(1), ==, 0);
    property_request(RPI_FWREQ_GET_CLOCK_STATE, get_arm_clock,
                     G_N_ELEMENTS(get_arm_clock), sizeof(get_arm_clock));
    g_assert_cmphex(property_payload(1), ==, 0);

    property_request(RPI_FWREQ_SET_CLOCK_STATE, set_arm_clock_reserved,
                     G_N_ELEMENTS(set_arm_clock_reserved),
                     sizeof(set_arm_clock_reserved));
    g_assert_cmphex(property_payload(1), ==, 0);

    property_request(RPI_FWREQ_GET_CLOCK_STATE, get_invalid_clock,
                     G_N_ELEMENTS(get_invalid_clock),
                     sizeof(get_invalid_clock));
    g_assert_cmphex(property_payload(1), ==,
                    RPI_FIRMWARE_STATE_NOT_EXIST);

    property_request(RPI_FWREQ_GET_DOMAIN_STATE, get_arm_domain,
                     G_N_ELEMENTS(get_arm_domain), sizeof(get_arm_domain));
    g_assert_cmphex(property_payload(1), ==, RPI_FIRMWARE_STATE_ENABLE);
    property_request(RPI_FWREQ_GET_DOMAIN_STATE, get_v3d_domain,
                     G_N_ELEMENTS(get_v3d_domain), sizeof(get_v3d_domain));
    g_assert_cmphex(property_payload(1), ==, 0);
    property_request(RPI_FWREQ_SET_DOMAIN_STATE, set_invalid_domain_on,
                     G_N_ELEMENTS(set_invalid_domain_on),
                     sizeof(set_invalid_domain_on));
    g_assert_cmphex(property_payload(1), ==, 0);
    property_request(RPI_FWREQ_GET_DOMAIN_STATE, get_invalid_domain,
                     G_N_ELEMENTS(get_invalid_domain),
                     sizeof(get_invalid_domain));
    g_assert_cmphex(property_payload(1), ==, 0);

    property_request(RPI_FWREQ_SET_DOMAIN_STATE, set_v3d_domain_on,
                     G_N_ELEMENTS(set_v3d_domain_on),
                     sizeof(set_v3d_domain_on));
    g_assert_cmphex(property_payload(1), ==, RPI_FIRMWARE_STATE_ENABLE);
    property_request(RPI_FWREQ_GET_DOMAIN_STATE, get_v3d_domain,
                     G_N_ELEMENTS(get_v3d_domain), sizeof(get_v3d_domain));
    g_assert_cmphex(property_payload(1), ==, RPI_FIRMWARE_STATE_ENABLE);

    property_request(RPI_FWREQ_SET_DOMAIN_STATE, set_v3d_domain_off,
                     G_N_ELEMENTS(set_v3d_domain_off),
                     sizeof(set_v3d_domain_off));
    g_assert_cmphex(property_payload(1), ==, 0);
    property_request(RPI_FWREQ_NOTIFY_REBOOT, NULL, 0, 0);

    property_request(RPI_FWREQ_SET_DOMAIN_STATE, set_v3d_domain_on,
                     G_N_ELEMENTS(set_v3d_domain_on),
                     sizeof(set_v3d_domain_on));
    raspi4b_watchdog_reset();

    property_request(RPI_FWREQ_GET_CLOCK_STATE, get_arm_clock,
                     G_N_ELEMENTS(get_arm_clock), sizeof(get_arm_clock));
    g_assert_cmphex(property_payload(1), ==, RPI_FIRMWARE_STATE_ENABLE);
    property_request(RPI_FWREQ_GET_DOMAIN_STATE, get_v3d_domain,
                     G_N_ELEMENTS(get_v3d_domain), sizeof(get_v3d_domain));
    g_assert_cmphex(property_payload(1), ==, 0);
}

static void test_firmware_notify_xhci_reset(void)
{
    const uint32_t invalid_bdf[] = { 0 };
    const uint32_t vl805_bdf[] = { pcie_cfg_index(1, 0, 0) };
    const uint16_t command = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

    pcie_vl805_test_start();
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_CONFIG, 1);
    writel(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD,
           VL805_XHCI_USBCMD_RUN | VL805_XHCI_USBCMD_INTE);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD),
                    ==, VL805_XHCI_USBCMD_RUN | VL805_XHCI_USBCMD_INTE);
    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                   VL805_XHCI_USBSTS_HCH);

    property_request(RPI_FWREQ_NOTIFY_XHCI_RESET, invalid_bdf,
                     G_N_ELEMENTS(invalid_bdf), sizeof(invalid_bdf));
    g_assert_cmphex(property_payload(0), ==, UINT32_MAX);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_CONFIG),
                    ==, 1);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD),
                    ==, VL805_XHCI_USBCMD_RUN | VL805_XHCI_USBCMD_INTE);
    g_assert_false(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                   VL805_XHCI_USBSTS_HCH);

    property_request(RPI_FWREQ_NOTIFY_XHCI_RESET, vl805_bdf,
                     G_N_ELEMENTS(vl805_bdf), sizeof(vl805_bdf));
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_CONFIG),
                    ==, 1);
    g_assert_cmphex(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBCMD),
                    ==, 0);
    g_assert_true(readl(RASPI4_PCIE_VL805_CPU_BAR + VL805_XHCI_USBSTS) &
                  VL805_XHCI_USBSTS_HCH);

    g_assert_cmphex(pcie_vl805_cfg_readw(PCI_COMMAND), ==, command);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_0), ==,
                    RASPI4_PCIE_VL805_PCI_BAR |
                    PCI_BASE_ADDRESS_MEM_TYPE_64);
    g_assert_cmphex(pcie_vl805_cfg_readl(PCI_BASE_ADDRESS_1), ==, 0);
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

typedef struct Raspi4TestData {
    void (*func)(void);
} Raspi4TestData;

static void raspi4b_test_run(const void *opaque)
{
    const Raspi4TestData *data = opaque;
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
#ifndef _WIN32
    int test_sockets[2];
    int ret;
#endif

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
    data->func();
    qtest_end();
#ifndef _WIN32
    close(genet_test_socket);
    genet_test_socket = -1;
#endif
}

static void raspi4b_add_test(const char *path, void (*func)(void))
{
    Raspi4TestData *data = g_new(Raspi4TestData, 1);

    data->func = func;
    qtest_add_data_func_full(path, data, raspi4b_test_run, g_free);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    pcie_has_edu = qtest_has_device("edu");

    raspi4b_add_test("/raspi4b/asb/bridge_ids", test_asb_bridge_ids);
    raspi4b_add_test("/raspi4b/cpu/configuration", test_cpu_configuration);
    raspi4b_add_test("/raspi4b/powermgt/watchdog", test_powermgt_watchdog);
    raspi4b_add_test("/raspi4b/interrupts/system_timer",
                     test_system_timer_interrupts);
    raspi4b_add_test("/raspi4b/interrupts/aon_l2",
                     test_aon_interrupts_and_reset);
    raspi4b_add_test("/raspi4b/interrupts/spi0", test_spi0_interrupt);
    raspi4b_add_test("/raspi4b/aux/modem_and_reset",
                     test_aux_uart_modem_and_reset);
    raspi4b_add_test("/raspi4b/dwc2/reset_and_fifo_flush",
                     test_dwc2_reset_and_fifo_flush);
    raspi4b_add_test("/raspi4b/gpio/events_and_interrupts",
                     test_gpio_events_and_interrupts);
    raspi4b_add_test("/raspi4b/rng200/fifo_and_interrupts",
                     test_rng200_fifo_and_interrupts);
    raspi4b_add_test("/raspi4b/rng200/rate_and_enable_mask",
                     test_rng200_rate_and_enable_mask);
#ifndef _WIN32
    raspi4b_add_test("/raspi4b/migration/soc_peripherals",
                     test_soc_peripheral_migration);
#endif
    raspi4b_add_test("/raspi4b/thermal/temperature",
                     test_thermal_temperature);
    raspi4b_add_test("/raspi4b/sd/card_on_emmc2", test_sd_card_on_emmc2);
    raspi4b_add_test("/raspi4b/firmware_gpio", test_firmware_gpio);
    raspi4b_add_test("/raspi4b/firmware_dma_channels",
                     test_firmware_dma_channels);
    raspi4b_add_test("/raspi4b/firmware/clocks", test_firmware_clocks);
    raspi4b_add_test("/raspi4b/firmware/state_and_reboot",
                     test_firmware_state_and_reboot);
    raspi4b_add_test("/raspi4b/firmware/notify_xhci_reset",
                     test_firmware_notify_xhci_reset);
    raspi4b_add_test("/raspi4b/pcie/root_config", test_pcie_root_config);
    raspi4b_add_test("/raspi4b/pcie/reset_link_and_mdio",
                     test_pcie_reset_link_and_mdio);
    raspi4b_add_test("/raspi4b/pcie/indirect_absent",
                     test_pcie_indirect_absent);
    raspi4b_add_test("/raspi4b/pcie/outbound_windows",
                     test_pcie_outbound_windows);
    raspi4b_add_test("/raspi4b/pcie/edu/config_and_mmio",
                     test_pcie_edu_config_and_mmio);
    raspi4b_add_test("/raspi4b/pcie/edu/intx", test_pcie_edu_intx);
    raspi4b_add_test("/raspi4b/pcie/edu/inbound_dma",
                     test_pcie_edu_inbound_dma);
    raspi4b_add_test("/raspi4b/pcie/edu/msi", test_pcie_edu_msi);
    raspi4b_add_test("/raspi4b/pcie/vl805/config_and_mmio",
                     test_pcie_vl805_config_and_mmio);
    raspi4b_add_test("/raspi4b/pcie/vl805/event_dma_msi",
                     test_pcie_vl805_event_dma_msi);
    raspi4b_add_test("/raspi4b/pcie/vl805/multisegment_event_ring",
                     test_pcie_vl805_multisegment_event_ring);
#ifndef _WIN32
    raspi4b_add_test("/raspi4b/pcie/vl805/multisegment_migration",
                     test_pcie_vl805_multisegment_migration);
#endif
    raspi4b_add_test("/raspi4b/pcie/vl805/usb_topology",
                     test_pcie_vl805_usb_topology);
    raspi4b_add_test("/raspi4b/pcie/vl805/perst", test_pcie_vl805_perst);
    raspi4b_add_test("/raspi4b/pcie/system_reset", test_pcie_system_reset);
    raspi4b_add_test("/raspi4b/genet/registers_and_mdio",
                     test_genet_registers_and_mdio);
#ifndef _WIN32
    raspi4b_add_test("/raspi4b/genet/packet_dma", test_genet_packet_dma);
#endif

    return g_test_run();
}
