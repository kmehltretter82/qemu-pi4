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
#include "libqos/sdhci-cmd.h"
#include "qemu/bswap.h"
#include "qemu/iov.h"
#include "qemu/sockets.h"
#include "qemu/timer.h"
#include "qemu/units.h"
#include "qobject/qdict.h"
#include "qobject/qlist.h"

#define RASPI4_MBOX_BASE       0xfe00b800
#define RASPI4_MBOX_READ       (RASPI4_MBOX_BASE + 0x80)
#define RASPI4_MBOX_WRITE      (RASPI4_MBOX_BASE + 0xa0)
#define RASPI4_PROPERTY_BUFFER 0x1000
#define RASPI4_BOARD_SERIAL    0x51454d55

#if HOST_LONG_BITS == 32
#define RASPI4B_BOARD_REVISION 0xa03111
#else
#define RASPI4B_BOARD_REVISION 0xb03115
#endif

#define RASPI4_VCRAM_BASE      0x3c000000
#define RASPI4_GPU_RAM_ALIAS   0x40000000
#define RASPI4_POWER_USB_HCD_ID 3
#define RASPI4_EMMC2_BASE      0xfe340000
#define RASPI4_EMMC2_CAPAREG   0x0000a52545ee6432ULL
#define RASPI4_EMMC1_BASE      0xfe300000

#define SDIO_CMD_IO_SEND_OP_COND  5
#define SDIO_CMD_IO_RW_DIRECT    52
#define SDIO_CMD_IO_RW_EXTENDED  53
#define SDIO_CMD52_WRITE          (1U << 31)
#define SDIO_CMD52_RAW            (1U << 27)
#define SDIO_CMD53_WRITE          (1U << 31)
#define SDIO_CMD53_BLOCK_MODE     (1U << 27)
#define SDIO_CMD53_INCREMENT      (1U << 26)
#define SDIO_CMD_FUNC_SHIFT       28
#define SDIO_CMD_ADDR_SHIFT       9

#define SDHC_NORINTSTS            0x30
#define SDHC_NORINTSTSEN          0x34
#define SDHC_NORINTSIGEN          0x38
#define SDHC_NIS_CARDINT          0x0100

#define CYW43455_SDIO_OCR_RESPONSE 0xb0ff8000U
#define CYW43455_SDIO_RCA          1
#define CYW43455_SDIO_CCCR_REV     0x00
#define CYW43455_SDIO_CCCR_IOEX    0x02
#define CYW43455_SDIO_CCCR_IORX    0x03
#define CYW43455_SDIO_CCCR_INTEN   0x04
#define CYW43455_SDIO_CCCR_INTPEND 0x05
#define CYW43455_SDIO_CCCR_CIS     0x09
#define CYW43455_SDIO_CIS_COMMON   0x1000
#define CYW43455_SDIO_FBR_SIZE     0x100
#define CYW43455_SDIO_FBR_CIS      0x09
#define CYW43455_SDIO_CIS_FUNC3    0x1300
#define CYW43455_SDIO_CHIPCLKCSR   0x1000e
#define CYW43455_SDIO_FORCE_ALP    0x01
#define CYW43455_SDIO_ALP_REQ      0x08
#define CYW43455_SDIO_HWREQ_OFF    0x20
#define CYW43455_SDIO_ALP_AVAIL    0x40
#define CYW43455_SDIO_SBADDRLOW    0x1000a
#define CYW43455_SDIO_SBADDRMID    0x1000b
#define CYW43455_SDIO_SBADDRHIGH   0x1000c
#define CYW43455_SDIO_SB_ACCESS    0x08000
#define CYW43455_SDIO_SB_OFFSET    0x07fff
#define CYW43455_SDIO_CHIP_ID_ADDR 0x18000000
#define CYW43455_SDIO_CHIP_ID      0x15294345
#define CYW43455_SDIO_EROM_PTR     0x180000fc
#define CYW43455_SDIO_EROM_BASE    0x18110000
#define CYW43455_SDIO_CR4_BASE     0x18002000
#define CYW43455_SDIO_CR4_WRAPPER  0x18102000
#define CYW43455_SDIO_CORE_BASE    0x18004000
#define CYW43455_SDIO_INTSTATUS    (CYW43455_SDIO_CORE_BASE + 0x20)
#define CYW43455_SDIO_HOSTINTMASK  (CYW43455_SDIO_CORE_BASE + 0x24)
#define CYW43455_SDIO_FRAME_IND    BIT(6)
#define CYW43455_SDPCM_HW_HEADER_LEN 4
#define CYW43455_SDPCM_HWEXT_LEN   8
#define CYW43455_SDPCM_HWEXT_LAST  BIT(24)
#define CYW43455_SDPCM_HEADER_LEN  12
#define CYW43455_SDPCM_EVENT_CHANNEL 1
#define CYW43455_SDPCM_DATA_CHANNEL 2
#define CYW43455_BCDC_HEADER_LEN   16
#define CYW43455_BCDC_DATA_HEADER_LEN 4
#define CYW43455_BCDC_DCMD_ERROR   BIT(0)
#define CYW43455_BCDC_DCMD_SET     BIT(1)
#define CYW43455_BCME_BADARG       (-2)
#define CYW43455_BCME_BADLEN       (-24)
#define CYW43455_C_SET_PROMISC     10
#define CYW43455_C_GET_PM           85
#define CYW43455_C_SET_PM           86
#define CYW43455_C_GET_PHY_NOISE   135
#define CYW43455_C_GET_BSS_INFO    136
#define CYW43455_C_GET_VAR         262
#define CYW43455_C_SET_VAR         263
#define CYW43455_TEST_CREDIT_DELAY_MS 25
#define CYW43455_TEST_CREDIT_WINDOW 5
#define CYW43455_DEFAULT_CREDIT_WINDOW 8
#define CYW43455_MAC_LEN           6
#define CYW43455_ARP_OL_AGENT      BIT(0)
#define CYW43455_ARP_OL_PEER_AUTO_REPLY BIT(3)
#define CYW43455_ICMP6_NEXT_HEADER 58
#define CYW43455_ND_PAYLOAD_LEN    32
#define CYW43455_STA_INFO_VERSION  4
#define CYW43455_STA_INFO_LEN      172
#define CYW43455_STA_INFO_FLAGS_OFFSET 8
#define CYW43455_STA_INFO_EA_OFFSET 16
#define CYW43455_STA_INFO_COUNT_OFFSET 24
#define CYW43455_STA_INFO_RATES_OFFSET 28
#define CYW43455_STA_INFO_TX_PKTS_OFFSET 52
#define CYW43455_STA_INFO_TX_FAIL_OFFSET 56
#define CYW43455_STA_INFO_RX_UCAST_OFFSET 60
#define CYW43455_STA_INFO_RX_MCAST_OFFSET 64
#define CYW43455_STA_INFO_TX_RATE_OFFSET 68
#define CYW43455_STA_INFO_RX_RATE_OFFSET 72
#define CYW43455_STA_INFO_TX_TOTAL_OFFSET 84
#define CYW43455_STA_INFO_RX_TOTAL_OFFSET 88
#define CYW43455_STA_INFO_TX_MCAST_OFFSET 92
#define CYW43455_STA_INFO_TX_BYTES_OFFSET 96
#define CYW43455_STA_INFO_RX_BYTES_OFFSET 104
#define CYW43455_STA_INFO_TX_UCAST_BYTES_OFFSET 112
#define CYW43455_STA_INFO_TX_MCAST_BYTES_OFFSET 120
#define CYW43455_STA_INFO_RX_UCAST_BYTES_OFFSET 128
#define CYW43455_STA_INFO_RX_MCAST_BYTES_OFFSET 136
#define CYW43455_STA_INFO_RSSI_OFFSET 144
#define CYW43455_STA_INFO_NF_OFFSET 148
#define CYW43455_STA_INFO_RX_LAST_RSSI_OFFSET 168
#define CYW43455_STA_FLAGS         0x0000403a
#define CYW43455_BSS_INFO_VERSION  109
#define CYW43455_BSS_INFO_LEN      128
#define CYW43455_BSS_INFO_PREFIX_LEN 4
#define CYW43455_BSS_INFO_BSSID_OFFSET 8
#define CYW43455_BSS_INFO_BEACON_OFFSET 14
#define CYW43455_BSS_INFO_CAP_OFFSET 16
#define CYW43455_BSS_INFO_SSID_LEN_OFFSET 18
#define CYW43455_BSS_INFO_SSID_OFFSET 19
#define CYW43455_BSS_INFO_RATE_COUNT_OFFSET 52
#define CYW43455_BSS_INFO_RATES_OFFSET 56
#define CYW43455_BSS_INFO_CHANSPEC_OFFSET 72
#define CYW43455_BSS_INFO_DTIM_OFFSET 76
#define CYW43455_BSS_INFO_RSSI_OFFSET 78
#define CYW43455_BSS_INFO_NOISE_OFFSET 80
#define CYW43455_BSS_INFO_N_CAP_OFFSET 81
#define CYW43455_BSS_INFO_CTL_CH_OFFSET 88
#define CYW43455_BSS_INFO_IE_OFFSET 116
#define CYW43455_BSS_INFO_IE_LEN_OFFSET 120
#define CYW43455_BSS_INFO_SNR_OFFSET 124
#define CYW43455_BSS_CAPABILITIES  0x0421
#define CYW43455_BSS_BEACON_PERIOD 100
#define CYW43455_BSS_DTIM_PERIOD   1
#define CYW43455_BSS_SSID          "qemu-cyw43455"
#define CYW43455_BSS_PAYLOAD_LEN   160
#define CYW43455_WLC_BAND_5G       1
#define CYW43455_WLC_BAND_2G       2
#define CYW43455_PM_OFF             0
#define CYW43455_PM_FAST            2
#define CYW43455_OBSS_COEX_AUTO    (-1)
#define CYW43455_OBSS_COEX_OFF       0
#define CYW43455_OBSS_COEX_ON        1
#define CYW43455_BW_CAP_20MHZ      BIT(0)
#define CYW43455_BW_CAP_40MHZ      BIT(1)
#define CYW43455_BW_CAP_2G         \
    (CYW43455_BW_CAP_20MHZ | CYW43455_BW_CAP_40MHZ)
#define CYW43455_CHANSPEC_2G_CH1_20MHZ 0x1001
#define CYW43455_CHANSPEC_2G_CH13_20MHZ 0x100d
#define CYW43455_CHANSPEC_2G_CH13_40MHZ 0x190b
#define CYW43455_CHANSPEC_20_COUNT  13
#define CYW43455_CHANSPEC_40_COUNT  18
#define CYW43455_CHANSPEC_ALL_COUNT \
    (CYW43455_CHANSPEC_20_COUNT + CYW43455_CHANSPEC_40_COUNT)
#define CYW43455_CHANSPEC_LIST_LEN(_count) \
    (sizeof(uint32_t) * (1 + (_count)))
#define CYW43455_LINK_RATE_KBPS    72200
#define CYW43455_SIGNAL_DBM        (-28)
#define CYW43455_NOISE_DBM         (-95)
#define CYW43455_WLC_VERSION_LEN   16
#define CYW43455_STA_PAYLOAD_LEN   256
#define CYW43455_ETH_HEADER_LEN    14
#define CYW43455_BRCM_EVENT_HEADER_LEN 10
#define CYW43455_EVENT_MSG_LEN     48
#define CYW43455_EVENT_FRAME_LEN   \
    (CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_DATA_HEADER_LEN + \
     CYW43455_ETH_HEADER_LEN + CYW43455_BRCM_EVENT_HEADER_LEN + \
     CYW43455_EVENT_MSG_LEN)
#define CYW43455_EVENT_ESCAN_RESULT 69
#define CYW43455_EVENT_STATUS_SUCCESS 0
#define CYW43455_EVENT_STATUS_PARTIAL 8
#define CYW43455_ESCAN_REQ_VERSION_V1 1
#define CYW43455_ESCAN_REQ_VERSION_V2 2
#define CYW43455_ESCAN_RESULT_VERSION 1
#define CYW43455_ESCAN_ACTION_START 1
#define CYW43455_ESCAN_HEADER_LEN  8
#define CYW43455_SCAN_PARAMS_V1_FIXED_LEN 64
#define CYW43455_SCAN_PARAMS_V2_FIXED_LEN 72
#define CYW43455_SCAN_SSID_LEN     36
#define CYW43455_ESCAN_RESULT_FIXED_LEN 12
#define CYW43455_BSS_INFO_IE_BYTES 24
#define CYW43455_ESCAN_BSS_INFO_LEN \
    (CYW43455_BSS_INFO_LEN + CYW43455_BSS_INFO_IE_BYTES)
#define CYW43455_ESCAN_RESULT_LEN  \
    (CYW43455_ESCAN_RESULT_FIXED_LEN + CYW43455_ESCAN_BSS_INFO_LEN)
#define CYW43455_ESCAN_EVENT_FRAME_LEN \
    (CYW43455_EVENT_FRAME_LEN + CYW43455_ESCAN_RESULT_LEN)
#define CYW43455_ESCAN_V1_REQUEST_LEN \
    (CYW43455_ESCAN_HEADER_LEN + CYW43455_SCAN_PARAMS_V1_FIXED_LEN + \
     sizeof(uint32_t) + CYW43455_SCAN_SSID_LEN)
#define CYW43455_ESCAN_V2_REQUEST_LEN \
    (CYW43455_ESCAN_HEADER_LEN + CYW43455_SCAN_PARAMS_V2_FIXED_LEN + \
     sizeof(uint32_t) + CYW43455_SCAN_SSID_LEN)
#define CYW43455_SURVEY_DURATION_MS 80
#define CYW43455_SURVEY_PAYLOAD_LEN 256

#define PROPERTY_RESPONSE_SUCCESS 0x80000000
#define PROPERTY_RESPONSE_ERROR   0x80000001

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
#define RASPI4_GICD_BASE           0xff841000
#define RASPI4_GICC_BASE           0xff842000
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
#define AUX_ENABLE_SPI1           (1U << 1)
#define AUX_ENABLE_SPI2           (1U << 2)
#define AUX_IER_TX_INT            (1U << 1)
#define AUX_MCR_RTS               (1U << 1)
#define AUX_MSR_CTS               (1U << 4)
#define AUX_IRQ_SPI1              (1U << 1)
#define AUX_IRQ_SPI2              (1U << 2)

#define RASPI4_AUX_SPI1_BASE      (RASPI4_AUX_BASE + 0x80)
#define RASPI4_AUX_SPI2_BASE      (RASPI4_AUX_BASE + 0xc0)
#define AUX_SPI_CNTL0(_base)      ((_base) + 0x00)
#define AUX_SPI_CNTL1(_base)      ((_base) + 0x04)
#define AUX_SPI_STAT(_base)       ((_base) + 0x08)
#define AUX_SPI_PEEK(_base)       ((_base) + 0x0c)
#define AUX_SPI_IO(_base)         ((_base) + 0x20)
#define AUX_SPI_TXHOLD(_base)     ((_base) + 0x30)
#define AUX_SPI_CNTL0_VAR_WIDTH   (1U << 14)
#define AUX_SPI_CNTL0_ENABLE      (1U << 11)
#define AUX_SPI_CNTL0_MSBF_OUT    (1U << 6)
#define AUX_SPI_CNTL1_TXEMPTY     (1U << 7)
#define AUX_SPI_CNTL1_MSBF_IN     (1U << 1)
#define AUX_SPI_STAT_IDLE          0x00000280

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
#define RASPI4_PCIE_ROOT_EXP_CAP     0xac

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
#define BCM2711_PCIE_HW_REVISION     0x0304U
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
#define RASPI4_PCIE_GIC_EVENT        147
#define RASPI4_PCIE_EDU_GIC_MSI      148
#define RASPI4_PCIE_EVENT_OUTPUT     4

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

static bool qom_bus_has_device_type(const char *path, const char *type)
{
    QDict *response;
    QList *properties;
    QListEntry *entry;
    g_autofree char *link_type = g_strdup_printf("link<%s>", type);
    bool found = false;

    response = qtest_qmp(global_qtest,
                         "{ 'execute': 'qom-list',"
                         "  'arguments': { 'path': %s } }", path);
    g_assert(qdict_haskey(response, "return"));
    properties = qdict_get_qlist(response, "return");

    QLIST_FOREACH_ENTRY(properties, entry) {
        QDict *property = qobject_to(QDict, qlist_entry_obj(entry));

        if (g_str_equal(qdict_get_str(property, "type"), link_type)) {
            found = true;
            break;
        }
    }

    qobject_unref(response);
    return found;
}

static bool qom_bus_has_sd_card(const char *path)
{
    return qom_bus_has_device_type(path, "sd-card");
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

static void test_aux_spi_pio_and_reset(void)
{
    const uint32_t cntl0 = AUX_SPI_CNTL0_ENABLE |
                           AUX_SPI_CNTL0_VAR_WIDTH |
                           AUX_SPI_CNTL0_MSBF_OUT;
    const uint32_t cntl1 = AUX_SPI_CNTL1_MSBF_IN;

    /* Pi 400 gates AUX SPI bank reads and ignores writes while disabled. */
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==, 0);
    writel(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE), cntl0);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE), cntl1);
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);

    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_SPI1);
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    AUX_SPI_STAT_IDLE);

    writel(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE), cntl0);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE), cntl1);
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, cntl0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, cntl1);

    /* The empty QEMU SSI bus returns zero, but preserves PIO word framing. */
    writel(AUX_SPI_TXHOLD(RASPI4_AUX_SPI1_BASE), 0x18a1b2c3);
    writel(AUX_SPI_IO(RASPI4_AUX_SPI1_BASE), 0x08d40000);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00040200);
    g_assert_cmphex(readl(AUX_SPI_PEEK(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00040200);
    g_assert_cmphex(readl(AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00010200);
    g_assert_cmphex(readl(AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    AUX_SPI_STAT_IDLE);

    /* The defined SPI interrupt bit stays live while its register bank gates. */
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE),
           cntl1 | AUX_SPI_CNTL1_TXEMPTY);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, AUX_IRQ_SPI1);
    g_assert_true(get_irq(RASPI4_AUX_GIC_IRQ));
    writel(RASPI4_AUX_ENABLES, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, AUX_IRQ_SPI1);
    g_assert_true(get_irq(RASPI4_AUX_GIC_IRQ));

    /* SPI1 and SPI2 contribute distinct bits to the same AUX interrupt. */
    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_SPI1 | AUX_ENABLE_SPI2);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI2_BASE),
           cntl1 | AUX_SPI_CNTL1_TXEMPTY);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==,
                    AUX_IRQ_SPI1 | AUX_IRQ_SPI2);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE), cntl1);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, AUX_IRQ_SPI2);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI2_BASE), cntl1);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 0);
    g_assert_false(get_irq(RASPI4_AUX_GIC_IRQ));

    writel(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE), cntl0);
    writel(AUX_SPI_IO(RASPI4_AUX_SPI1_BASE), 0x18a1b2c3);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00030200);
    raspi4b_watchdog_reset();
    g_assert_cmphex(readl(RASPI4_AUX_ENABLES), ==, 0);
    g_assert_cmphex(readl(RASPI4_AUX_IRQ), ==, 0);
    g_assert_false(get_irq(RASPI4_AUX_GIC_IRQ));
    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_SPI1);
    g_assert_cmphex(readl(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(readl(AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    AUX_SPI_STAT_IDLE);
}

static void aux_spi_flash_read_id(uint64_t spi_base)
{
    const uint32_t cntl0 = AUX_SPI_CNTL0_ENABLE |
                           AUX_SPI_CNTL0_VAR_WIDTH |
                           AUX_SPI_CNTL0_MSBF_OUT;
    const uint32_t cntl1 = AUX_SPI_CNTL1_MSBF_IN;

    writel(AUX_SPI_CNTL0(spi_base), cntl0);
    writel(AUX_SPI_CNTL1(spi_base), cntl1);

    /* TXHOLD preserves virtual CS; the final IO word ends the transaction. */
    writel(AUX_SPI_TXHOLD(spi_base), 0x089f0000);
    writel(AUX_SPI_IO(spi_base), 0x18000000);
    /*
     * Received words are right-aligned: the three-byte read returns the first
     * two identity bytes below a zero turnaround byte, and the trailing
     * one-byte word returns the capacity byte in bits 7 to 0.
     */
    g_assert_cmphex(readl(AUX_SPI_IO(spi_base)), ==, 0x002020);
    g_assert_cmphex(readl(AUX_SPI_IO(spi_base)), ==, 0x14);
    g_assert_cmphex(readl(AUX_SPI_STAT(spi_base)), ==,
                    AUX_SPI_STAT_IDLE);
}

/*
 * A variable-width word is transmitted from the most significant bits but
 * received into the least significant ones, so a short receive word must not
 * be returned in the transmit alignment.  Linux's spi-bcm2835aux reads byte i
 * of an n-byte result from data >> (8 * (n - i - 1)); left-aligning it would
 * corrupt every transfer whose length is not a multiple of three.
 */
static void aux_spi_flash_short_word_alignment(uint64_t spi_base)
{
    const uint32_t cntl0 = AUX_SPI_CNTL0_ENABLE |
                           AUX_SPI_CNTL0_VAR_WIDTH |
                           AUX_SPI_CNTL0_MSBF_OUT;
    const uint32_t cntl1 = AUX_SPI_CNTL1_MSBF_IN;

    writel(AUX_SPI_CNTL0(spi_base), cntl0);
    writel(AUX_SPI_CNTL1(spi_base), cntl1);

    /*
     * A one-byte command word and a one-byte data word leave exactly two
     * bytes in the receive FIFO: the command turnaround byte and the first
     * identity byte.  Right-aligned they read back as 0x0020, whereas the
     * transmit alignment would place them at 0x2000.
     */
    writel(AUX_SPI_TXHOLD(spi_base), 0x089f0000);
    writel(AUX_SPI_IO(spi_base), 0x08000000);
    g_assert_cmphex(readl(AUX_SPI_IO(spi_base)), ==, 0x0020);
    g_assert_cmphex(readl(AUX_SPI_STAT(spi_base)), ==, AUX_SPI_STAT_IDLE);
}

static void test_aux_spi_flash_transaction(void)
{
    /* One standard QEMU M25P80 peripheral is attached to each AUX SSI bus. */
    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_SPI1 | AUX_ENABLE_SPI2);
    aux_spi_flash_read_id(RASPI4_AUX_SPI1_BASE);
    aux_spi_flash_read_id(RASPI4_AUX_SPI1_BASE);
    aux_spi_flash_read_id(RASPI4_AUX_SPI2_BASE);
    aux_spi_flash_read_id(RASPI4_AUX_SPI2_BASE);
    aux_spi_flash_short_word_alignment(RASPI4_AUX_SPI1_BASE);
    aux_spi_flash_short_word_alignment(RASPI4_AUX_SPI2_BASE);
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

static void test_pcie_root_event_irq(void)
{
    const uint64_t slot_ctl = RASPI4_PCIE_BASE +
                              RASPI4_PCIE_ROOT_EXP_CAP + PCI_EXP_SLTCTL;
    const uint64_t slot_sta = RASPI4_PCIE_BASE +
                              RASPI4_PCIE_ROOT_EXP_CAP + PCI_EXP_SLTSTA;
    unsigned int irq;

    if (!pcie_edu_test_start()) {
        return;
    }

    /*
     * A root-port slot command generates QEMU's standard command-completed
     * service event.  BCM2711 delivers it through SPI 147, independently of
     * the downstream device's INTx lines.
     */
    writew(slot_ctl, PCI_EXP_SLTCTL_HPIE | PCI_EXP_SLTCTL_CCIE);
    g_assert_true(readw(slot_sta) & PCI_EXP_SLTSTA_CC);
    g_assert_true(get_irq(RASPI4_PCIE_GIC_EVENT));
    for (irq = RASPI4_PCIE_INTX_FIRST;
         irq <= RASPI4_PCIE_INTX_LAST; irq++) {
        g_assert_false(get_irq(irq));
    }

    /* A simultaneous downstream INTx must retain its ordinary SPI route. */
    writel(RASPI4_PCIE_EDU_CPU_BAR + EDU_IRQ_RAISE, 1);
    g_assert_true(get_irq(RASPI4_PCIE_GIC_EVENT));
    g_assert_true(get_irq(RASPI4_PCIE_EDU_GIC_INTX));

    writew(slot_sta, PCI_EXP_SLTSTA_CC);
    g_assert_false(get_irq(RASPI4_PCIE_GIC_EVENT));
    g_assert_true(get_irq(RASPI4_PCIE_EDU_GIC_INTX));

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
    const uint64_t slot_ctl = RASPI4_PCIE_BASE +
                              RASPI4_PCIE_ROOT_EXP_CAP + PCI_EXP_SLTCTL;
    const uint64_t slot_sta = RASPI4_PCIE_BASE +
                              RASPI4_PCIE_ROOT_EXP_CAP + PCI_EXP_SLTSTA;
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

    /* A pending root service event must migrate with the controller IRQ. */
    writew(slot_ctl, PCI_EXP_SLTCTL_HPIE | PCI_EXP_SLTCTL_CCIE);
    g_assert_true(readw(slot_sta) & PCI_EXP_SLTSTA_CC);
    g_assert_true(get_irq(RASPI4_PCIE_GIC_EVENT));

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
    g_string_append(cmd_line, " -incoming defer");
    destination = qtest_init(cmd_line->str);
    close(net_sockets[1]);
    qtest_irq_intercept_out_named(destination,
                                  "/machine/soc/peripherals/pcie",
                                  "sysbus-irq");
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    g_assert_true(qtest_readw(destination, slot_sta) & PCI_EXP_SLTSTA_CC);
    g_assert_true(qtest_get_irq(destination, RASPI4_PCIE_EVENT_OUTPUT));
    qtest_writew(destination, slot_sta, PCI_EXP_SLTSTA_CC);
    g_assert_false(qtest_get_irq(destination, RASPI4_PCIE_EVENT_OUTPUT));

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

static void test_aux_spi_flash_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line = g_string_new("-machine raspi4b");
    QTestState *source = global_qtest;
    QTestState *destination;
    const uint32_t cntl0 = AUX_SPI_CNTL0_ENABLE |
                           AUX_SPI_CNTL0_VAR_WIDTH |
                           AUX_SPI_CNTL0_MSBF_OUT;
    const uint32_t cntl1 = AUX_SPI_CNTL1_MSBF_IN;
    int net_sockets[2];
    int ret;

    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_SPI1);
    writel(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE), cntl0);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE), cntl1);

    /* Start a JEDEC-ID exchange but leave its virtual CS asserted. */
    writel(AUX_SPI_TXHOLD(RASPI4_AUX_SPI1_BASE), 0x089f0000);
    g_assert_cmphex(readl(AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==, 0);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-aux-spi-migration-XXXXXX", &error);
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
    g_string_append(cmd_line, " -device m25p80,id=spi1flash,bus=spi1");
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, net_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(cmd_line, " -nic socket,fd=%d,model=genet",
                           net_sockets[1]);
    g_string_append(cmd_line, " -incoming defer");
    destination = qtest_init(cmd_line->str);
    close(net_sockets[1]);
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_ENABLES), ==,
                    AUX_ENABLE_SPI1);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==,
                    cntl0);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==,
                    cntl1);

    /* Complete the source transaction on the destination VM. */
    qtest_writel(destination, AUX_SPI_IO(RASPI4_AUX_SPI1_BASE), 0x18000000);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00202014);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==,
                    0);

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
    const uint32_t aux_spi_cntl0 = AUX_SPI_CNTL0_ENABLE |
                                   AUX_SPI_CNTL0_VAR_WIDTH |
                                   AUX_SPI_CNTL0_MSBF_OUT;
    const uint32_t aux_spi_cntl1 = AUX_SPI_CNTL1_MSBF_IN;
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
    const uint32_t set_customer_otp[] = {
        1, 1, 0xa5c35a3c,
    };
    const uint32_t get_customer_otp[] = {
        1, 1, 0,
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
    property_request_qtest(source, RPI_FWREQ_SET_CUSTOMER_OTP,
                           set_customer_otp,
                           G_N_ELEMENTS(set_customer_otp),
                           sizeof(uint32_t));
    property_request_qtest(source, RPI_FWREQ_GET_CUSTOMER_OTP,
                           get_customer_otp,
                           G_N_ELEMENTS(get_customer_otp),
                           sizeof(get_customer_otp));
    g_assert_cmphex(property_payload_qtest(source, 2), ==, 0xa5c35a3c);
    writel(RASPI4_AUX_ENABLES, AUX_ENABLE_UART | AUX_ENABLE_SPI1);
    writel(RASPI4_AUX_MU_MCR, AUX_MCR_RTS);
    writel(RASPI4_AUX_MU_SCRATCH, 0xa5);
    writel(AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE), aux_spi_cntl0);
    writel(AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE), aux_spi_cntl1);
    writel(AUX_SPI_TXHOLD(RASPI4_AUX_SPI1_BASE), 0x18a1b2c3);
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
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==, 0);
    qtest_writel(destination, RASPI4_AUX_ENABLES,
                 AUX_ENABLE_UART | AUX_ENABLE_SPI1);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_MCR), ==,
                    AUX_MCR_RTS);
    g_assert_cmphex(qtest_readl(destination, RASPI4_AUX_MU_SCRATCH), ==,
                    0xa5);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL0(RASPI4_AUX_SPI1_BASE)), ==,
                    aux_spi_cntl0);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_CNTL1(RASPI4_AUX_SPI1_BASE)), ==,
                    aux_spi_cntl1);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    0x00030200);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_IO(RASPI4_AUX_SPI1_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(destination,
                                AUX_SPI_STAT(RASPI4_AUX_SPI1_BASE)), ==,
                    AUX_SPI_STAT_IDLE);
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
    property_request_qtest(destination, RPI_FWREQ_GET_CUSTOMER_OTP,
                           get_customer_otp,
                           G_N_ELEMENTS(get_customer_otp),
                           sizeof(get_customer_otp));
    g_assert_cmphex(property_payload_qtest(destination, 2), ==, 0xa5c35a3c);

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

/*
 * Identity, field widths and fixed configuration of the GIC-400, in the
 * Non-secure view, as captured from a Pi 400 on 2026-10-03.
 */
static void test_gic400_identity(void)
{
    const uint64_t d = RASPI4_GICD_BASE, c = RASPI4_GICC_BASE;
    uint32_t cfg;

    g_assert_cmphex(readl(d + 0x004), ==, 0x0000fc67);  /* GICD_TYPER */
    g_assert_cmphex(readl(d + 0x008), ==, 0x0200143b);  /* GICD_IIDR */
    g_assert_cmphex(readl(c + 0x0fc), ==, 0x0202143b);  /* GICC_IIDR */
    g_assert_cmphex(readl(d + 0x080), ==, 0);           /* IGROUPR0: RAZ */

    /* SGI and PPI configuration is fixed; only PPIs 25-31 exist */
    g_assert_cmphex(readl(d + 0xc00), ==, 0xaaaaaaaa);
    g_assert_cmphex(readl(d + 0xc04), ==, 0x55540000);
    writel(d + 0xc04, 0xaaabffff);
    g_assert_cmphex(readl(d + 0xc04), ==, 0x55540000);

    /* INTID 255 exists; bit 0 of an SPI configuration field reads as one */
    cfg = readl(d + 0xc3c);
    writel(d + 0xc3c, cfg | (3U << 30));
    g_assert_cmphex(readl(d + 0xc3c) >> 30, ==, 3);
    writel(d + 0xc3c, cfg & ~(3U << 30));
    g_assert_cmphex(readl(d + 0xc3c) >> 30, ==, 1);

    /* Four Non-secure priority bits and four CPU interfaces */
    writeb(d + 0x4ff, 0xff);
    g_assert_cmphex(readb(d + 0x4ff), ==, 0xf0);
    writeb(d + 0x4ff, 0);
    writeb(d + 0x8ff, 0xff);
    g_assert_cmphex(readb(d + 0x8ff), ==, 0x0f);
    writeb(d + 0x8ff, 0);

    writel(c + 0x004, 0xff);
    g_assert_cmphex(readl(c + 0x004), ==, 0xf0);
    writel(c + 0x004, 0x01);
    g_assert_cmphex(readl(c + 0x004), ==, 0);
    g_assert_cmphex(readl(c + 0x008), ==, 3);           /* GICC_BPR */
    writel(c + 0x008, 0);
    g_assert_cmphex(readl(c + 0x008), ==, 3);
    g_assert_cmphex(readl(c + 0x01c), ==, 0);           /* GICC_ABPR: RAZ */

    /* The value Linux leaves in GICC_CTLR reads back whole */
    writel(c + 0x000, 0x261);
    g_assert_cmphex(readl(c + 0x000), ==, 0x261);
    writel(c + 0x000, 0);
}

/*
 * A GIC-400 reports a pending interrupt in GICC_HPPIR while GICC_PMR alone
 * masks it, and records an active priority of 0x40 as bit 4 of GICC_APR0.
 */
static void test_gic400_masked_hppir_and_apr(void)
{
    const uint64_t d = RASPI4_GICD_BASE, c = RASPI4_GICC_BASE;
    const uint32_t bit = 1U << 31;                      /* INTID 255 */
    uint32_t iar;

    writel(d + 0x000, 1);
    writel(c + 0x000, 0x261);
    writeb(d + 0x4ff, 0x40);
    writeb(d + 0x8ff, 0x01);
    writel(d + 0x11c, bit);                             /* ISENABLER7 */

    writel(c + 0x004, 0x40);
    writel(d + 0x21c, bit);                             /* ISPENDR7 */
    g_assert_cmpuint(readl(c + 0x018), ==, 255);
    g_assert_cmpuint(readl(c + 0x00c), ==, 1023);
    g_assert_true(readl(d + 0x21c) & bit);

    writel(c + 0x004, 0xf0);
    iar = readl(c + 0x00c);
    g_assert_cmpuint(iar, ==, 255);
    g_assert_cmphex(readl(c + 0x014), ==, 0x40);        /* GICC_RPR */
    g_assert_cmphex(readl(c + 0x0d0), ==, 0x10);        /* GICC_APR0 */
    g_assert_false(readl(d + 0x21c) & bit);
    writel(c + 0x010, iar);
    g_assert_cmphex(readl(c + 0x0d0), ==, 0);
    writel(c + 0x1000, iar);
    g_assert_false(readl(d + 0x31c) & bit);

    writel(d + 0x19c, bit);                             /* ICENABLER7 */
    writel(c + 0x000, 0);
    writel(d + 0x000, 0);
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

    /* BCM2711 exposes five-bit packet and transfer-size counters. */
    g_assert_cmphex((readl(RASPI4_DWC2_REG(GHWCFG3)) &
                     GHWCFG3_PACKET_SIZE_CNTR_WIDTH_MASK) >>
                    GHWCFG3_PACKET_SIZE_CNTR_WIDTH_SHIFT, ==, 5);
    g_assert_cmphex((readl(RASPI4_DWC2_REG(GHWCFG3)) &
                     GHWCFG3_XFER_SIZE_CNTR_WIDTH_MASK) >>
                    GHWCFG3_XFER_SIZE_CNTR_WIDTH_SHIFT, ==, 5);

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
    QDict *response;

    g_assert_cmphex(readq(RASPI4_EMMC2_BASE + 0x40), ==,
                    RASPI4_EMMC2_CAPAREG);
    g_assert_true(qom_bus_has_sd_card(
        "/machine/soc/peripherals/emmc2/sd-bus"));
    g_assert_false(qom_bus_has_sd_card(
        "/machine/soc/peripherals/sdhci/sd-bus"));

    response = qmp("{ 'execute': 'qom-get', 'arguments': {"
                   "  'path': '/machine/soc/peripherals/emmc2',"
                   "  'property': 'dma' } }");
    g_assert(qdict_haskey(response, "return"));
    g_assert_cmpstr(qdict_get_str(response, "return"), ==,
                    "/machine/soc/peripherals/bcm2835-gpu[0]");
    qobject_unref(response);
    g_assert_false(qom_bus_has_device_type(
        "/machine/soc/peripherals/sdhci/sd-bus", "cyw43455-sdio"));
}

static uint32_t cyw43455_sdio_cmd52_qtest(QTestState *qts, bool write,
                                          bool read_after_write,
                                          unsigned int function,
                                          uint32_t address, uint8_t value)
{
    uint32_t argument = function << SDIO_CMD_FUNC_SHIFT;

    argument |= address << SDIO_CMD_ADDR_SHIFT;
    argument |= value;
    if (write) {
        argument |= SDIO_CMD52_WRITE;
    }
    if (read_after_write) {
        argument |= SDIO_CMD52_RAW;
    }
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 0, 0, argument, 0,
                   (SDIO_CMD_IO_RW_DIRECT << 8) | SDHC_CMD_RESPONSE);
    return qtest_readl(qts, RASPI4_EMMC1_BASE + SDHC_RSPREG0);
}

static uint32_t cyw43455_sdio_cmd52(bool write, bool read_after_write,
                                    unsigned int function, uint32_t address,
                                    uint8_t value)
{
    return cyw43455_sdio_cmd52_qtest(global_qtest, write,
                                     read_after_write, function, address,
                                     value);
}

static void cyw43455_sdio_select_qtest(QTestState *qts)
{
    qtest_writeb(qts, RASPI4_EMMC1_BASE + SDHC_SWRST, SDHC_RESET_ALL);
    qtest_writew(qts, RASPI4_EMMC1_BASE + SDHC_CLKCON,
                 SDHC_CLOCK_SDCLK_EN | SDHC_CLOCK_INT_STABLE |
                 SDHC_CLOCK_INT_EN);

    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 0, 0, 0, 0,
                   (SDIO_CMD_IO_SEND_OP_COND << 8) | SDHC_CMD_RESPONSE);
    g_assert_cmphex(qtest_readl(qts, RASPI4_EMMC1_BASE + SDHC_RSPREG0),
                    ==, CYW43455_SDIO_OCR_RESPONSE);

    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 0, 0, 0, 0,
                   SDHC_SEND_RELATIVE_ADDR | SDHC_CMD_RESPONSE);
    g_assert_cmphex(qtest_readl(qts, RASPI4_EMMC1_BASE + SDHC_RSPREG0),
                    ==, CYW43455_SDIO_RCA << 16);
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 0, 0,
                   CYW43455_SDIO_RCA << 16, 0,
                   SDHC_SELECT_DESELECT_CARD | SDHC_CMD_RESPONSE);
}

static void cyw43455_sdio_set_backplane_window_qtest(QTestState *qts,
                                                       uint32_t address)
{
    uint32_t window = address & ~CYW43455_SDIO_SB_OFFSET;

    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(qts, true, true, 1,
                                              CYW43455_SDIO_SBADDRLOW,
                                              window >> 8) & 0xff,
                    ==, (window >> 8) & 0xff);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(qts, true, true, 1,
                                              CYW43455_SDIO_SBADDRMID,
                                              window >> 16) & 0xff,
                    ==, (window >> 16) & 0xff);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(qts, true, true, 1,
                                              CYW43455_SDIO_SBADDRHIGH,
                                              window >> 24) & 0xff,
                    ==, window >> 24);
}

static uint32_t cyw43455_sdio_backplane_readl_qtest(QTestState *qts,
                                                     uint32_t address)
{
    uint32_t argument;
    uint32_t sdio_address;

    cyw43455_sdio_set_backplane_window_qtest(qts, address);
    sdio_address = CYW43455_SDIO_SB_ACCESS |
                   (address & CYW43455_SDIO_SB_OFFSET);
    argument = (1U << SDIO_CMD_FUNC_SHIFT) | SDIO_CMD53_INCREMENT |
               (sdio_address << SDIO_CMD_ADDR_SHIFT) | 4;
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 4, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN | SDHC_TRNS_READ,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    return qtest_readl(qts, RASPI4_EMMC1_BASE + SDHC_BDATA);
}

static void cyw43455_sdio_backplane_writel_qtest(QTestState *qts,
                                                  uint32_t address,
                                                  uint32_t value)
{
    uint32_t argument;
    uint32_t sdio_address;

    cyw43455_sdio_set_backplane_window_qtest(qts, address);
    sdio_address = CYW43455_SDIO_SB_ACCESS |
                   (address & CYW43455_SDIO_SB_OFFSET);
    argument = SDIO_CMD53_WRITE | (1U << SDIO_CMD_FUNC_SHIFT) |
               SDIO_CMD53_INCREMENT |
               (sdio_address << SDIO_CMD_ADDR_SHIFT) | 4;
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, 4, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    qtest_writel(qts, RASPI4_EMMC1_BASE + SDHC_BDATA, value);
}

static void cyw43455_sdio_function2_write_qtest(QTestState *qts,
                                                 const uint8_t *data,
                                                 size_t length)
{
    uint32_t argument;
    size_t i;

    g_assert_cmpuint(length, >, 0);
    g_assert_cmpuint(length, <=, 511);
    g_assert_cmpuint(length % sizeof(uint32_t), ==, 0);
    argument = SDIO_CMD53_WRITE | (2U << SDIO_CMD_FUNC_SHIFT) | length;
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, length, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    for (i = 0; i < length; i += sizeof(uint32_t)) {
        qtest_writel(qts, RASPI4_EMMC1_BASE + SDHC_BDATA,
                     ldl_le_p(data + i));
    }
}

static void cyw43455_sdio_function2_read_qtest(QTestState *qts,
                                                uint8_t *data, size_t length)
{
    uint32_t argument;
    size_t i;

    g_assert_cmpuint(length, >, 0);
    g_assert_cmpuint(length, <=, 511);
    g_assert_cmpuint(length % sizeof(uint32_t), ==, 0);
    argument = (2U << SDIO_CMD_FUNC_SHIFT) | length;
    sdhci_cmd_regs(qts, RASPI4_EMMC1_BASE, length, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN | SDHC_TRNS_READ,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    for (i = 0; i < length; i += sizeof(uint32_t)) {
        stl_le_p(data + i,
                 qtest_readl(qts, RASPI4_EMMC1_BASE + SDHC_BDATA));
    }
}

static size_t cyw43455_sdio_build_iovar_request(uint8_t *frame,
                                                size_t frame_size,
                                                const char *iovar,
                                                uint8_t sequence,
                                                uint16_t request_id)
{
    const uint32_t payload_len = 20;
    const uint32_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                               CYW43455_BCDC_HEADER_LEN + payload_len;

    g_assert_cmpuint(frame_size, >=, frame_len);
    memset(frame, 0, frame_len);
    stw_le_p(frame, frame_len);
    stw_le_p(frame + 2, (uint16_t)~frame_len);
    frame[4] = sequence;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN, CYW43455_C_GET_VAR);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4, payload_len);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8,
             (uint32_t)request_id << 16);
    g_assert_cmpuint(strlen(iovar) + 1, <=, payload_len);
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN,
           iovar, strlen(iovar) + 1);
    return frame_len;
}

static size_t cyw43455_sdio_build_iovar_data_request(
    uint8_t *frame, size_t frame_size, const char *iovar,
    const void *data, size_t data_len, size_t payload_len,
    uint8_t sequence, uint16_t request_id)
{
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_HEADER_LEN + payload_len;
    size_t stored_len = QEMU_ALIGN_UP(frame_len, sizeof(uint32_t));

    g_assert_cmpuint(strlen(iovar) + 1 + data_len, <=, payload_len);
    g_assert_cmpuint(payload_len, <=, UINT32_MAX);
    g_assert_cmpuint(frame_size, >=, stored_len);
    memset(frame, 0, stored_len);
    stw_le_p(frame, frame_len);
    stw_le_p(frame + 2, (uint16_t)~frame_len);
    frame[4] = sequence;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN, CYW43455_C_GET_VAR);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4, payload_len);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8,
             (uint32_t)request_id << 16);
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN,
           iovar, strlen(iovar) + 1);
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN +
                   strlen(iovar) + 1,
           data, data_len);
    return stored_len;
}

static size_t cyw43455_sdio_build_set_iovar_request(
    uint8_t *frame, size_t frame_size, const char *iovar,
    const void *data, size_t data_len, uint8_t sequence,
    uint16_t request_id)
{
    uint32_t payload_len = strlen(iovar) + 1 + data_len;
    uint32_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                         CYW43455_BCDC_HEADER_LEN + payload_len;
    uint32_t stored_len = QEMU_ALIGN_UP(frame_len, sizeof(uint32_t));

    g_assert_cmpuint(frame_size, >=, stored_len);
    g_assert_cmpuint(data_len, <=, UINT32_MAX - strlen(iovar) - 1);
    memset(frame, 0, stored_len);
    stw_le_p(frame, stored_len);
    stw_le_p(frame + 2, (uint16_t)~stored_len);
    frame[4] = sequence;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN, CYW43455_C_SET_VAR);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4, payload_len);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8,
             ((uint32_t)request_id << 16) | CYW43455_BCDC_DCMD_SET);
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN,
           iovar, strlen(iovar) + 1);
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN +
                   strlen(iovar) + 1,
           data, data_len);
    return stored_len;
}

static size_t cyw43455_sdio_build_escan_request(
    uint8_t *frame, size_t frame_size, uint32_t version, uint16_t sync_id,
    uint16_t channel, uint8_t sequence, uint16_t request_id)
{
    uint8_t escan[CYW43455_ESCAN_V2_REQUEST_LEN] = { };
    uint8_t *params = escan + CYW43455_ESCAN_HEADER_LEN;
    uint8_t *ssid;
    const size_t ssid_len = strlen(CYW43455_BSS_SSID);
    size_t params_fixed_len;
    size_t escan_fixed_len;
    size_t escan_len;
    size_t bssid_offset;
    size_t bss_type_offset;
    size_t nprobes_offset;
    size_t channel_num_offset;

    switch (version) {
    case CYW43455_ESCAN_REQ_VERSION_V1:
        params_fixed_len = CYW43455_SCAN_PARAMS_V1_FIXED_LEN;
        bssid_offset = 36;
        bss_type_offset = 42;
        params[43] = 0; /* active scan */
        nprobes_offset = 44;
        channel_num_offset = 60;
        break;
    case CYW43455_ESCAN_REQ_VERSION_V2:
        params_fixed_len = CYW43455_SCAN_PARAMS_V2_FIXED_LEN;
        bssid_offset = 40;
        bss_type_offset = 46;
        stl_le_p(params + 48, 0); /* active scan */
        nprobes_offset = 52;
        channel_num_offset = 68;
        break;
    default:
        g_assert_not_reached();
    }

    escan_fixed_len = CYW43455_ESCAN_HEADER_LEN + params_fixed_len;
    escan_len = escan_fixed_len + sizeof(uint32_t) +
                CYW43455_SCAN_SSID_LEN;
    ssid = escan + escan_fixed_len + sizeof(uint32_t);

    stl_le_p(escan, version);
    stw_le_p(escan + 4, CYW43455_ESCAN_ACTION_START);
    stw_le_p(escan + 6, sync_id);
    if (version == CYW43455_ESCAN_REQ_VERSION_V2) {
        stw_le_p(params, CYW43455_ESCAN_REQ_VERSION_V2);
        stw_le_p(params + 2, escan_len - CYW43455_ESCAN_HEADER_LEN);
    }
    memset(params + bssid_offset, 0xff, CYW43455_MAC_LEN);
    params[bss_type_offset] = 2; /* any BSS type */
    stl_le_p(params + nprobes_offset, UINT32_MAX);
    stl_le_p(params + nprobes_offset + 4, UINT32_MAX);
    stl_le_p(params + nprobes_offset + 8, UINT32_MAX);
    stl_le_p(params + nprobes_offset + 12, UINT32_MAX);
    stl_le_p(params + channel_num_offset, (1U << 16) | 1);
    stw_le_p(escan + escan_fixed_len, channel);
    stl_le_p(ssid, ssid_len);
    memcpy(ssid + sizeof(uint32_t), CYW43455_BSS_SSID, ssid_len);

    return cyw43455_sdio_build_set_iovar_request(
        frame, frame_size, "escan", escan, escan_len, sequence,
        request_id);
}

static size_t cyw43455_sdio_build_cur_etheraddr_request(uint8_t *frame,
                                                        size_t frame_size)
{
    return cyw43455_sdio_build_iovar_request(frame, frame_size,
                                              "cur_etheraddr", 1, 1);
}

static size_t cyw43455_sdio_build_command_request(uint8_t *frame,
                                                   size_t frame_size,
                                                   uint32_t command,
                                                   uint32_t value,
                                                   uint8_t sequence,
                                                   uint16_t request_id)
{
    const uint32_t payload_len = sizeof(value);
    const uint32_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                               CYW43455_BCDC_HEADER_LEN + payload_len;

    g_assert_cmpuint(frame_size, >=, frame_len);
    memset(frame, 0, frame_len);
    stw_le_p(frame, frame_len);
    stw_le_p(frame + 2, (uint16_t)~frame_len);
    frame[4] = sequence;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN, command);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4, payload_len);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8,
             ((uint32_t)request_id << 16) | CYW43455_BCDC_DCMD_SET);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + CYW43455_BCDC_HEADER_LEN,
             value);
    return frame_len;
}

static size_t cyw43455_sdio_build_query_command_request(
    uint8_t *frame, size_t frame_size, uint32_t command,
    size_t response_payload_len, uint8_t sequence, uint16_t request_id)
{
    const size_t payload_len = sizeof(uint32_t);
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_HEADER_LEN + payload_len;
    size_t stored_len = QEMU_ALIGN_UP(frame_len, sizeof(uint32_t));

    g_assert_cmpuint(response_payload_len, >=, payload_len);
    g_assert_cmpuint(response_payload_len, <=, UINT32_MAX);
    g_assert_cmpuint(frame_len, <=, UINT16_MAX);
    g_assert_cmpuint(frame_size, >=, stored_len);
    memset(frame, 0, stored_len);
    stw_le_p(frame, frame_len);
    stw_le_p(frame + 2, (uint16_t)~frame_len);
    frame[4] = sequence;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN, command);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4,
             response_payload_len);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8,
             (uint32_t)request_id << 16);
    stl_le_p(frame + CYW43455_SDPCM_HEADER_LEN +
             CYW43455_BCDC_HEADER_LEN, response_payload_len);
    return stored_len;
}

static size_t cyw43455_sdio_build_data_request(uint8_t *frame,
                                               size_t frame_size,
                                               const uint8_t *packet,
                                               size_t packet_len)
{
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_DATA_HEADER_LEN + packet_len;

    g_assert_cmpuint(frame_size, >=, frame_len);
    g_assert_cmpuint(frame_len, <=, UINT16_MAX);
    memset(frame, 0, frame_len);
    stw_le_p(frame, frame_len);
    stw_le_p(frame + 2, (uint16_t)~frame_len);
    frame[4] = 1;
    frame[5] = CYW43455_SDPCM_DATA_CHANNEL;
    frame[7] = CYW43455_SDPCM_HEADER_LEN;
    frame[CYW43455_SDPCM_HEADER_LEN] = 2 << 4; /* BCDC protocol version. */
    memcpy(frame + CYW43455_SDPCM_HEADER_LEN +
                   CYW43455_BCDC_DATA_HEADER_LEN,
           packet, packet_len);
    return frame_len;
}

static size_t cyw43455_sdio_build_two_data_glom(uint8_t *frame,
                                                size_t frame_size,
                                                const uint8_t *packet,
                                                size_t packet_len)
{
    const size_t header_len = CYW43455_SDPCM_HW_HEADER_LEN +
                              CYW43455_SDPCM_HWEXT_LEN + 8;
    const size_t subframe_len = header_len +
                                CYW43455_BCDC_DATA_HEADER_LEN + packet_len;
    const size_t total_len = 2 * subframe_len;
    size_t offset;
    unsigned int i;

    g_assert_cmpuint(frame_size, >=, total_len);
    g_assert_cmpuint(total_len, <=, UINT16_MAX);
    g_assert_cmpuint(subframe_len % sizeof(uint32_t), ==, 0);
    memset(frame, 0, total_len);

    for (i = 0, offset = 0; i < 2; i++, offset += subframe_len) {
        uint32_t extension = subframe_len -
                             CYW43455_SDPCM_HW_HEADER_LEN;

        if (i == 1) {
            extension |= CYW43455_SDPCM_HWEXT_LAST;
        }
        stw_le_p(frame + offset, subframe_len);
        stw_le_p(frame + offset + 2, (uint16_t)~subframe_len);
        stl_le_p(frame + offset + CYW43455_SDPCM_HW_HEADER_LEN,
                 extension);
        frame[offset + CYW43455_SDPCM_HW_HEADER_LEN +
              CYW43455_SDPCM_HWEXT_LEN] = i + 1;
        frame[offset + CYW43455_SDPCM_HW_HEADER_LEN +
              CYW43455_SDPCM_HWEXT_LEN + 1] =
            CYW43455_SDPCM_DATA_CHANNEL;
        frame[offset + CYW43455_SDPCM_HW_HEADER_LEN +
              CYW43455_SDPCM_HWEXT_LEN + 3] = header_len;
        frame[offset + header_len] = 2 << 4;
        memcpy(frame + offset + header_len +
                       CYW43455_BCDC_DATA_HEADER_LEN,
               packet, packet_len);
    }

    /* A transmit glom replaces the first hardware length with chain length. */
    stw_le_p(frame, total_len);
    stw_le_p(frame + 2, (uint16_t)~total_len);
    return total_len;
}

static void cyw43455_sdio_enable_control_irq_qtest(QTestState *qts)
{
    uint16_t interrupt_enable;

    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(
                        qts, true, true, 0, CYW43455_SDIO_CCCR_INTEN,
                        BIT(0) | BIT(1)) & 0xff,
                    ==, BIT(0) | BIT(1));
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_HOSTINTMASK, CYW43455_SDIO_FRAME_IND);
    interrupt_enable = qtest_readw(qts, RASPI4_EMMC1_BASE +
                                        SDHC_NORINTSTSEN);
    qtest_writew(qts, RASPI4_EMMC1_BASE + SDHC_NORINTSTSEN,
                 interrupt_enable | SDHC_NIS_CARDINT);
    interrupt_enable = qtest_readw(qts, RASPI4_EMMC1_BASE +
                                        SDHC_NORINTSIGEN);
    qtest_writew(qts, RASPI4_EMMC1_BASE + SDHC_NORINTSIGEN,
                 interrupt_enable | SDHC_NIS_CARDINT);
}

static const uint8_t cyw43455_sdio_default_test_mac[] = {
    0x02, 0, 0, 0, 0, 1,
};

static void cyw43455_sdio_assert_cur_etheraddr_response(
    const uint8_t *frame, size_t frame_len, const uint8_t *expected_mac,
    uint16_t request_id)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;

    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==,
                    (uint16_t)~frame_len);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     20);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);
    g_assert_cmpmem(payload, CYW43455_MAC_LEN,
                    expected_mac, CYW43455_MAC_LEN);
}

static void cyw43455_sdio_assert_set_iovar_status_response(
    const uint8_t *frame, size_t stored_len, const char *iovar,
    size_t data_len, uint8_t sequence, uint16_t request_id, int32_t status)
{
    uint32_t payload_len = strlen(iovar) + 1 + data_len;
    uint32_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                         CYW43455_BCDC_HEADER_LEN + payload_len;
    uint32_t flags = ((uint32_t)request_id << 16) |
                     CYW43455_BCDC_DCMD_SET;

    if (status < 0) {
        flags |= CYW43455_BCDC_DCMD_ERROR;
    }

    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_SET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     payload_len);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    flags);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==,
                    status);
}

static void cyw43455_sdio_assert_set_iovar_response(
    const uint8_t *frame, size_t stored_len, const char *iovar,
    size_t data_len, uint8_t sequence, uint16_t request_id)
{
    cyw43455_sdio_assert_set_iovar_status_response(
        frame, stored_len, iovar, data_len, sequence, request_id, 0);
}

static void cyw43455_sdio_assert_zero_iovar_response(
    const uint8_t *frame, size_t frame_len, uint8_t sequence,
    uint16_t request_id)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;

    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     20);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);
    for (unsigned int i = 0; i < 20; i++) {
        g_assert_cmphex(payload[i], ==, 0);
    }
}

static void cyw43455_sdio_assert_iovar_value_response(
    const uint8_t *frame, size_t frame_len, uint8_t sequence,
    uint16_t request_id, const uint8_t *expected, size_t expected_len)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;

    g_assert_cmpuint(expected_len, <=, 20);
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     20);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);
    g_assert_cmpmem(payload, expected_len, expected, expected_len);
    for (unsigned int i = expected_len; i < 20; i++) {
        g_assert_cmphex(payload[i], ==, 0);
    }
}

static const uint8_t *cyw43455_sdio_assert_iovar_data_response(
    const uint8_t *frame, size_t stored_len, size_t payload_len,
    uint8_t sequence, uint16_t request_id, int32_t status)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_HEADER_LEN + payload_len;
    uint32_t flags = (uint32_t)request_id << 16;

    if (status < 0) {
        flags |= CYW43455_BCDC_DCMD_ERROR;
    }
    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     payload_len);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    flags);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==,
                    status);
    return payload;
}

static void cyw43455_sdio_assert_station_info_response(
    const uint8_t *frame, size_t stored_len, uint16_t request_id,
    const uint8_t *peer_mac, uint32_t tx_packets, uint64_t tx_bytes)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_HEADER_LEN +
                       CYW43455_STA_PAYLOAD_LEN;
    uint32_t rx_packets;
    uint64_t rx_bytes;

    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_VAR);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     CYW43455_STA_PAYLOAD_LEN);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);

    g_assert_cmpuint(lduw_le_p(payload), ==, CYW43455_STA_INFO_VERSION);
    g_assert_cmpuint(lduw_le_p(payload + 2), ==, CYW43455_STA_INFO_LEN);
    g_assert_cmphex(ldl_le_p(payload + CYW43455_STA_INFO_FLAGS_OFFSET), ==,
                    CYW43455_STA_FLAGS);
    g_assert_cmpmem(payload + CYW43455_STA_INFO_EA_OFFSET,
                    CYW43455_MAC_LEN, peer_mac, CYW43455_MAC_LEN);
    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_COUNT_OFFSET), ==,
                     1);
    g_assert_cmpuint(payload[CYW43455_STA_INFO_RATES_OFFSET], ==, 144);
    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_TX_RATE_OFFSET),
                     ==, CYW43455_LINK_RATE_KBPS);
    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_RX_RATE_OFFSET),
                     ==, CYW43455_LINK_RATE_KBPS);
    g_assert_cmpint((int8_t)payload[CYW43455_STA_INFO_RSSI_OFFSET], ==,
                    CYW43455_SIGNAL_DBM);
    g_assert_cmpint((int8_t)payload[CYW43455_STA_INFO_NF_OFFSET], ==,
                    CYW43455_NOISE_DBM);
    g_assert_cmpint((int8_t)payload[
                        CYW43455_STA_INFO_RX_LAST_RSSI_OFFSET], ==,
                    CYW43455_SIGNAL_DBM);

    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_TX_TOTAL_OFFSET),
                     ==, tx_packets);
    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_TX_PKTS_OFFSET) +
                     ldl_le_p(payload +
                              CYW43455_STA_INFO_TX_MCAST_OFFSET), ==,
                     tx_packets);
    g_assert_cmpuint(ldl_le_p(payload + CYW43455_STA_INFO_TX_FAIL_OFFSET),
                     ==, 0);
    g_assert_cmpuint(ldq_le_p(payload + CYW43455_STA_INFO_TX_BYTES_OFFSET),
                     ==, tx_bytes);
    g_assert_cmpuint(ldq_le_p(payload +
                             CYW43455_STA_INFO_TX_UCAST_BYTES_OFFSET) +
                     ldq_le_p(payload +
                             CYW43455_STA_INFO_TX_MCAST_BYTES_OFFSET), ==,
                     tx_bytes);

    rx_packets = ldl_le_p(payload + CYW43455_STA_INFO_RX_TOTAL_OFFSET);
    rx_bytes = ldq_le_p(payload + CYW43455_STA_INFO_RX_BYTES_OFFSET);
    g_assert_cmpuint(rx_packets, >, 0);
    g_assert_cmpuint(rx_bytes, >, 0);
    g_assert_cmpuint(ldl_le_p(payload +
                             CYW43455_STA_INFO_RX_UCAST_OFFSET) +
                     ldl_le_p(payload +
                             CYW43455_STA_INFO_RX_MCAST_OFFSET), ==,
                     rx_packets);
    g_assert_cmpuint(ldq_le_p(payload +
                             CYW43455_STA_INFO_RX_UCAST_BYTES_OFFSET) +
                     ldq_le_p(payload +
                             CYW43455_STA_INFO_RX_MCAST_BYTES_OFFSET), ==,
                     rx_bytes);
}

static void cyw43455_sdio_assert_bss_info_response(
    const uint8_t *frame, size_t stored_len, uint16_t request_id,
    const uint8_t *peer_mac, uint16_t chanspec, uint8_t control_channel)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;
    const uint8_t *bss = payload + CYW43455_BSS_INFO_PREFIX_LEN;
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_HEADER_LEN +
                       CYW43455_BSS_PAYLOAD_LEN;
    const size_t ssid_len = strlen(CYW43455_BSS_SSID);

    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     CYW43455_C_GET_BSS_INFO);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     CYW43455_BSS_PAYLOAD_LEN);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);

    g_assert_cmpuint(ldl_le_p(payload), ==, CYW43455_BSS_PAYLOAD_LEN);
    g_assert_cmpuint(ldl_le_p(bss), ==, CYW43455_BSS_INFO_VERSION);
    g_assert_cmpuint(ldl_le_p(bss + 4), ==, CYW43455_BSS_INFO_LEN);
    g_assert_cmpmem(bss + CYW43455_BSS_INFO_BSSID_OFFSET,
                    CYW43455_MAC_LEN, peer_mac, CYW43455_MAC_LEN);
    g_assert_cmpuint(lduw_le_p(bss + CYW43455_BSS_INFO_BEACON_OFFSET), ==,
                     CYW43455_BSS_BEACON_PERIOD);
    g_assert_cmphex(lduw_le_p(bss + CYW43455_BSS_INFO_CAP_OFFSET), ==,
                    CYW43455_BSS_CAPABILITIES);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_SSID_LEN_OFFSET], ==, ssid_len);
    g_assert_cmpmem(bss + CYW43455_BSS_INFO_SSID_OFFSET, ssid_len,
                    CYW43455_BSS_SSID, ssid_len);
    g_assert_cmpuint(ldl_le_p(bss + CYW43455_BSS_INFO_RATE_COUNT_OFFSET), ==,
                     1);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_RATES_OFFSET], ==, 144);
    g_assert_cmphex(lduw_le_p(bss + CYW43455_BSS_INFO_CHANSPEC_OFFSET), ==,
                    chanspec);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_DTIM_OFFSET], ==,
                     CYW43455_BSS_DTIM_PERIOD);
    g_assert_cmpint((int16_t)lduw_le_p(
                        bss + CYW43455_BSS_INFO_RSSI_OFFSET), ==,
                    CYW43455_SIGNAL_DBM);
    g_assert_cmpint((int8_t)bss[CYW43455_BSS_INFO_NOISE_OFFSET], ==,
                    CYW43455_NOISE_DBM);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_N_CAP_OFFSET], ==, 1);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_CTL_CH_OFFSET], ==,
                     control_channel);
    g_assert_cmpuint(lduw_le_p(bss + CYW43455_BSS_INFO_IE_OFFSET), ==,
                     CYW43455_BSS_INFO_LEN);
    g_assert_cmpuint(ldl_le_p(bss + CYW43455_BSS_INFO_IE_LEN_OFFSET), ==, 0);
    g_assert_cmpuint(lduw_le_p(bss + CYW43455_BSS_INFO_SNR_OFFSET), ==,
                     CYW43455_SIGNAL_DBM - CYW43455_NOISE_DBM);
    for (size_t i = CYW43455_BSS_INFO_PREFIX_LEN +
                    CYW43455_BSS_INFO_LEN;
         i < CYW43455_BSS_PAYLOAD_LEN; i++) {
        g_assert_cmphex(payload[i], ==, 0);
    }
}

static void cyw43455_sdio_assert_command_status_response(
    const uint8_t *frame, size_t frame_len, uint32_t command,
    uint8_t sequence, uint16_t request_id, int32_t status)
{
    uint32_t flags = ((uint32_t)request_id << 16) |
                     CYW43455_BCDC_DCMD_SET;

    if (status < 0) {
        flags |= CYW43455_BCDC_DCMD_ERROR;
    }
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     command);
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    flags);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==,
                    status);
}

static void cyw43455_sdio_assert_command_response(const uint8_t *frame,
                                                   size_t frame_len,
                                                   uint32_t command,
                                                   uint8_t sequence,
                                                   uint16_t request_id)
{
    cyw43455_sdio_assert_command_status_response(
        frame, frame_len, command, sequence, request_id, 0);
}

static void cyw43455_sdio_assert_query_command_value_response(
    const uint8_t *frame, size_t frame_len, uint32_t command,
    uint32_t expected, uint8_t sequence, uint16_t request_id)
{
    const uint8_t *payload = frame + CYW43455_SDPCM_HEADER_LEN +
                             CYW43455_BCDC_HEADER_LEN;

    g_assert_cmpuint(frame_len, ==, CYW43455_SDPCM_HEADER_LEN +
                                  CYW43455_BCDC_HEADER_LEN +
                                  sizeof(uint32_t));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, 0);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN), ==,
                     command);
    g_assert_cmpuint(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 4), ==,
                     sizeof(uint32_t));
    g_assert_cmphex(ldl_le_p(frame + CYW43455_SDPCM_HEADER_LEN + 8), ==,
                    (uint32_t)request_id << 16);
    g_assert_cmpint((int32_t)ldl_le_p(frame +
                                     CYW43455_SDPCM_HEADER_LEN + 12), ==, 0);
    g_assert_cmpuint((uint32_t)ldl_le_p(payload), ==, expected);
}

static void cyw43455_sdio_assert_link_event(const uint8_t *frame,
                                             size_t stored_len,
                                             uint8_t sequence,
                                             uint8_t tx_max, bool link_up,
                                             const uint8_t *guest_mac)
{
    static const uint8_t peer_mac[CYW43455_MAC_LEN] = {
        0x52, 0x55, 0x0a, 0x00, 0x02, 0x02,
    };
    static const uint8_t oui[] = { 0x00, 0x10, 0x18 };
    static const char ifname[16] = "wlan0";
    const uint8_t *bcdc = frame + CYW43455_SDPCM_HEADER_LEN;
    const uint8_t *ethernet = bcdc + CYW43455_BCDC_DATA_HEADER_LEN;
    const uint8_t *brcm = ethernet + CYW43455_ETH_HEADER_LEN;
    const uint8_t *event = brcm + CYW43455_BRCM_EVENT_HEADER_LEN;

    g_assert_cmpuint(stored_len, ==, CYW43455_EVENT_FRAME_LEN);
    g_assert_cmpuint(lduw_le_p(frame), ==, CYW43455_EVENT_FRAME_LEN);
    g_assert_cmphex(lduw_le_p(frame + 2), ==,
                    (uint16_t)~CYW43455_EVENT_FRAME_LEN);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, CYW43455_SDPCM_EVENT_CHANNEL);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(frame[9], ==, tx_max);
    g_assert_cmphex(bcdc[0], ==, 2 << 4);
    g_assert_cmphex(bcdc[1], ==, 0);
    g_assert_cmphex(bcdc[2], ==, 0);
    g_assert_cmphex(bcdc[3], ==, 0);
    g_assert_cmpmem(ethernet, CYW43455_MAC_LEN,
                    guest_mac, CYW43455_MAC_LEN);
    g_assert_cmpmem(ethernet + CYW43455_MAC_LEN, CYW43455_MAC_LEN,
                    peer_mac, CYW43455_MAC_LEN);
    g_assert_cmphex(lduw_be_p(ethernet + 2 * CYW43455_MAC_LEN), ==, 0x886c);
    g_assert_cmphex(lduw_be_p(brcm), ==, 0x8001);
    g_assert_cmpuint(lduw_be_p(brcm + 2), ==, CYW43455_EVENT_MSG_LEN);
    g_assert_cmphex(brcm[4], ==, 0);
    g_assert_cmpmem(brcm + 5, sizeof(oui), oui, sizeof(oui));
    g_assert_cmpuint(lduw_be_p(brcm + 8), ==, 1);
    g_assert_cmpuint(lduw_be_p(event), ==, 2);
    g_assert_cmphex(lduw_be_p(event + 2), ==, link_up ? 1 : 0);
    g_assert_cmpuint(ldl_be_p(event + 4), ==, link_up ? 0 : 16);
    g_assert_cmpuint(ldl_be_p(event + 8), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 12), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 16), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 20), ==, 0);
    g_assert_cmpmem(event + 24, CYW43455_MAC_LEN,
                    peer_mac, CYW43455_MAC_LEN);
    g_assert_cmpmem(event + 30, sizeof(ifname), ifname, sizeof(ifname));
    g_assert_cmphex(event[46], ==, 0);
    g_assert_cmphex(event[47], ==, 0);
}

static void cyw43455_sdio_assert_escan_event(
    const uint8_t *frame, size_t stored_len, uint8_t sequence,
    uint8_t tx_max, uint16_t sync_id, bool partial,
    const uint8_t *guest_mac, uint16_t chanspec, uint8_t control_channel)
{
    static const uint8_t peer_mac[CYW43455_MAC_LEN] = {
        0x52, 0x55, 0x0a, 0x00, 0x02, 0x02,
    };
    uint8_t expected_ies[CYW43455_BSS_INFO_IE_BYTES] = {
        0, 13,
        'q', 'e', 'm', 'u', '-', 'c', 'y', 'w', '4', '3', '4', '5', '5',
        1, 4, 0x82, 0x84, 0x8b, 0x96,
        3, 1, 0,
    };
    const uint8_t *bcdc = frame + CYW43455_SDPCM_HEADER_LEN;
    const uint8_t *ethernet = bcdc + CYW43455_BCDC_DATA_HEADER_LEN;
    const uint8_t *brcm = ethernet + CYW43455_ETH_HEADER_LEN;
    const uint8_t *event = brcm + CYW43455_BRCM_EVENT_HEADER_LEN;
    const uint8_t *result = event + CYW43455_EVENT_MSG_LEN;
    const uint8_t *bss = result + CYW43455_ESCAN_RESULT_FIXED_LEN;
    size_t data_len = partial ? CYW43455_ESCAN_RESULT_LEN : 0;
    size_t frame_len = CYW43455_EVENT_FRAME_LEN + data_len;
    size_t ssid_len = strlen(CYW43455_BSS_SSID);

    expected_ies[sizeof(expected_ies) - 1] = control_channel;

    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, CYW43455_SDPCM_EVENT_CHANNEL);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(frame[9], ==, tx_max);
    g_assert_cmphex(bcdc[0], ==, 2 << 4);
    g_assert_cmpmem(ethernet, CYW43455_MAC_LEN,
                    guest_mac, CYW43455_MAC_LEN);
    g_assert_cmpmem(ethernet + CYW43455_MAC_LEN, CYW43455_MAC_LEN,
                    peer_mac, CYW43455_MAC_LEN);
    g_assert_cmphex(lduw_be_p(ethernet + 2 * CYW43455_MAC_LEN), ==, 0x886c);
    g_assert_cmphex(lduw_be_p(brcm), ==, 0x8001);
    g_assert_cmpuint(lduw_be_p(brcm + 2), ==,
                     CYW43455_EVENT_MSG_LEN + data_len);
    g_assert_cmpuint(lduw_be_p(event), ==, 2);
    g_assert_cmphex(lduw_be_p(event + 2), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 4), ==,
                     CYW43455_EVENT_ESCAN_RESULT);
    g_assert_cmpuint(ldl_be_p(event + 8), ==,
                     partial ? CYW43455_EVENT_STATUS_PARTIAL :
                               CYW43455_EVENT_STATUS_SUCCESS);
    g_assert_cmpuint(ldl_be_p(event + 12), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 16), ==, 0);
    g_assert_cmpuint(ldl_be_p(event + 20), ==, data_len);
    g_assert_cmpmem(event + 24, CYW43455_MAC_LEN,
                    peer_mac, CYW43455_MAC_LEN);

    if (!partial) {
        return;
    }

    g_assert_cmpuint(ldl_le_p(result), ==, CYW43455_ESCAN_RESULT_LEN);
    g_assert_cmpuint(ldl_le_p(result + 4), ==,
                     CYW43455_ESCAN_RESULT_VERSION);
    g_assert_cmphex(lduw_le_p(result + 8), ==, sync_id);
    g_assert_cmpuint(lduw_le_p(result + 10), ==, 1);
    g_assert_cmpuint(ldl_le_p(bss), ==, CYW43455_BSS_INFO_VERSION);
    g_assert_cmpuint(ldl_le_p(bss + 4), ==,
                     CYW43455_ESCAN_BSS_INFO_LEN);
    g_assert_cmpmem(bss + CYW43455_BSS_INFO_BSSID_OFFSET,
                    CYW43455_MAC_LEN, peer_mac, CYW43455_MAC_LEN);
    g_assert_cmpuint(lduw_le_p(bss + CYW43455_BSS_INFO_BEACON_OFFSET), ==,
                     CYW43455_BSS_BEACON_PERIOD);
    g_assert_cmphex(lduw_le_p(bss + CYW43455_BSS_INFO_CAP_OFFSET), ==,
                    CYW43455_BSS_CAPABILITIES);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_SSID_LEN_OFFSET], ==, ssid_len);
    g_assert_cmpmem(bss + CYW43455_BSS_INFO_SSID_OFFSET, ssid_len,
                    CYW43455_BSS_SSID, ssid_len);
    g_assert_cmphex(lduw_le_p(bss + CYW43455_BSS_INFO_CHANSPEC_OFFSET), ==,
                    chanspec);
    g_assert_cmpuint(bss[CYW43455_BSS_INFO_CTL_CH_OFFSET], ==,
                     control_channel);
    g_assert_cmpint((int16_t)lduw_le_p(
                        bss + CYW43455_BSS_INFO_RSSI_OFFSET), ==,
                    CYW43455_SIGNAL_DBM);
    g_assert_cmpuint(lduw_le_p(bss + CYW43455_BSS_INFO_IE_OFFSET), ==,
                     CYW43455_BSS_INFO_LEN);
    g_assert_cmpuint(ldl_le_p(bss + CYW43455_BSS_INFO_IE_LEN_OFFSET), ==,
                     sizeof(expected_ies));
    g_assert_cmpmem(bss + CYW43455_BSS_INFO_LEN, sizeof(expected_ies),
                    expected_ies, sizeof(expected_ies));
}

static void cyw43455_sdio_assert_credit_update(const uint8_t *frame,
                                                size_t frame_len,
                                                uint8_t tx_sequence,
                                                uint8_t tx_window)
{
    g_assert_cmpuint(frame_len, ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[5] & 0xf, ==, CYW43455_SDPCM_DATA_CHANNEL);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(frame[9], ==, (uint8_t)(tx_sequence + tx_window));
}

static void test_cyw43455_sdio_endpoint(void)
{
    const char *bus_path = "/machine/soc/peripherals/sdhci/sd-bus";
    static const uint8_t packet[16] = { 0 };
    uint32_t argument;
    uint32_t response;
    uint32_t cis;
    uint8_t control_frame[48];
    uint8_t credit_frame[CYW43455_SDPCM_HEADER_LEN];
    uint8_t data_frame[CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_DATA_HEADER_LEN + sizeof(packet)];
    size_t control_len;
    size_t data_len;

    g_assert_true(qom_bus_has_device_type(bus_path, "cyw43455-sdio"));

    cyw43455_sdio_select_qtest(global_qtest);

    response = cyw43455_sdio_cmd52(false, false, 0,
                                   CYW43455_SDIO_CCCR_REV, 0);
    g_assert_cmphex(response & 0xff, ==, 0x32);

    cis = cyw43455_sdio_cmd52(false, false, 0,
                              CYW43455_SDIO_CCCR_CIS, 0) & 0xff;
    cis |= (cyw43455_sdio_cmd52(false, false, 0,
                                CYW43455_SDIO_CCCR_CIS + 1, 0) & 0xff) << 8;
    cis |= (cyw43455_sdio_cmd52(false, false, 0,
                                CYW43455_SDIO_CCCR_CIS + 2, 0) & 0xff) << 16;
    g_assert_cmphex(cis, ==, CYW43455_SDIO_CIS_COMMON);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis, 0) & 0xff,
                    ==, 0x20); /* CISTPL_MANFID */
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 1, 0) & 0xff,
                    ==, 4);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 2, 0) & 0xff,
                    ==, 0xd0);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 3, 0) & 0xff,
                    ==, 0x02);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 4, 0) & 0xff,
                    ==, 0xa6);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 5, 0) & 0xff,
                    ==, 0xa9);

    g_assert_cmphex(cyw43455_sdio_cmd52(
                        false, false, 0, 3 * CYW43455_SDIO_FBR_SIZE, 0) &
                    0xff, ==, 0x02);
    cis = cyw43455_sdio_cmd52(
              false, false, 0,
              3 * CYW43455_SDIO_FBR_SIZE + CYW43455_SDIO_FBR_CIS, 0) & 0xff;
    cis |= (cyw43455_sdio_cmd52(
                 false, false, 0,
                 3 * CYW43455_SDIO_FBR_SIZE + CYW43455_SDIO_FBR_CIS + 1,
                 0) & 0xff) << 8;
    cis |= (cyw43455_sdio_cmd52(
                 false, false, 0,
                 3 * CYW43455_SDIO_FBR_SIZE + CYW43455_SDIO_FBR_CIS + 2,
                 0) & 0xff) << 16;
    g_assert_cmphex(cis, ==, CYW43455_SDIO_CIS_FUNC3);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 2, 0) & 0xff,
                    ==, 0xd0);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 3, 0) & 0xff,
                    ==, 0x02);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 4, 0) & 0xff,
                    ==, 0xa6);
    g_assert_cmphex(cyw43455_sdio_cmd52(false, false, 0, cis + 5, 0) & 0xff,
                    ==, 0xa9);

    response = cyw43455_sdio_cmd52(true, true, 3, 0x400, 0x3c);
    g_assert_cmphex(response & 0xff, ==, 0x3c);

    response = cyw43455_sdio_cmd52(true, true, 0,
                                   CYW43455_SDIO_CCCR_IOEX,
                                   BIT(1) | BIT(2) | BIT(3));
    g_assert_cmphex(response & 0xff, ==, BIT(1) | BIT(2) | BIT(3));
    response = cyw43455_sdio_cmd52(false, false, 0,
                                   CYW43455_SDIO_CCCR_IORX, 0);
    g_assert_cmphex(response & 0xff, ==, BIT(1) | BIT(2) | BIT(3));

    response = cyw43455_sdio_cmd52(true, true, 1,
                                   CYW43455_SDIO_CHIPCLKCSR,
                                   CYW43455_SDIO_HWREQ_OFF |
                                   CYW43455_SDIO_ALP_REQ);
    g_assert_cmphex(response & 0xff, ==,
                    CYW43455_SDIO_HWREQ_OFF |
                    CYW43455_SDIO_ALP_REQ |
                    CYW43455_SDIO_ALP_AVAIL);
    response = cyw43455_sdio_cmd52(true, true, 1,
                                   CYW43455_SDIO_CHIPCLKCSR,
                                   CYW43455_SDIO_HWREQ_OFF |
                                   CYW43455_SDIO_FORCE_ALP);
    g_assert_cmphex(response & 0xff, ==,
                    CYW43455_SDIO_HWREQ_OFF |
                    CYW43455_SDIO_FORCE_ALP |
                    CYW43455_SDIO_ALP_AVAIL);

    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_CHIP_ID_ADDR),
                    ==, CYW43455_SDIO_CHIP_ID);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_PTR),
                    ==, CYW43455_SDIO_EROM_BASE);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE),
                    ==, 0x00080001);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 4),
                    ==, 0x33004001);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 2 * 4),
                    ==, 0x00000003);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 3 * 4),
                    ==, 0x18000005);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 4 * 4),
                    ==, 0x181000c5);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 10 * 4),
                    ==, 0x00083e01);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_EROM_BASE + 25 * 4),
                    ==, 0x0000000f);

    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_CR4_BASE + 4),
                    ==, 0x12);
    cyw43455_sdio_backplane_writel_qtest(
        global_qtest, CYW43455_SDIO_CR4_BASE + 0x40, 2);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_CR4_BASE + 0x44),
                    ==, 0x0f);
    cyw43455_sdio_backplane_writel_qtest(
        global_qtest, CYW43455_SDIO_CR4_WRAPPER + 0x800, 1);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest,
                        CYW43455_SDIO_CR4_WRAPPER + 0x800),
                    ==, 1);

    /* Exercise an incrementing, byte-mode CMD53 transaction on function 1. */
    argument = SDIO_CMD53_WRITE | (1U << SDIO_CMD_FUNC_SHIFT) |
               SDIO_CMD53_INCREMENT | (0x400U << SDIO_CMD_ADDR_SHIFT) | 4;
    sdhci_cmd_regs(global_qtest, RASPI4_EMMC1_BASE, 4, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    writel(RASPI4_EMMC1_BASE + SDHC_BDATA, 0x44332211);

    argument &= ~SDIO_CMD53_WRITE;
    sdhci_cmd_regs(global_qtest, RASPI4_EMMC1_BASE, 4, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN | SDHC_TRNS_READ,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    g_assert_cmphex(readl(RASPI4_EMMC1_BASE + SDHC_BDATA), ==,
                    0x44332211);

    /* Exercise eight 64-byte blocks in one register transfer. */
    argument = SDIO_CMD53_WRITE | (1U << SDIO_CMD_FUNC_SHIFT) |
               SDIO_CMD53_BLOCK_MODE | SDIO_CMD53_INCREMENT |
               (0x800U << SDIO_CMD_ADDR_SHIFT) | 8;
    sdhci_cmd_regs(global_qtest, RASPI4_EMMC1_BASE, 512, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    for (unsigned int i = 0; i < 512 / sizeof(uint32_t); i++) {
        writel(RASPI4_EMMC1_BASE + SDHC_BDATA, 0x80000000U | i);
    }

    argument &= ~SDIO_CMD53_WRITE;
    sdhci_cmd_regs(global_qtest, RASPI4_EMMC1_BASE, 512, 1, argument,
                   SDHC_TRNS_BLK_CNT_EN | SDHC_TRNS_READ,
                   (SDIO_CMD_IO_RW_EXTENDED << 8) |
                   SDHC_CMD_DATA_PRESENT | SDHC_CMD_RESPONSE);
    for (unsigned int i = 0; i < 512 / sizeof(uint32_t); i++) {
        g_assert_cmphex(readl(RASPI4_EMMC1_BASE + SDHC_BDATA), ==,
                        0x80000000U | i);
    }

    /* Exercise a firmware-control response and its level SDIO interrupt. */
    cyw43455_sdio_enable_control_irq_qtest(global_qtest);
    control_len = cyw43455_sdio_build_cur_etheraddr_request(
        control_frame, sizeof(control_frame));
    cyw43455_sdio_function2_write_qtest(global_qtest, control_frame,
                                         control_len);
    g_assert_cmphex(cyw43455_sdio_cmd52(
                        false, false, 0, CYW43455_SDIO_CCCR_INTPEND, 0) & 0xff,
                    ==, BIT(1));
    g_assert_cmphex(readw(RASPI4_EMMC1_BASE + SDHC_NORINTSTS) &
                    SDHC_NIS_CARDINT, ==, SDHC_NIS_CARDINT);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_INTSTATUS),
                    ==, CYW43455_SDIO_FRAME_IND);

    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(global_qtest, control_frame,
                                        control_len);
    cyw43455_sdio_assert_cur_etheraddr_response(
        control_frame, control_len, cyw43455_sdio_default_test_mac, 1);
    cyw43455_sdio_backplane_writel_qtest(
        global_qtest, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_cmd52(
                        false, false, 0, CYW43455_SDIO_CCCR_INTPEND, 0) & 0xff,
                    ==, 0);
    g_assert_cmphex(readw(RASPI4_EMMC1_BASE + SDHC_NORINTSTS) &
                    SDHC_NIS_CARDINT, ==, 0);

    /* Data transmit credits arrive only after the configured delay. */
    data_len = cyw43455_sdio_build_data_request(
        data_frame, sizeof(data_frame), packet, sizeof(packet));
    cyw43455_sdio_function2_write_qtest(global_qtest, data_frame, data_len);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
    qtest_clock_step(global_qtest,
                     (CYW43455_TEST_CREDIT_DELAY_MS - 1) * SCALE_MS);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
    qtest_clock_step(global_qtest, SCALE_MS);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        global_qtest, CYW43455_SDIO_INTSTATUS),
                    ==, CYW43455_SDIO_FRAME_IND);
    memset(credit_frame, 0, sizeof(credit_frame));
    cyw43455_sdio_function2_read_qtest(global_qtest, credit_frame,
                                        sizeof(credit_frame));
    cyw43455_sdio_assert_credit_update(credit_frame, sizeof(credit_frame),
                                        data_frame[4],
                                        CYW43455_TEST_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        global_qtest, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
}

#ifndef _WIN32
static void cyw43455_sdio_send_all(int socket_fd, const void *buffer,
                                   size_t size)
{
    const uint8_t *p = buffer;

    while (size) {
        ssize_t ret = send(socket_fd, p, size, 0);

        if (ret < 0 && errno == EINTR) {
            continue;
        }
        g_assert_cmpint(ret, >, 0);
        p += ret;
        size -= ret;
    }
}

static void cyw43455_sdio_recv_all(int socket_fd, void *buffer, size_t size)
{
    uint8_t *p = buffer;

    while (size) {
        GPollFD pollfd = {
            .fd = socket_fd,
            .events = G_IO_IN | G_IO_HUP | G_IO_ERR,
        };
        ssize_t ret;

        g_assert_cmpint(g_poll(&pollfd, 1, 5000), >, 0);
        ret = recv(socket_fd, p, size, 0);
        g_assert_cmpint(ret, >, 0);
        p += ret;
        size -= ret;
    }
}

static uint16_t cyw43455_sdio_icmp6_checksum_qtest(
    const uint8_t *source, const uint8_t *destination,
    uint8_t *payload, uint32_t payload_len)
{
    uint8_t pseudo[40] = { };
    uint32_t sum = 0;
    unsigned int i;

    memcpy(pseudo, source, 16);
    memcpy(pseudo + 16, destination, 16);
    stl_be_p(pseudo + 32, payload_len);
    pseudo[39] = CYW43455_ICMP6_NEXT_HEADER;
    for (i = 0; i < sizeof(pseudo); i += 2) {
        sum += lduw_be_p(pseudo + i);
    }
    for (i = 0; i < payload_len; i += 2) {
        sum += (uint16_t)payload[i] << 8;
        if (i + 1 < payload_len) {
            sum += payload[i + 1];
        }
    }
    while (sum >> 16) {
        sum = (sum & UINT16_MAX) + (sum >> 16);
    }
    return ~sum;
}

static void cyw43455_sdio_send_packet(int socket_fd, const uint8_t *packet,
                                      size_t packet_len)
{
    uint32_t framed_len;

    g_assert_cmpuint(packet_len, <=, UINT32_MAX);
    framed_len = htonl(packet_len);
    cyw43455_sdio_send_all(socket_fd, &framed_len, sizeof(framed_len));
    cyw43455_sdio_send_all(socket_fd, packet, packet_len);
}

static void cyw43455_sdio_wait_for_frame(QTestState *qts)
{
    int64_t deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;

    do {
        if (cyw43455_sdio_backplane_readl_qtest(
                qts, CYW43455_SDIO_INTSTATUS) & CYW43455_SDIO_FRAME_IND) {
            return;
        }
        g_usleep(1000);
    } while (g_get_monotonic_time() < deadline);

    g_error("timed out waiting for a CYW43455 receive frame");
}

static void cyw43455_sdio_assert_data_response(const uint8_t *frame,
                                                size_t stored_len,
                                                const uint8_t *packet,
                                                size_t packet_len,
                                                uint8_t sequence,
                                                uint8_t tx_max)
{
    size_t frame_len = CYW43455_SDPCM_HEADER_LEN +
                       CYW43455_BCDC_DATA_HEADER_LEN + packet_len;
    size_t payload_offset = CYW43455_SDPCM_HEADER_LEN +
                            CYW43455_BCDC_DATA_HEADER_LEN;

    g_assert_cmpuint(stored_len, ==,
                     QEMU_ALIGN_UP(frame_len, sizeof(uint32_t)));
    g_assert_cmpuint(lduw_le_p(frame), ==, frame_len);
    g_assert_cmphex(lduw_le_p(frame + 2), ==, (uint16_t)~frame_len);
    g_assert_cmpuint(frame[4], ==, sequence);
    g_assert_cmpuint(frame[5] & 0xf, ==, CYW43455_SDPCM_DATA_CHANNEL);
    g_assert_cmpuint(frame[7], ==, CYW43455_SDPCM_HEADER_LEN);
    g_assert_cmpuint(frame[9], ==, tx_max);
    g_assert_cmphex(frame[CYW43455_SDPCM_HEADER_LEN], ==, 2 << 4);
    g_assert_cmphex(frame[CYW43455_SDPCM_HEADER_LEN + 1], ==, 0);
    g_assert_cmphex(frame[CYW43455_SDPCM_HEADER_LEN + 2], ==, 0);
    g_assert_cmphex(frame[CYW43455_SDPCM_HEADER_LEN + 3], ==, 0);
    g_assert_cmpmem(frame + payload_offset, packet_len, packet, packet_len);
    for (size_t i = frame_len; i < stored_len; i++) {
        g_assert_cmphex(frame[i], ==, 0);
    }
}

static void test_cyw43455_sdio_network_backend(void)
{
    static const uint8_t guest_mac[6] = { 0x02, 0, 0, 0, 0, 1 };
    static const uint8_t runtime_mac[6] = { 0x02, 0, 0, 0, 0, 0x33 };
    static const uint8_t peer_mac[6] = { 0x52, 0x55, 0x0a, 0, 2, 2 };
    static const uint8_t guest_ip[4] = { 10, 0, 2, 15 };
    static const uint8_t peer_ip[4] = { 10, 0, 2, 2 };
    static const uint8_t guest_ip6[16] = {
        0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0f,
    };
    static const uint8_t peer_ip6[16] = {
        0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02,
    };
    static const uint8_t solicited_ip6[16] = {
        0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0xff, 0, 0, 0x0f,
    };
    static const uint8_t joined_multicast[6] = {
        0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb,
    };
    static const uint8_t second_multicast[6] = {
        0x33, 0x33, 0x00, 0x00, 0x00, 0xfb,
    };
    static const uint8_t unlisted_multicast[6] = {
        0x33, 0x33, 0xff, 0x00, 0x00, 0x01,
    };
    static const uint8_t packet[64] = {
        0x02, 0, 0, 0, 0, 2,
        0x02, 0, 0, 0, 0, 1,
        0x08, 0x00,
        0x45, 0x00, 0x00, 0x32, 0x00, 0x00, 0x40, 0x00,
        0x40, 0x11, 0x00, 0x00, 0xc0, 0x00, 0x02, 0x02,
        0xc0, 0x00, 0x02, 0x01, 0x1a, 0x0a, 0x1a, 0x0a,
        0x00, 0x1e, 0x00, 0x00,
        'q', 'e', 'm', 'u', '-', 'c', 'y', 'w', '4', '3', '4', '5', '5',
    };
    g_autoptr(GString) command = g_string_new(
        "-machine raspi4b,sdio-wifi=on");
    QTestState *qts;
    uint8_t sdpcm_frame[CYW43455_SDPCM_HEADER_LEN +
                        CYW43455_BCDC_DATA_HEADER_LEN + sizeof(packet)];
    uint8_t glom_frame[2 * (CYW43455_SDPCM_HW_HEADER_LEN +
                            CYW43455_SDPCM_HWEXT_LEN + 8 +
                            CYW43455_BCDC_DATA_HEADER_LEN + sizeof(packet))];
    uint8_t received[sizeof(packet)];
    uint8_t inbound_packet[sizeof(packet)];
    uint8_t second_packet[sizeof(packet) - 1];
    uint8_t filtered_packet[sizeof(packet)];
    uint8_t arp_request[60] = { };
    uint8_t arp_reply[60] = { };
    uint8_t expected_arp_reply[60] = { };
    uint8_t nd_solicitation[CYW43455_ETH_HEADER_LEN + 40 +
                            CYW43455_ND_PAYLOAD_LEN] = { };
    uint8_t nd_advertisement[sizeof(nd_solicitation)] = { };
    uint8_t expected_nd_advertisement[sizeof(nd_solicitation)] = { };
    uint8_t multicast_data[sizeof(uint32_t) + 2 * 6];
    uint8_t scalar[sizeof(uint32_t)];
    uint8_t control_frame[320];
    uint8_t credit_frame[CYW43455_SDPCM_HEADER_LEN];
    uint8_t inbound_frame[256];
    uint8_t event_frame[CYW43455_EVENT_FRAME_LEN];
    uint32_t received_len;
    size_t inbound_len;
    const size_t packet_frame_len =
        QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                      CYW43455_BCDC_DATA_HEADER_LEN + sizeof(packet),
                      sizeof(uint32_t));
    size_t control_len;
    size_t sdpcm_len;
    int net_sockets[2];
    int ret;

    memcpy(inbound_packet, packet, sizeof(inbound_packet));
    memcpy(inbound_packet, guest_mac, sizeof(guest_mac));
    memcpy(inbound_packet + sizeof(guest_mac), peer_mac, sizeof(peer_mac));

    memset(arp_request, 0xff, sizeof(guest_mac));
    memcpy(arp_request + sizeof(guest_mac), peer_mac, sizeof(peer_mac));
    stw_be_p(arp_request + 12, 0x0806);
    stw_be_p(arp_request + 14, 1);
    stw_be_p(arp_request + 16, 0x0800);
    arp_request[18] = sizeof(peer_mac);
    arp_request[19] = sizeof(peer_ip);
    stw_be_p(arp_request + 20, 1);
    memcpy(arp_request + 22, peer_mac, sizeof(peer_mac));
    memcpy(arp_request + 28, peer_ip, sizeof(peer_ip));
    memcpy(arp_request + 38, guest_ip, sizeof(guest_ip));

    memcpy(expected_arp_reply, peer_mac, sizeof(peer_mac));
    memcpy(expected_arp_reply + sizeof(peer_mac), runtime_mac,
           sizeof(runtime_mac));
    stw_be_p(expected_arp_reply + 12, 0x0806);
    stw_be_p(expected_arp_reply + 14, 1);
    stw_be_p(expected_arp_reply + 16, 0x0800);
    expected_arp_reply[18] = sizeof(peer_mac);
    expected_arp_reply[19] = sizeof(peer_ip);
    stw_be_p(expected_arp_reply + 20, 2);
    memcpy(expected_arp_reply + 22, runtime_mac, sizeof(runtime_mac));
    memcpy(expected_arp_reply + 28, guest_ip, sizeof(guest_ip));
    memcpy(expected_arp_reply + 32, peer_mac, sizeof(peer_mac));
    memcpy(expected_arp_reply + 38, peer_ip, sizeof(peer_ip));

    memcpy(nd_solicitation, "\x33\x33\xff\x00\x00\x0f", 6);
    memcpy(nd_solicitation + 6, peer_mac, sizeof(peer_mac));
    stw_be_p(nd_solicitation + 12, 0x86dd);
    stl_be_p(nd_solicitation + 14, 6U << 28);
    stw_be_p(nd_solicitation + 18, CYW43455_ND_PAYLOAD_LEN);
    nd_solicitation[20] = CYW43455_ICMP6_NEXT_HEADER;
    nd_solicitation[21] = 255;
    memcpy(nd_solicitation + 22, peer_ip6, sizeof(peer_ip6));
    memcpy(nd_solicitation + 38, solicited_ip6, sizeof(solicited_ip6));
    nd_solicitation[54] = 135;
    memcpy(nd_solicitation + 62, guest_ip6, sizeof(guest_ip6));
    nd_solicitation[78] = 1;
    nd_solicitation[79] = 1;
    memcpy(nd_solicitation + 80, peer_mac, sizeof(peer_mac));
    stw_be_p(nd_solicitation + 56,
             cyw43455_sdio_icmp6_checksum_qtest(
                 nd_solicitation + 22, nd_solicitation + 38,
                 nd_solicitation + 54, CYW43455_ND_PAYLOAD_LEN));

    memcpy(expected_nd_advertisement, peer_mac, sizeof(peer_mac));
    memcpy(expected_nd_advertisement + 6, runtime_mac,
           sizeof(runtime_mac));
    stw_be_p(expected_nd_advertisement + 12, 0x86dd);
    stl_be_p(expected_nd_advertisement + 14, 6U << 28);
    stw_be_p(expected_nd_advertisement + 18, CYW43455_ND_PAYLOAD_LEN);
    expected_nd_advertisement[20] = CYW43455_ICMP6_NEXT_HEADER;
    expected_nd_advertisement[21] = 255;
    memcpy(expected_nd_advertisement + 22, guest_ip6, sizeof(guest_ip6));
    memcpy(expected_nd_advertisement + 38, peer_ip6, sizeof(peer_ip6));
    expected_nd_advertisement[54] = 136;
    stl_be_p(expected_nd_advertisement + 58, BIT(30) | BIT(29));
    memcpy(expected_nd_advertisement + 62, guest_ip6, sizeof(guest_ip6));
    expected_nd_advertisement[78] = 2;
    expected_nd_advertisement[79] = 1;
    memcpy(expected_nd_advertisement + 80, runtime_mac,
           sizeof(runtime_mac));
    stw_be_p(expected_nd_advertisement + 56,
             cyw43455_sdio_icmp6_checksum_qtest(
                 expected_nd_advertisement + 22,
                 expected_nd_advertisement + 38,
                 expected_nd_advertisement + 54,
                 CYW43455_ND_PAYLOAD_LEN));

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, net_sockets);
    g_assert_cmpint(ret, !=, -1);
    g_string_append_printf(command,
                           " -nic socket,id=wifi-test,fd=%d,"
                           "model=cyw43455-sdio,"
                           "mac=02:00:00:00:00:01",
                           net_sockets[1]);
    qts = qtest_init(command->str);
    close(net_sockets[1]);

    cyw43455_sdio_select_qtest(qts);
    cyw43455_sdio_enable_control_irq_qtest(qts);

    /* Backend changes cannot associate before firmware startup completes. */
    qtest_qmp_assert_success(qts,
        "{ 'execute': 'set_link', 'arguments': { "
        "'name': 'wifi-test', 'up': false } }");
    qtest_qmp_assert_success(qts,
        "{ 'execute': 'set_link', 'arguments': { "
        "'name': 'wifi-test', 'up': true } }");
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    /* Backend input waits for function two and synthetic firmware startup. */
    cyw43455_sdio_send_packet(net_sockets[0], inbound_packet,
                              sizeof(inbound_packet));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(
                        qts, true, true, 0, CYW43455_SDIO_CCCR_IOEX,
                        BIT(1) | BIT(2)) & 0xff,
                    ==, BIT(1) | BIT(2));
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(
                        qts, false, false, 0, CYW43455_SDIO_CCCR_IORX, 0) &
                    0xff,
                    ==, BIT(1) | BIT(2));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", 1, 2);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);

    /* Linux acknowledges an interrupt before consuming its SDPCM frame. */
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);

    /* A second packet cannot replace the acknowledged-but-unread first one. */
    memcpy(second_packet, inbound_packet, sizeof(second_packet));
    second_packet[sizeof(second_packet) - 1] ^= 1;
    cyw43455_sdio_send_packet(net_sockets[0], second_packet,
                              sizeof(second_packet));

    /* A control reply queues behind data already presented to the guest. */
    control_len = cyw43455_sdio_build_cur_etheraddr_request(
        control_frame, sizeof(control_frame));
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);

    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    inbound_len = QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                                CYW43455_BCDC_DATA_HEADER_LEN +
                                sizeof(packet), sizeof(uint32_t));
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame, inbound_len);
    cyw43455_sdio_assert_data_response(inbound_frame, inbound_len,
                                       inbound_packet,
                                       sizeof(inbound_packet), 1,
                                       1 + CYW43455_DEFAULT_CREDIT_WINDOW);

    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_cur_etheraddr_response(
        control_frame, control_len, guest_mac, 1);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_wait_for_frame(qts);
    inbound_len = QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                                CYW43455_BCDC_DATA_HEADER_LEN +
                                sizeof(second_packet), sizeof(uint32_t));
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame, inbound_len);
    cyw43455_sdio_assert_data_response(inbound_frame, inbound_len,
                                       second_packet, sizeof(second_packet),
                                       3,
                                       1 + CYW43455_DEFAULT_CREDIT_WINDOW);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, CYW43455_SDIO_FRAME_IND);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame, 64);
    for (unsigned int i = 0; i < 64; i++) {
        g_assert_cmphex(inbound_frame[i], ==, 0);
    }

    /* A post-open filter update produces one synthetic association event. */
    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        0, 4, 3);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PROMISC, 4, 3);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_wait_for_frame(qts);
    memset(event_frame, 0xff, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(qts, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_link_event(
        event_frame, sizeof(event_frame), 5,
        4 + CYW43455_DEFAULT_CREDIT_WINDOW, true, guest_mac);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Repeated filter updates must not synthesize duplicate link events. */
    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        0, 5, 4);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PROMISC, 6, 4);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    /* The next netdev open queries toe_ol before updating its filters. */
    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "toe_ol", 6, 5);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_zero_iovar_response(
        control_frame, control_len, 7, 5);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        0, 7, 6);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PROMISC, 8, 6);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_wait_for_frame(qts);
    memset(event_frame, 0xff, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(qts, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_link_event(
        event_frame, sizeof(event_frame), 9,
        7 + CYW43455_DEFAULT_CREDIT_WINDOW, true, guest_mac);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    /* Firmware filtering drops foreign unicast but always admits broadcast. */
    memcpy(filtered_packet, inbound_packet, sizeof(filtered_packet));
    filtered_packet[5] = 0x99;
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    memset(filtered_packet, 0xff, sizeof(guest_mac));
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    cyw43455_sdio_wait_for_frame(qts);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame,
                                       packet_frame_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, packet_frame_len, filtered_packet,
        sizeof(filtered_packet), 10, 7 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Selective multicast is closed until the address is programmed. */
    memcpy(filtered_packet, inbound_packet, sizeof(filtered_packet));
    memcpy(filtered_packet, joined_multicast, sizeof(joined_multicast));
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    stl_le_p(multicast_data, 2);
    memcpy(multicast_data + sizeof(uint32_t), joined_multicast,
           sizeof(joined_multicast));
    memcpy(multicast_data + sizeof(uint32_t) + sizeof(joined_multicast),
           second_multicast, sizeof(second_multicast));
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "mcast_list",
        multicast_data, sizeof(multicast_data), 8, 7);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "mcast_list", sizeof(multicast_data),
        11, 7);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    cyw43455_sdio_wait_for_frame(qts);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame,
                                       packet_frame_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, packet_frame_len, filtered_packet,
        sizeof(filtered_packet), 12,
        8 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    memcpy(filtered_packet, unlisted_multicast,
           sizeof(unlisted_multicast));
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    stl_le_p(scalar, 1);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "allmulti", scalar,
        sizeof(scalar), 9, 8);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "allmulti", sizeof(scalar), 13, 8);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    cyw43455_sdio_wait_for_frame(qts);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame,
                                       packet_frame_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, packet_frame_len, filtered_packet,
        sizeof(filtered_packet), 14,
        9 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 0);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "allmulti", scalar,
        sizeof(scalar), 10, 9);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "allmulti", sizeof(scalar), 15, 9);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        1, 11, 10);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PROMISC, 16, 10);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    memcpy(filtered_packet, inbound_packet, sizeof(filtered_packet));
    filtered_packet[5] = 0x99;
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    cyw43455_sdio_wait_for_frame(qts);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame,
                                       packet_frame_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, packet_frame_len, filtered_packet,
        sizeof(filtered_packet), 17,
        11 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 2000);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "pm2_sleep_ret", scalar,
        sizeof(scalar), 12, 11);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "pm2_sleep_ret", sizeof(scalar), 18, 11);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        0, 13, 12);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PROMISC, 19, 12);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    qtest_qmp_assert_success(qts,
        "{ 'execute': 'set_link', 'arguments': { "
        "'name': 'wifi-test', 'up': false } }");
    cyw43455_sdio_wait_for_frame(qts);
    memset(event_frame, 0xff, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(qts, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_link_event(
        event_frame, sizeof(event_frame), 20,
        13 + CYW43455_DEFAULT_CREDIT_WINDOW, false, guest_mac);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_qmp_assert_success(qts,
        "{ 'execute': 'set_link', 'arguments': { "
        "'name': 'wifi-test', 'up': true } }");
    cyw43455_sdio_wait_for_frame(qts);
    memset(event_frame, 0xff, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(qts, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_link_event(
        event_frame, sizeof(event_frame), 21,
        13 + CYW43455_DEFAULT_CREDIT_WINDOW, true, guest_mac);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "cur_etheraddr", runtime_mac,
        sizeof(runtime_mac), 14, 13);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "cur_etheraddr", sizeof(runtime_mac),
        22, 13);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    memcpy(filtered_packet, inbound_packet, sizeof(filtered_packet));
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    memcpy(filtered_packet, runtime_mac, sizeof(runtime_mac));
    cyw43455_sdio_send_packet(net_sockets[0], filtered_packet,
                              sizeof(filtered_packet));
    cyw43455_sdio_wait_for_frame(qts);
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame,
                                       packet_frame_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, packet_frame_len, filtered_packet,
        sizeof(filtered_packet), 23,
        14 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "cur_etheraddr", 15, 14);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_cur_etheraddr_response(
        control_frame, control_len, runtime_mac, 14);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Firmware answers for a registered host address without waking Linux. */
    stl_le_p(scalar, CYW43455_ARP_OL_AGENT);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arp_ol", scalar,
        sizeof(scalar), 16, 15);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "arp_ol", sizeof(scalar), 25, 15);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 1);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", scalar,
        sizeof(scalar), 17, 16);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "arpoe", sizeof(scalar), 26, 16);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arp_hostip", guest_ip,
        sizeof(guest_ip), 18, 17);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "arp_hostip", sizeof(guest_ip), 27, 17);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], arp_request,
                              sizeof(arp_request));
    cyw43455_sdio_recv_all(net_sockets[0], &received_len,
                           sizeof(received_len));
    g_assert_cmpuint(ntohl(received_len), ==, sizeof(arp_reply));
    cyw43455_sdio_recv_all(net_sockets[0], arp_reply, sizeof(arp_reply));
    g_assert_cmpmem(arp_reply, sizeof(arp_reply),
                    expected_arp_reply, sizeof(expected_arp_reply));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    stl_le_p(scalar, 0);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", scalar,
        sizeof(scalar), 19, 18);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "arpoe", sizeof(scalar), 28, 18);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], arp_request,
                              sizeof(arp_request));
    cyw43455_sdio_wait_for_frame(qts);
    inbound_len = QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                                CYW43455_BCDC_DATA_HEADER_LEN +
                                sizeof(arp_request), sizeof(uint32_t));
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame, inbound_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, inbound_len, arp_request, sizeof(arp_request), 29,
        19 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 1);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "ndoe", scalar,
        sizeof(scalar), 20, 19);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "ndoe", sizeof(scalar), 30, 19);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "nd_hostip_clear", guest_ip6,
        0, 21, 20);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "nd_hostip_clear", 0, 31, 20);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "nd_hostip", guest_ip6,
        sizeof(guest_ip6), 22, 21);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "nd_hostip", sizeof(guest_ip6), 32, 21);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], nd_solicitation,
                              sizeof(nd_solicitation));
    cyw43455_sdio_recv_all(net_sockets[0], &received_len,
                           sizeof(received_len));
    g_assert_cmpuint(ntohl(received_len), ==, sizeof(nd_advertisement));
    cyw43455_sdio_recv_all(net_sockets[0], nd_advertisement,
                           sizeof(nd_advertisement));
    g_assert_cmpmem(nd_advertisement, sizeof(nd_advertisement),
                    expected_nd_advertisement,
                    sizeof(expected_nd_advertisement));
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    stl_le_p(scalar, 0);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "ndoe", scalar,
        sizeof(scalar), 23, 22);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "ndoe", sizeof(scalar), 33, 22);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Admit solicited-node multicast once firmware stops handling it. */
    stl_le_p(scalar, 1);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "allmulti", scalar,
        sizeof(scalar), 24, 23);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "allmulti", sizeof(scalar), 34, 23);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_send_packet(net_sockets[0], nd_solicitation,
                              sizeof(nd_solicitation));
    cyw43455_sdio_wait_for_frame(qts);
    inbound_len = QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                                CYW43455_BCDC_DATA_HEADER_LEN +
                                sizeof(nd_solicitation), sizeof(uint32_t));
    memset(inbound_frame, 0xff, sizeof(inbound_frame));
    cyw43455_sdio_function2_read_qtest(qts, inbound_frame, inbound_len);
    cyw43455_sdio_assert_data_response(
        inbound_frame, inbound_len, nd_solicitation,
        sizeof(nd_solicitation), 35,
        24 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    sdpcm_len = cyw43455_sdio_build_data_request(
        sdpcm_frame, sizeof(sdpcm_frame), packet, sizeof(packet));
    cyw43455_sdio_function2_write_qtest(qts, sdpcm_frame, sdpcm_len);

    cyw43455_sdio_recv_all(net_sockets[0], &received_len,
                            sizeof(received_len));
    g_assert_cmpuint(ntohl(received_len), ==, sizeof(packet));
    cyw43455_sdio_recv_all(net_sockets[0], received, sizeof(received));
    g_assert_cmpmem(received, sizeof(received), packet, sizeof(packet));

    sdpcm_len = cyw43455_sdio_build_two_data_glom(
        glom_frame, sizeof(glom_frame), packet, sizeof(packet));
    cyw43455_sdio_function2_write_qtest(qts, glom_frame, sdpcm_len);
    for (unsigned int i = 0; i < 2; i++) {
        cyw43455_sdio_recv_all(net_sockets[0], &received_len,
                               sizeof(received_len));
        g_assert_cmpuint(ntohl(received_len), ==, sizeof(packet));
        cyw43455_sdio_recv_all(net_sockets[0], received,
                               sizeof(received));
        g_assert_cmpmem(received, sizeof(received), packet, sizeof(packet));
    }

    qtest_clock_step(qts, SCALE_MS);
    cyw43455_sdio_wait_for_frame(qts);
    memset(credit_frame, 0, sizeof(credit_frame));
    cyw43455_sdio_function2_read_qtest(qts, credit_frame,
                                       sizeof(credit_frame));
    g_assert_true(credit_frame[9] == 1 + CYW43455_DEFAULT_CREDIT_WINDOW ||
                  credit_frame[9] == 2 + CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_assert_credit_update(
        credit_frame, sizeof(credit_frame),
        credit_frame[9] - CYW43455_DEFAULT_CREDIT_WINDOW,
        CYW43455_DEFAULT_CREDIT_WINDOW);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "sta_info", peer_mac,
        sizeof(peer_mac), CYW43455_STA_PAYLOAD_LEN, 3, 24);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_station_info_response(
        control_frame, control_len, 24, peer_mac, 3, 3 * sizeof(packet));
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_BSS_INFO,
        CYW43455_BSS_PAYLOAD_LEN, 4, 25);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    control_len = QEMU_ALIGN_UP(CYW43455_SDPCM_HEADER_LEN +
                                CYW43455_BCDC_HEADER_LEN +
                                CYW43455_BSS_PAYLOAD_LEN,
                                sizeof(uint32_t));
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_bss_info_response(
        control_frame, control_len, 25, peer_mac,
        CYW43455_CHANSPEC_2G_CH1_20MHZ, 1);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_quit(qts);
    close(net_sockets[0]);
}

static void test_cyw43455_sdio_bandwidth_channels(void)
{
    static const struct {
        uint32_t band;
        uint32_t capability;
        int32_t status;
    } queries[] = {
        { CYW43455_WLC_BAND_2G, CYW43455_BW_CAP_2G, 0 },
        { CYW43455_WLC_BAND_5G, 0, 0 },
        { 0, 0, CYW43455_BCME_BADARG },
    };
    const size_t bw_payload_len = sizeof("bw_cap") + sizeof(uint32_t);
    QTestState *qts = global_qtest;
    const uint8_t *payload;
    uint8_t control_frame[256];
    uint8_t bw_set[2 * sizeof(uint32_t)];
    uint8_t scalar[sizeof(uint32_t)];
    size_t control_len;
    unsigned int index;

    cyw43455_sdio_select_qtest(qts);
    cyw43455_sdio_enable_control_irq_qtest(qts);

    for (index = 0; index < ARRAY_SIZE(queries); index++) {
        stl_le_p(scalar, queries[index].band);
        control_len = cyw43455_sdio_build_iovar_data_request(
            control_frame, sizeof(control_frame), "bw_cap", scalar,
            sizeof(scalar), bw_payload_len, index, index + 1);
        cyw43455_sdio_function2_write_qtest(qts, control_frame,
                                             control_len);
        cyw43455_sdio_wait_for_frame(qts);
        memset(control_frame, 0xff, sizeof(control_frame));
        cyw43455_sdio_function2_read_qtest(qts, control_frame,
                                            control_len);
        payload = cyw43455_sdio_assert_iovar_data_response(
            control_frame, control_len, bw_payload_len, index, index + 1,
            queries[index].status);
        if (queries[index].status == 0) {
            g_assert_cmphex(ldl_le_p(payload), ==,
                            queries[index].capability);
        } else {
            g_assert_cmphex(ldl_le_p(payload), ==, 0);
        }
        for (size_t i = sizeof(uint32_t); i < bw_payload_len; i++) {
            g_assert_cmphex(payload[i], ==, 0);
        }
        cyw43455_sdio_backplane_writel_qtest(
            qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    }

    stl_le_p(bw_set, CYW43455_WLC_BAND_2G);
    stl_le_p(bw_set + sizeof(uint32_t), CYW43455_BW_CAP_2G);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "bw_cap", bw_set,
        sizeof(bw_set), 3, 4);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "bw_cap", sizeof(bw_set), 3, 4);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* The firmware contract requires a two-byte chanspec filter. */
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "chanspecs", scalar, 0,
        sizeof("chanspecs"), 4, 5);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, sizeof("chanspecs"), 4, 5,
        CYW43455_BCME_BADLEN);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stw_le_p(scalar, 0);
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "chanspecs", scalar,
        sizeof(uint16_t),
        CYW43455_CHANSPEC_LIST_LEN(CYW43455_CHANSPEC_ALL_COUNT), 5, 6);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len,
        CYW43455_CHANSPEC_LIST_LEN(CYW43455_CHANSPEC_ALL_COUNT), 5, 6, 0);
    g_assert_cmpuint(ldl_le_p(payload), ==,
                     CYW43455_CHANSPEC_ALL_COUNT);
    index = 1;
    for (uint32_t channel = 1; channel <= 13; channel++, index++) {
        g_assert_cmphex(ldl_le_p(payload + index * sizeof(uint32_t)), ==,
                        0x1000 | channel);
    }
    for (uint32_t channel = 3; channel <= 11; channel++, index++) {
        g_assert_cmphex(ldl_le_p(payload + index * sizeof(uint32_t)), ==,
                        0x1900 | channel);
    }
    for (uint32_t channel = 3; channel <= 11; channel++, index++) {
        g_assert_cmphex(ldl_le_p(payload + index * sizeof(uint32_t)), ==,
                        0x1800 | channel);
    }
    g_assert_cmpuint(index, ==, 1 + CYW43455_CHANSPEC_ALL_COUNT);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stw_le_p(scalar, 0x1800);
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "chanspecs", scalar,
        sizeof(uint16_t),
        CYW43455_CHANSPEC_LIST_LEN(CYW43455_CHANSPEC_40_COUNT), 6, 7);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0xff, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len,
        CYW43455_CHANSPEC_LIST_LEN(CYW43455_CHANSPEC_40_COUNT), 6, 7, 0);
    g_assert_cmpuint(ldl_le_p(payload), ==,
                     CYW43455_CHANSPEC_40_COUNT);
    index = 1;
    for (uint32_t channel = 3; channel <= 11; channel++, index++) {
        g_assert_cmphex(ldl_le_p(payload + index * sizeof(uint32_t)), ==,
                        0x1900 | channel);
    }
    for (uint32_t channel = 3; channel <= 11; channel++, index++) {
        g_assert_cmphex(ldl_le_p(payload + index * sizeof(uint32_t)), ==,
                        0x1800 | channel);
    }
    g_assert_cmpuint(index, ==, 1 + CYW43455_CHANSPEC_40_COUNT);
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "chanspec", 7, 8);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    stl_le_p(scalar, CYW43455_CHANSPEC_2G_CH13_40MHZ);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 7, 8, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
}

static void test_cyw43455_sdio_power_save_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line =
        g_string_new("-machine raspi4b,sdio-wifi=on");
    QTestState *source = global_qtest;
    QTestState *destination;
    uint8_t control_frame[64];
    size_t control_len;
    int net_sockets[2];
    int ret;

    cyw43455_sdio_select_qtest(source);
    cyw43455_sdio_enable_control_irq_qtest(source);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_PM,
        sizeof(uint32_t), 0, 1);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_query_command_value_response(
        control_frame, control_len, CYW43455_C_GET_PM, CYW43455_PM_OFF,
        0, 1);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PM, 3, 1, 2);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_command_status_response(
        control_frame, control_len, CYW43455_C_SET_PM, 1, 2,
        CYW43455_BCME_BADARG);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PM,
        CYW43455_PM_FAST, 2, 3);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PM, 2, 3);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_PM,
        sizeof(uint32_t), 3, 4);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_query_command_value_response(
        control_frame, control_len, CYW43455_C_GET_PM, CYW43455_PM_FAST,
        3, 4);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-cyw43455-power-XXXXXX", &error);
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
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_PM,
        sizeof(uint32_t), 4, 5);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_query_command_value_response(
        control_frame, control_len, CYW43455_C_GET_PM, CYW43455_PM_FAST,
        4, 5);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PM,
        CYW43455_PM_OFF, 5, 6);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_command_response(
        control_frame, control_len, CYW43455_C_SET_PM, 5, 6);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_PM,
        sizeof(uint32_t), 6, 7);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_query_command_value_response(
        control_frame, control_len, CYW43455_C_GET_PM, CYW43455_PM_OFF,
        6, 7);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_cyw43455_sdio_obss_coex_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line =
        g_string_new("-machine raspi4b,sdio-wifi=on");
    QTestState *source = global_qtest;
    QTestState *destination;
    uint8_t control_frame[64];
    uint8_t scalar[sizeof(uint32_t)];
    size_t control_len;
    int net_sockets[2];
    int ret;

    cyw43455_sdio_select_qtest(source);
    cyw43455_sdio_enable_control_irq_qtest(source);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", 0, 1);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    stl_le_p(scalar, CYW43455_OBSS_COEX_OFF);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 0, 1, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 2);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", scalar,
        sizeof(scalar), 1, 2);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_status_response(
        control_frame, control_len, "obss_coex", sizeof(scalar), 1, 2,
        CYW43455_BCME_BADARG);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stw_le_p(scalar, CYW43455_OBSS_COEX_ON);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", scalar,
        sizeof(uint16_t), 2, 3);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_status_response(
        control_frame, control_len, "obss_coex", sizeof(uint16_t), 2, 3,
        CYW43455_BCME_BADLEN);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, CYW43455_OBSS_COEX_ON);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", scalar,
        sizeof(scalar), 3, 4);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "obss_coex", sizeof(scalar), 3, 4);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", 4, 5);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 4, 5, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-cyw43455-obss-XXXXXX", &error);
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
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", 5, 6);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 5, 6, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, CYW43455_OBSS_COEX_AUTO);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", scalar,
        sizeof(scalar), 6, 7);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "obss_coex", sizeof(scalar), 6, 7);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "obss_coex", 7, 8);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 7, 8, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_cyw43455_sdio_scan_migration(void)
{
    const uint16_t first_sync_id = 0x1234;
    const uint16_t filtered_sync_id = 0x5678;
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line =
        g_string_new("-machine raspi4b,sdio-wifi=on");
    QTestState *source = global_qtest;
    QTestState *destination;
    uint8_t control_frame[160];
    uint8_t event_frame[CYW43455_ESCAN_EVENT_FRAME_LEN];
    uint8_t scalar[sizeof(uint32_t)];
    size_t control_len;
    int net_sockets[2];
    int ret;

    cyw43455_sdio_select_qtest(source);
    cyw43455_sdio_enable_control_irq_qtest(source);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "scan_ver", 0, 1);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    stl_le_p(scalar, CYW43455_ESCAN_REQ_VERSION_V2);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 0, 1, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_escan_request(
        control_frame, sizeof(control_frame),
        CYW43455_ESCAN_REQ_VERSION_V2, first_sync_id,
        CYW43455_CHANSPEC_2G_CH13_20MHZ, 1, 2);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "escan",
        CYW43455_ESCAN_V2_REQUEST_LEN, 1, 2);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    cyw43455_sdio_wait_for_frame(source);

    /* Migrate with the partial result active and completion still pending. */
    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-cyw43455-scan-XXXXXX", &error);
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
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    memset(event_frame, 0, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(destination, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_escan_event(
        event_frame, sizeof(event_frame), 2,
        1 + CYW43455_DEFAULT_CREDIT_WINDOW, first_sync_id, true,
        cyw43455_sdio_default_test_mac,
        CYW43455_CHANSPEC_2G_CH13_40MHZ, 13);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    cyw43455_sdio_wait_for_frame(destination);
    memset(event_frame, 0, CYW43455_EVENT_FRAME_LEN);
    cyw43455_sdio_function2_read_qtest(destination, event_frame,
                                       CYW43455_EVENT_FRAME_LEN);
    cyw43455_sdio_assert_escan_event(
        event_frame, CYW43455_EVENT_FRAME_LEN, 3,
        1 + CYW43455_DEFAULT_CREDIT_WINDOW, first_sync_id, false,
        cyw43455_sdio_default_test_mac,
        CYW43455_CHANSPEC_2G_CH13_40MHZ, 13);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Legacy v1 remains accepted and does not invent an off-channel BSS. */
    control_len = cyw43455_sdio_build_escan_request(
        control_frame, sizeof(control_frame),
        CYW43455_ESCAN_REQ_VERSION_V1, filtered_sync_id, 6, 2, 3);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "escan",
        CYW43455_ESCAN_V1_REQUEST_LEN, 4, 3);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    cyw43455_sdio_wait_for_frame(destination);
    memset(event_frame, 0, CYW43455_EVENT_FRAME_LEN);
    cyw43455_sdio_function2_read_qtest(destination, event_frame,
                                       CYW43455_EVENT_FRAME_LEN);
    cyw43455_sdio_assert_escan_event(
        event_frame, CYW43455_EVENT_FRAME_LEN, 5,
        2 + CYW43455_DEFAULT_CREDIT_WINDOW, filtered_sync_id, false,
        cyw43455_sdio_default_test_mac,
        CYW43455_CHANSPEC_2G_CH13_40MHZ, 13);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        destination, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_cyw43455_sdio_survey_migration(void)
{
    static const char expected_results[] =
        "OBSS 0 0 0 0 0 0 0 0 0 0 0 0 0 0 12\n"
        "IBSS 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3\n"
        "TXDur 0 0 0 0 0 0 0 0 0 0 0 0 0 5\n"
        "Category 0 0 0 0 0 0 0 0 0 0 0 0 4\n"
        "Packet 0 0 0 0 0 0 0 0 0 0 0 0 0 0 2\n"
        "Opp(time): 0 0 0 0 0 0 0 0 0 0 0 50\n";
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line =
        g_string_new("-machine raspi4b,sdio-wifi=on");
    QTestState *source = global_qtest;
    QTestState *destination;
    const uint8_t *payload;
    uint8_t control_frame[320];
    uint8_t query[2 * sizeof(uint32_t)];
    uint8_t scalar[sizeof(uint32_t)];
    size_t control_len;
    int net_sockets[2];
    int ret;

    cyw43455_sdio_select_qtest(source);
    cyw43455_sdio_enable_control_irq_qtest(source);

    memset(scalar, 0, sizeof(scalar));
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "dump_obss", scalar,
        sizeof(scalar), sizeof("dump_obss") + sizeof(scalar), 0, 1);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, sizeof("dump_obss") + sizeof(scalar),
        0, 1, 0);
    g_assert_cmphex(ldl_le_p(payload), ==, 0);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, CYW43455_CHANSPEC_2G_CH1_20MHZ);
    control_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "chanspec", scalar,
        sizeof(scalar), 1, 2);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, control_len, "chanspec", sizeof(scalar), 1, 2);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_query_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_GET_PHY_NOISE,
        sizeof(uint32_t), 2, 3);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    cyw43455_sdio_assert_query_command_value_response(
        control_frame, control_len, CYW43455_C_GET_PHY_NOISE,
        (uint32_t)(int32_t)CYW43455_NOISE_DBM, 2, 3);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(query, 0);
    stl_le_p(query + sizeof(uint32_t), CYW43455_SURVEY_DURATION_MS);
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "dump_obss", query,
        sizeof(query), CYW43455_SURVEY_PAYLOAD_LEN, 3, 4);
    cyw43455_sdio_function2_write_qtest(source, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, CYW43455_SURVEY_PAYLOAD_LEN, 3, 4, 0);
    g_assert_cmpuint(ldl_le_p(payload), ==, CYW43455_SURVEY_DURATION_MS);
    g_assert_cmpuint(ldl_le_p(payload + sizeof(uint32_t)), ==, 0);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Migrate after starting the measurement but before collecting it. */
    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-cyw43455-survey-XXXXXX", &error);
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
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    stl_le_p(query, 1);
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "dump_obss", query,
        sizeof(query), CYW43455_SURVEY_PAYLOAD_LEN, 4, 5);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, CYW43455_SURVEY_PAYLOAD_LEN, 4, 5, 0);
    g_assert_cmpuint(ldl_le_p(payload), ==, CYW43455_SURVEY_DURATION_MS);
    g_assert_cmpuint(ldl_le_p(payload + sizeof(uint32_t)), ==, 1);
    g_assert_cmpstr((const char *)payload + 2 * sizeof(uint32_t), ==,
                    expected_results);
    for (size_t i = 2 * sizeof(uint32_t) + sizeof(expected_results);
         i < CYW43455_SURVEY_PAYLOAD_LEN; i++) {
        g_assert_cmphex(payload[i], ==, 0);
    }
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "chanspec", 5, 6);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    stl_le_p(scalar, CYW43455_CHANSPEC_2G_CH1_20MHZ);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 5, 6, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* Results are consumable once; a second collection is rejected. */
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "dump_obss", query,
        sizeof(query), CYW43455_SURVEY_PAYLOAD_LEN, 6, 7);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        control_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       control_len);
    cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, CYW43455_SURVEY_PAYLOAD_LEN, 6, 7,
        CYW43455_BCME_BADARG);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        destination, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_cyw43455_sdio_wlc_version(void)
{
    static const uint8_t expected_version[CYW43455_WLC_VERSION_LEN] = {
        1, 0, 16, 0, 7, 0, 84, 0,
        17, 0, 1, 0, 7, 0, 84, 0,
    };
    QTestState *qts = global_qtest;
    const uint8_t *payload;
    uint8_t control_frame[64];
    uint32_t scalar = 0;
    size_t control_len;

    cyw43455_sdio_select_qtest(qts);
    cyw43455_sdio_enable_control_irq_qtest(qts);

    control_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "wlc_ver", 0, 1);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, control_len, 0, 1, expected_version,
        sizeof(expected_version));
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    /* The full fixed-size structure must fit in the advertised buffer. */
    control_len = cyw43455_sdio_build_iovar_data_request(
        control_frame, sizeof(control_frame), "wlc_ver", &scalar,
        sizeof(scalar), 12, 1, 2);
    cyw43455_sdio_function2_write_qtest(qts, control_frame, control_len);
    cyw43455_sdio_wait_for_frame(qts);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(qts, control_frame, control_len);
    payload = cyw43455_sdio_assert_iovar_data_response(
        control_frame, control_len, 12, 1, 2, CYW43455_BCME_BADLEN);
    for (size_t i = 0; i < 12; i++) {
        g_assert_cmphex(payload[i], ==, 0);
    }
    cyw43455_sdio_backplane_writel_qtest(
        qts, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        qts, CYW43455_SDIO_INTSTATUS),
                    ==, 0);
}

static void test_cyw43455_sdio_migration(void)
{
    static const uint8_t multicast[6] = {
        0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb,
    };
    static const uint8_t migrated_mac[6] = {
        0x02, 0, 0, 0, 0, 0x44,
    };
    static const uint8_t guest_ip[4] = { 10, 0, 2, 15 };
    static const uint8_t guest_ip6[16] = {
        0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0f,
    };
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autoptr(GString) cmd_line =
        g_string_new("-machine raspi4b,sdio-wifi=on");
    QTestState *source = global_qtest;
    QTestState *destination;
    uint8_t control_frame[128];
    uint8_t event_frame[CYW43455_EVENT_FRAME_LEN];
    uint8_t multicast_data[sizeof(uint32_t) + sizeof(multicast)];
    uint8_t scalar[sizeof(uint32_t)];
    size_t arpoe_len;
    size_t filter_len;
    size_t current_len;
    size_t offload_len;
    size_t promisc_len;
    int net_sockets[2];
    int ret;

    cyw43455_sdio_select_qtest(source);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(source, true, true, 1,
                                              0x400, 0x5a) & 0xff,
                    ==, 0x5a);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(source, true, true, 3,
                                              0x400, 0x3c) & 0xff,
                    ==, 0x3c);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_CR4_WRAPPER + 0x408, 0x23);
    cyw43455_sdio_enable_control_irq_qtest(source);

    /* Complete synthetic startup so a later filter update can associate. */
    arpoe_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", 0, 9);
    cyw43455_sdio_function2_write_qtest(source, control_frame, arpoe_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, arpoe_len);
    cyw43455_sdio_assert_zero_iovar_response(control_frame, arpoe_len,
                                              0, 9);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(multicast_data, 1);
    memcpy(multicast_data + sizeof(uint32_t), multicast, sizeof(multicast));
    filter_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "mcast_list", multicast_data,
        sizeof(multicast_data), 1, 10);
    cyw43455_sdio_function2_write_qtest(source, control_frame, filter_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, filter_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, filter_len, "mcast_list", sizeof(multicast_data),
        1, 10);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 0);
    filter_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "allmulti", scalar,
        sizeof(scalar), 2, 11);
    cyw43455_sdio_function2_write_qtest(source, control_frame, filter_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, filter_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, filter_len, "allmulti", sizeof(scalar), 2, 11);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 2000);
    filter_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "pm2_sleep_ret", scalar,
        sizeof(scalar), 3, 12);
    cyw43455_sdio_function2_write_qtest(source, control_frame, filter_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, filter_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, filter_len, "pm2_sleep_ret", sizeof(scalar), 3, 12);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    filter_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "cur_etheraddr", migrated_mac,
        sizeof(migrated_mac), 4, 16);
    cyw43455_sdio_function2_write_qtest(source, control_frame, filter_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, filter_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, filter_len, "cur_etheraddr", sizeof(migrated_mac),
        4, 16);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, CYW43455_ARP_OL_AGENT |
                     CYW43455_ARP_OL_PEER_AUTO_REPLY);
    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arp_ol", scalar,
        sizeof(scalar), 5, 17);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "arp_ol", sizeof(scalar), 5, 17);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 1);
    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", scalar,
        sizeof(scalar), 6, 18);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "arpoe", sizeof(scalar), 6, 18);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "arp_hostip", guest_ip,
        sizeof(guest_ip), 7, 19);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "arp_hostip", sizeof(guest_ip), 7, 19);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    stl_le_p(scalar, 1);
    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "ndoe", scalar,
        sizeof(scalar), 8, 23);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "ndoe", sizeof(scalar), 8, 23);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "nd_hostip_clear", guest_ip6,
        0, 9, 24);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "nd_hostip_clear", 0, 9, 24);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    offload_len = cyw43455_sdio_build_set_iovar_request(
        control_frame, sizeof(control_frame), "nd_hostip", guest_ip6,
        sizeof(guest_ip6), 10, 25);
    cyw43455_sdio_function2_write_qtest(source, control_frame, offload_len);
    cyw43455_sdio_wait_for_frame(source);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(source, control_frame, offload_len);
    cyw43455_sdio_assert_set_iovar_response(
        control_frame, offload_len, "nd_hostip", sizeof(guest_ip6), 10, 25);
    cyw43455_sdio_backplane_writel_qtest(
        source, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "cur_etheraddr", 11, 1);
    cyw43455_sdio_function2_write_qtest(source, control_frame, current_len);
    promisc_len = cyw43455_sdio_build_command_request(
        control_frame, sizeof(control_frame), CYW43455_C_SET_PROMISC,
        0, 12, 2);
    cyw43455_sdio_function2_write_qtest(source, control_frame, promisc_len);
    g_assert_cmphex(qtest_readw(source, RASPI4_EMMC1_BASE +
                                       SDHC_NORINTSTS) & SDHC_NIS_CARDINT,
                    ==, SDHC_NIS_CARDINT);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("raspi4b-cyw43455-migration-XXXXXX", &error);
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
    qtest_qmp_assert_success(destination,
        "{ 'execute': 'migrate-incoming', 'arguments': { "
        "'uri': %s, 'exit-on-error': false } }", uri);
    raspi4b_wait_for_migration(destination);

    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(destination, false, false, 1,
                                              0x400, 0) & 0xff,
                    ==, 0x5a);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(destination, false, false, 3,
                                              0x400, 0) & 0xff,
                    ==, 0x3c);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        destination,
                        CYW43455_SDIO_CR4_WRAPPER + 0x408),
                    ==, 0x23);
    g_assert_cmphex(cyw43455_sdio_cmd52_qtest(
                        destination, false, false, 0,
                        CYW43455_SDIO_CCCR_INTPEND, 0) & 0xff,
                    ==, BIT(1));
    g_assert_cmphex(qtest_readw(destination, RASPI4_EMMC1_BASE +
                                            SDHC_NORINTSTS) &
                    SDHC_NIS_CARDINT, ==, SDHC_NIS_CARDINT);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    cyw43455_sdio_assert_cur_etheraddr_response(
        control_frame, current_len, migrated_mac, 1);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       promisc_len);
    cyw43455_sdio_assert_command_response(
        control_frame, promisc_len, CYW43455_C_SET_PROMISC, 12, 2);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    cyw43455_sdio_wait_for_frame(destination);
    memset(event_frame, 0, sizeof(event_frame));
    cyw43455_sdio_function2_read_qtest(destination, event_frame,
                                       sizeof(event_frame));
    cyw43455_sdio_assert_link_event(
        event_frame, sizeof(event_frame), 13,
        12 + CYW43455_DEFAULT_CREDIT_WINDOW, true, migrated_mac);
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);
    g_assert_cmphex(cyw43455_sdio_backplane_readl_qtest(
                        destination, CYW43455_SDIO_INTSTATUS),
                    ==, 0);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "mcast_list", 13, 13);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 14, 13, multicast_data,
        sizeof(multicast_data));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "allmulti", 14, 14);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    memset(scalar, 0, sizeof(scalar));
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 15, 14, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "pm2_sleep_ret", 15, 15);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    stl_le_p(scalar, 2000);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 16, 15, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "arp_ol", 16, 20);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    stl_le_p(scalar, CYW43455_ARP_OL_AGENT |
                     CYW43455_ARP_OL_PEER_AUTO_REPLY);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 17, 20, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "arpoe", 17, 21);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    stl_le_p(scalar, 1);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 18, 21, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "arp_hostip", 18, 22);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 19, 22, guest_ip, sizeof(guest_ip));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "ndoe", 19, 26);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    stl_le_p(scalar, 1);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 20, 26, scalar, sizeof(scalar));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    current_len = cyw43455_sdio_build_iovar_request(
        control_frame, sizeof(control_frame), "nd_hostip", 20, 27);
    cyw43455_sdio_function2_write_qtest(destination, control_frame,
                                        current_len);
    cyw43455_sdio_wait_for_frame(destination);
    memset(control_frame, 0, sizeof(control_frame));
    cyw43455_sdio_function2_read_qtest(destination, control_frame,
                                       current_len);
    cyw43455_sdio_assert_iovar_value_response(
        control_frame, current_len, 21, 27, guest_ip6, sizeof(guest_ip6));
    cyw43455_sdio_backplane_writel_qtest(
        destination, CYW43455_SDIO_INTSTATUS, CYW43455_SDIO_FRAME_IND);

    qtest_quit(destination);
    close(net_sockets[0]);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

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

static void property_submit_raw_at(uint32_t address)
{
    uint32_t response;

    writel(RASPI4_MBOX_WRITE, address | MBOX_CHAN_PROPERTY);
    response = readl(RASPI4_MBOX_READ);
    g_assert_cmphex(response, ==, address | MBOX_CHAN_PROPERTY);
}

static void property_submit_raw(void)
{
    property_submit_raw_at(RASPI4_PROPERTY_BUFFER);
}

static uint32_t property_payload_qtest(QTestState *qts, size_t word)
{
    return qtest_readl(qts, RASPI4_PROPERTY_BUFFER + 20 + word * 4);
}

static uint32_t property_payload(size_t word)
{
    return property_payload_qtest(global_qtest, word);
}

static void test_firmware_board_identity(void)
{
    const uint32_t get_model[] = { UINT32_MAX };
    const uint32_t get_revision[] = { UINT32_MAX };
    const uint32_t get_serial[] = { UINT32_MAX, UINT32_MAX };
    const uint32_t short_serial[] = { 0xdeadbeef };
    const uint32_t set_customer_otp[] = { 0, 1, 0x13579bdf };
    const uint32_t get_customer_otp[] = { 0, 1, 0 };

    property_request(RPI_FWREQ_GET_BOARD_MODEL, get_model,
                     G_N_ELEMENTS(get_model), sizeof(get_model));
    g_assert_cmphex(property_payload(0), ==, 0);

    property_request(RPI_FWREQ_GET_BOARD_REVISION, get_revision,
                     G_N_ELEMENTS(get_revision), sizeof(get_revision));
    g_assert_cmphex(property_payload(0), ==, RASPI4B_BOARD_REVISION);

    property_request(RPI_FWREQ_GET_BOARD_SERIAL, get_serial,
                     G_N_ELEMENTS(get_serial), sizeof(get_serial));
    g_assert_cmphex(property_payload(0), ==, RASPI4_BOARD_SERIAL);
    g_assert_cmphex(property_payload(1), ==, 0);

    property_request(RPI_FWREQ_GET_BOARD_SERIAL, short_serial,
                     G_N_ELEMENTS(short_serial), sizeof(uint64_t));
    g_assert_cmphex(property_payload(0), ==, RASPI4_BOARD_SERIAL);

    property_request(RPI_FWREQ_SET_CUSTOMER_OTP, set_customer_otp,
                     G_N_ELEMENTS(set_customer_otp), sizeof(uint32_t));
    property_request(RPI_FWREQ_GET_CUSTOMER_OTP, get_customer_otp,
                     G_N_ELEMENTS(get_customer_otp),
                     sizeof(get_customer_otp));
    g_assert_cmphex(property_payload(2), ==, 0x13579bdf);

    qtest_system_reset(global_qtest);

    property_request(RPI_FWREQ_GET_BOARD_REVISION, get_revision,
                     G_N_ELEMENTS(get_revision), sizeof(get_revision));
    g_assert_cmphex(property_payload(0), ==, RASPI4B_BOARD_REVISION);
    property_request(RPI_FWREQ_GET_BOARD_SERIAL, get_serial,
                     G_N_ELEMENTS(get_serial), sizeof(get_serial));
    g_assert_cmphex(property_payload(0), ==, RASPI4_BOARD_SERIAL);
    g_assert_cmphex(property_payload(1), ==, 0);
    property_request(RPI_FWREQ_GET_CUSTOMER_OTP, get_customer_otp,
                     G_N_ELEMENTS(get_customer_otp),
                     sizeof(get_customer_otp));
    g_assert_cmphex(property_payload(2), ==, 0x13579bdf);
}

static void test_firmware_malformed_buffers(void)
{
    uint8_t message[64];
    uint8_t response[64];
    uint32_t get_palette[256] = { 0 };
    const uint32_t test_palette[] = {
        254, 2, 0x89abcdef, 0x76543210, 0, 0,
    };
    const uint32_t invalid_palette[] = {
        255, 2, 0xaabbccdd, 0x55667788, 0, 0,
    };
    const uint32_t set_palette[] = {
        254, 2, 0xaabbccdd, 0x55667788, 0, 0,
    };
    const uint32_t get_customer_otp[] = { 0, 2, 0, 0 };
    const uint64_t palette_0 = RASPI4_VCRAM_BASE;
    const uint64_t palette_254 = RASPI4_VCRAM_BASE + 254 * sizeof(uint32_t);
    const uint64_t palette_255 = RASPI4_VCRAM_BASE + 255 * sizeof(uint32_t);
    const uint64_t after_palette = RASPI4_VCRAM_BASE + 256 * sizeof(uint32_t);

    /*
     * Responses are truncated to each value-buffer capacity, while the
     * response header reports the complete desired length.  Odd capacities
     * still have four-byte-aligned tag padding.
     */
    memset(message, 0xa5, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_GET_BOARD_REVISION);
    stl_le_p(message + 12, 2);
    stl_le_p(message + 16, 0);
    stl_le_p(message + 24, RPI_FWREQ_GET_DMA_CHANNELS);
    stl_le_p(message + 28, 3);
    stl_le_p(message + 32, 0);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    memread(RASPI4_PROPERTY_BUFFER, response, 44);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 16), ==, 0x80000004);
    g_assert_cmphex(response[20], ==, RASPI4B_BOARD_REVISION & 0xff);
    g_assert_cmphex(response[21], ==, (RASPI4B_BOARD_REVISION >> 8) & 0xff);
    g_assert_cmphex(response[22], ==, 0xa5);
    g_assert_cmphex(response[23], ==, 0xa5);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 32), ==, 0x80000004);
    g_assert_cmphex(response[36], ==, 0xf5);
    g_assert_cmphex(response[37], ==, 0x07);
    g_assert_cmphex(response[38], ==, 0x00);
    g_assert_cmphex(response[39], ==, 0xa5);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 40), ==, 0);

    /* Unknown and explicitly unimplemented tags must retain request form. */
    memset(message, 0xa5, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, 0x7f123456);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 16, 0);
    stl_le_p(message + 20, 0x11223344);
    stl_le_p(message + 24, RPI_FWREQ_GET_FIRMWARE_VARIANT);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 32, 0);
    stl_le_p(message + 36, 0x55667788);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0x11223344);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 0x55667788);

    /* A zero-sized unknown tag still advances to the following valid tag. */
    memset(message, 0xa5, 40);
    stl_le_p(message, 40);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, 0x7f123456);
    stl_le_p(message + 12, 0);
    stl_le_p(message + 16, 0);
    stl_le_p(message + 20, RPI_FWREQ_GET_BOARD_MODEL);
    stl_le_p(message + 24, 4);
    stl_le_p(message + 28, 0);
    stl_le_p(message + 32, UINT32_MAX);
    stl_le_p(message + 36, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 40);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 28), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);

    /* A declared buffer shorter than its header must not be overwritten. */
    memset(message, 0xa5, sizeof(message));
    stl_le_p(message, sizeof(uint32_t));
    memwrite(RASPI4_PROPERTY_BUFFER, message, sizeof(message));
    property_submit_raw();
    memread(RASPI4_PROPERTY_BUFFER, response, sizeof(response));
    for (size_t i = sizeof(uint32_t); i < sizeof(response); i++) {
        g_assert_cmphex(response[i], ==, 0xa5);
    }

    /* Top-level reserved codes and non-word-aligned sizes are parse errors. */
    memset(message, 0xa5, 16);
    stl_le_p(message, 12);
    stl_le_p(message + 4, 1);
    stl_le_p(message + 8, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 16);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 8), ==,
                    RPI_FWREQ_PROPERTY_END);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 12), ==, 0xa5a5a5a5);

    memset(message, 0xa5, 16);
    stl_le_p(message, 13);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 16);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 8), ==,
                    RPI_FWREQ_PROPERTY_END);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 12), ==, 0xa5a5a5a5);

    /* A message without an end tag is an error and cannot escape its size. */
    memset(message, 0xa5, sizeof(message));
    stl_le_p(message, 20);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_GET_FIRMWARE_REVISION);
    stl_le_p(message + 12, 0);
    stl_le_p(message + 16, 0);
    memwrite(RASPI4_PROPERTY_BUFFER, message, sizeof(message));
    property_submit_raw();
    memread(RASPI4_PROPERTY_BUFFER, response, sizeof(response));
    g_assert_cmphex((uint32_t)ldl_le_p(response + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 16), ==, 0);
    for (size_t i = 20; i < sizeof(response); i++) {
        g_assert_cmphex(response[i], ==, 0xa5);
    }

    /* The old parser's exact zero-progress size is rejected without a walk. */
    memset(message, 0xa5, sizeof(message));
    stl_le_p(message, 24);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, 0x7f123456);
    stl_le_p(message + 12, UINT32_MAX - 11);
    stl_le_p(message + 16, 0);
    memwrite(RASPI4_PROPERTY_BUFFER, message, sizeof(message));
    property_submit_raw();
    memread(RASPI4_PROPERTY_BUFFER, response, sizeof(response));
    g_assert_cmphex((uint32_t)ldl_le_p(response + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex((uint32_t)ldl_le_p(response + 16), ==, 0);
    for (size_t i = 20; i < sizeof(response); i++) {
        g_assert_cmphex(response[i], ==, 0xa5);
    }

    /* Real firmware rejects property messages at or above one MiB. */
    memset(message, 0xa5, sizeof(message));
    stl_le_p(message, MiB);
    stl_le_p(message + 4, 0);
    memwrite(RASPI4_PROPERTY_BUFFER, message, sizeof(message));
    property_submit_raw();
    memread(RASPI4_PROPERTY_BUFFER, response, sizeof(response));
    g_assert_cmphex((uint32_t)ldl_le_p(response + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    for (size_t i = 8; i < sizeof(response); i++) {
        g_assert_cmphex(response[i], ==, 0xa5);
    }

    /* U-Boot supplies the request payload length in the per-tag code. */
    memset(message, 0xa5, 32);
    stl_le_p(message, 32);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_SET_POWER_STATE);
    stl_le_p(message + 12, 8);
    stl_le_p(message + 16, 8);
    stl_le_p(message + 20, RASPI4_POWER_USB_HCD_ID);
    stl_le_p(message + 24, RPI_FIRMWARE_STATE_ENABLE | BIT(1));
    stl_le_p(message + 28, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 32);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000008);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==,
                    RASPI4_POWER_USB_HCD_ID);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==,
                    RPI_FIRMWARE_STATE_ENABLE);

    /* A response marker is not valid in a fresh request. */
    memset(message, 0xa5, 28);
    stl_le_p(message, 28);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_GET_FIRMWARE_REVISION);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 16, BIT(31));
    stl_le_p(message + 24, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 28);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, BIT(31));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0xa5a5a5a5);

    /* Incomplete required inputs are parse errors. */
    memset(message, 0, 28);
    stl_le_p(message, 28);
    stl_le_p(message + 8, RPI_FWREQ_GET_CLOCK_RATE);
    stl_le_p(message + 12, 3);
    message[20] = RPI_FIRMWARE_CORE_CLK_ID;
    stl_le_p(message + 24, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 28);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==, 0);

    /* A short GET_CLOCKS response still advertises all fifteen clock pairs. */
    memset(message, 0xa5, 28);
    stl_le_p(message, 28);
    stl_le_p(message + 4, 0);
    stl_le_p(message + 8, RPI_FWREQ_GET_CLOCKS);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 16, 0);
    stl_le_p(message + 24, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 28);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==,
                    0x80000000 | 15 * 2 * sizeof(uint32_t));
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==, 0);

    /* Palette intervals must fit 256 entries and apply atomically. */
    writel(palette_0, 0x00112233);
    writel(palette_254, 0x01020304);
    writel(palette_255, 0x11121314);
    writel(after_palette, 0x21222324);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_PALETTE, get_palette,
                     G_N_ELEMENTS(get_palette), sizeof(get_palette));
    g_assert_cmphex(property_payload(0), ==, 0x00112233);
    g_assert_cmphex(property_payload(254), ==, 0x01020304);
    g_assert_cmphex(property_payload(255), ==, 0x11121314);

    property_request(RPI_FWREQ_FRAMEBUFFER_TEST_PALETTE, test_palette,
                     G_N_ELEMENTS(test_palette), sizeof(uint32_t));
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(readl(palette_254), ==, 0x01020304);
    g_assert_cmphex(readl(palette_255), ==, 0x11121314);

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_PALETTE, invalid_palette,
                     G_N_ELEMENTS(invalid_palette), sizeof(uint32_t));
    g_assert_cmphex(property_payload(0), ==, 1);
    g_assert_cmphex(readl(palette_254), ==, 0x01020304);
    g_assert_cmphex(readl(palette_255), ==, 0x11121314);
    g_assert_cmphex(readl(after_palette), ==, 0x21222324);

    memset(message, 0, 36);
    stl_le_p(message, 36);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 12, 12);
    stl_le_p(message + 20, 254);
    stl_le_p(message + 24, 2);
    stl_le_p(message + 28, 0xaabbccdd);
    stl_le_p(message + 32, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 36);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 1);
    g_assert_cmphex(readl(palette_254), ==, 0x01020304);
    g_assert_cmphex(readl(palette_255), ==, 0x11121314);

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_PALETTE, set_palette,
                     G_N_ELEMENTS(set_palette), sizeof(uint32_t));
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(readl(palette_254), ==, 0xaabbccdd);
    g_assert_cmphex(readl(palette_255), ==, 0x55667788);
    g_assert_cmphex(readl(after_palette), ==, 0x21222324);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_PALETTE, get_palette,
                     G_N_ELEMENTS(get_palette), sizeof(get_palette));
    g_assert_cmphex(property_payload(0), ==, 0x00112233);
    g_assert_cmphex(property_payload(254), ==, 0xaabbccdd);
    g_assert_cmphex(property_payload(255), ==, 0x55667788);

    /* An incomplete OTP write must not fuse even its first supplied row. */
    memset(message, 0, 36);
    stl_le_p(message, 36);
    stl_le_p(message + 8, RPI_FWREQ_SET_CUSTOMER_OTP);
    stl_le_p(message + 12, 12);
    stl_le_p(message + 20, 0);
    stl_le_p(message + 24, 2);
    stl_le_p(message + 28, 0x89abcdef);
    stl_le_p(message + 32, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 36);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    property_request(RPI_FWREQ_GET_CUSTOMER_OTP, get_customer_otp,
                     G_N_ELEMENTS(get_customer_otp),
                     sizeof(get_customer_otp));
    g_assert_cmphex(property_payload(2), ==, 0);
    g_assert_cmphex(property_payload(3), ==, 0);
}

static void test_firmware_framebuffer_operation(void)
{
    static const struct {
        uint32_t physical_width;
        uint32_t physical_height;
        uint32_t virtual_width;
        uint32_t virtual_height;
        uint32_t requested_xoffset;
        uint32_t requested_yoffset;
        uint32_t expected_xoffset;
        uint32_t expected_yoffset;
    } viewport_cases[] = {
        { 64, 48, 32, 96, 7, 100, 0, 48 },
        { 32, 64, 64, 32, 100, 7, 32, 0 },
    };
    uint8_t message[1084];
    const uint32_t get_depth[] = { UINT32_MAX };
    const uint32_t get_pitch[] = { UINT32_MAX };
    const uint32_t allocate_buffer[] = { 16, UINT32_MAX };
    const uint32_t set_depth_16[] = { 16 };
    const uint32_t set_depth_32[] = { 32 };
    const uint32_t set_depth_huge[] = { 0x80000000 };
    const uint32_t size_640x480[] = { 640, 480 };
    const uint64_t palette_254 = RASPI4_VCRAM_BASE + 254 * sizeof(uint32_t);
    const uint64_t palette_255 = RASPI4_VCRAM_BASE + 255 * sizeof(uint32_t);

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_DEPTH, set_depth_16,
                     G_N_ELEMENTS(set_depth_16), sizeof(set_depth_16));
    g_assert_cmphex(property_payload(0), ==, 16);

    /* Framebuffer Get tags observe all Set tags in the same operation. */
    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_GET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, UINT32_MAX);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, 32);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 32);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 32);

    /* Ordinary tags retain their original position around framebuffer Gets. */
    memset(message, 0, 40);
    stl_le_p(message, 40);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_GET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, UINT32_MAX);
    stl_le_p(message + 24, RPI_FWREQ_GET_TEMPERATURE);
    stl_le_p(message + 28, 0);
    stl_le_p(message + 36, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 40);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 32);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT,
                     size_640x480, G_N_ELEMENTS(size_640x480),
                     sizeof(size_640x480));
    g_assert_cmphex(property_payload(0), ==, 640);
    g_assert_cmphex(property_payload(1), ==, 480);
    property_request(RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT,
                     size_640x480, G_N_ELEMENTS(size_640x480),
                     sizeof(size_640x480));
    g_assert_cmphex(property_payload(0), ==, 640);
    g_assert_cmphex(property_payload(1), ==, 480);

    /* All configuration setters are combined before validation. */
    memset(message, 0, 92);
    stl_le_p(message, 92);
    stl_le_p(message + 8,
             RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT);
    stl_le_p(message + 12, 8);
    stl_le_p(message + 28,
             RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT);
    stl_le_p(message + 32, 8);
    stl_le_p(message + 40, 800);
    stl_le_p(message + 44, 600);
    stl_le_p(message + 48,
             RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_WIDTH_HEIGHT);
    stl_le_p(message + 52, 8);
    stl_le_p(message + 68,
             RPI_FWREQ_FRAMEBUFFER_GET_PHYSICAL_WIDTH_HEIGHT);
    stl_le_p(message + 72, 8);
    stl_le_p(message + 88, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 92);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000008);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 800);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 24), ==, 600);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 0x80000008);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 40), ==, 800);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 44), ==, 600);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 56), ==, 0x80000008);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 60), ==, 800);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 64), ==, 600);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 76), ==, 0x80000008);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 80), ==, 800);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 84), ==, 600);

    /* A larger virtual size on one axis must not underflow the other. */
    for (size_t i = 0; i < G_N_ELEMENTS(viewport_cases); i++) {
        memset(message, 0, 92);
        stl_le_p(message, 92);
        stl_le_p(message + 8,
                 RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT);
        stl_le_p(message + 12, 8);
        stl_le_p(message + 20, viewport_cases[i].physical_width);
        stl_le_p(message + 24, viewport_cases[i].physical_height);
        stl_le_p(message + 28,
                 RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT);
        stl_le_p(message + 32, 8);
        stl_le_p(message + 40, viewport_cases[i].virtual_width);
        stl_le_p(message + 44, viewport_cases[i].virtual_height);
        stl_le_p(message + 48,
                 RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_OFFSET);
        stl_le_p(message + 52, 8);
        stl_le_p(message + 60, viewport_cases[i].requested_xoffset);
        stl_le_p(message + 64, viewport_cases[i].requested_yoffset);
        stl_le_p(message + 68,
                 RPI_FWREQ_FRAMEBUFFER_GET_VIRTUAL_OFFSET);
        stl_le_p(message + 72, 8);
        stl_le_p(message + 88, RPI_FWREQ_PROPERTY_END);
        memwrite(RASPI4_PROPERTY_BUFFER, message, 92);
        property_submit_raw();

        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                        PROPERTY_RESPONSE_SUCCESS);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 56), ==, 0x80000008);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 60), ==,
                        viewport_cases[i].expected_xoffset);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 64), ==,
                        viewport_cases[i].expected_yoffset);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 76), ==, 0x80000008);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 80), ==,
                        viewport_cases[i].expected_xoffset);
        g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 84), ==,
                        viewport_cases[i].expected_yoffset);
    }

    /* Bound depth before pitch and allocation-size arithmetic can wrap. */
    property_request(RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT,
                     size_640x480, G_N_ELEMENTS(size_640x480),
                     sizeof(size_640x480));
    property_request(RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT,
                     size_640x480, G_N_ELEMENTS(size_640x480),
                     sizeof(size_640x480));
    property_request(RPI_FWREQ_FRAMEBUFFER_SET_DEPTH, set_depth_huge,
                     G_N_ELEMENTS(set_depth_huge), sizeof(set_depth_huge));
    g_assert_cmphex(property_payload(0), ==, 32);
    property_request(RPI_FWREQ_FRAMEBUFFER_GET_PITCH, get_pitch,
                     G_N_ELEMENTS(get_pitch), sizeof(get_pitch));
    g_assert_cmphex(property_payload(0), ==, 640 * sizeof(uint32_t));
    property_request(RPI_FWREQ_FRAMEBUFFER_ALLOCATE, allocate_buffer,
                     G_N_ELEMENTS(allocate_buffer), sizeof(allocate_buffer));
    g_assert_cmphex(property_payload(0), ==,
                    RASPI4_VCRAM_BASE + 0x00100000);
    g_assert_cmphex(property_payload(1), ==,
                    640 * 480 * sizeof(uint32_t));

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_DEPTH, set_depth_16,
                     G_N_ELEMENTS(set_depth_16), sizeof(set_depth_16));
    property_request(RPI_FWREQ_FRAMEBUFFER_TEST_DEPTH, set_depth_huge,
                     G_N_ELEMENTS(set_depth_huge), sizeof(set_depth_huge));
    g_assert_cmphex(property_payload(0), ==, 32);
    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 16);
    property_request(RPI_FWREQ_FRAMEBUFFER_SET_DEPTH, set_depth_32,
                     G_N_ELEMENTS(set_depth_32), sizeof(set_depth_32));

    /* Mixing Test with Set/Get is invalid and returns no tag responses. */
    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, 16);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_TEST_DEPTH);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, 16);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 32);

    /* Other 0x0004xxxx properties are not part of the framebuffer group. */
    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_VCHIQ_INIT);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, UINT32_MAX);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_TEST_DEPTH);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, 16);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 16);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 32);

    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_VCHIQ_INIT);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, UINT32_MAX);
    stl_le_p(message + 24, RPI_FWREQ_VCHIQ_INIT);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, UINT32_MAX);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 0);

    /* Duplicate framebuffer tags are rejected deterministically. */
    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, 16);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, 24);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 32);

    /* A later invalid framebuffer tag prevents earlier Set side effects. */
    memset(message, 0, 44);
    stl_le_p(message, 44);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, 16);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 28, 4);
    stl_le_p(message + 36, 254);
    stl_le_p(message + 40, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 44);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 32);

    /* A palette rejection does not contradict another successful Set. */
    memset(message, 0, 64);
    stl_le_p(message, 64);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH);
    stl_le_p(message + 12, 4);
    stl_le_p(message + 20, 16);
    stl_le_p(message + 24, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 28, 6 * sizeof(uint32_t));
    stl_le_p(message + 36, 256);
    stl_le_p(message + 40, 1);
    stl_le_p(message + 44, 0xdeadbeef);
    stl_le_p(message + 60, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 64);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 16);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 32), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 36), ==, 1);

    property_request(RPI_FWREQ_FRAMEBUFFER_GET_DEPTH, get_depth,
                     G_N_ELEMENTS(get_depth), sizeof(get_depth));
    g_assert_cmphex(property_payload(0), ==, 16);

    property_request(RPI_FWREQ_FRAMEBUFFER_SET_DEPTH, set_depth_32,
                     G_N_ELEMENTS(set_depth_32), sizeof(set_depth_32));
    g_assert_cmphex(property_payload(0), ==, 32);

    /* Palette staging must not rewrite untouched, aliased request words. */
    memset(message, 0, 48);
    stl_le_p(message, 48);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 12, 6 * sizeof(uint32_t));
    stl_le_p(message + 20, 254);
    stl_le_p(message + 24, 2);
    stl_le_p(message + 28, 0xaabbccdd);
    stl_le_p(message + 32, 0x55667788);
    stl_le_p(message + 44, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_VCRAM_BASE, message, 48);
    property_submit_raw_at(RASPI4_VCRAM_BASE);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 20), ==, 0);
    g_assert_cmphex(readl(palette_254), ==, 0xaabbccdd);
    g_assert_cmphex(readl(palette_255), ==, 0x55667788);

    /* Reject a palette update that would overwrite its own response. */
    memset(message, 0, 48);
    stl_le_p(message, 48);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 12, 6 * sizeof(uint32_t));
    stl_le_p(message + 20, 1);
    stl_le_p(message + 24, 1);
    stl_le_p(message + 28, 0xdeadbeef);
    stl_le_p(message + 44, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_VCRAM_BASE, message, 48);
    property_submit_raw_at(RASPI4_VCRAM_BASE);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 20), ==, 1);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 4), !=, 0xdeadbeef);

    /* The overlap check covers the GPU bus's other RAM aliases too. */
    memwrite(RASPI4_VCRAM_BASE, message, 48);
    property_submit_raw_at(RASPI4_VCRAM_BASE + RASPI4_GPU_RAM_ALIAS);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_VCRAM_BASE + 20), ==, 1);

    /* Palette Get tags likewise observe a later Set in the same operation. */
    writel(palette_254, 0x01020304);
    writel(palette_255, 0x11121314);

    /* Fork hardening: an ordinary-tag failure rolls back a staged palette. */
    memset(message, 0, 60);
    stl_le_p(message, 60);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 12, 6 * sizeof(uint32_t));
    stl_le_p(message + 20, 254);
    stl_le_p(message + 24, 1);
    stl_le_p(message + 28, 0xdeadbeef);
    stl_le_p(message + 44, RPI_FWREQ_GET_TEMPERATURE);
    stl_le_p(message + 48, 0);
    stl_le_p(message + 56, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 60);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_ERROR);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 0);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 52), ==, 0);
    g_assert_cmphex(readl(palette_254), ==, 0x01020304);

    /* The published palette value area is bounded at 1032 bytes. */
    memset(message, 0, 1060);
    stl_le_p(message, 1060);
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 12, 1036);
    stl_le_p(message + 20, 254);
    stl_le_p(message + 24, 1);
    stl_le_p(message + 28, 0xdeadbeef);
    stl_le_p(message + 1056, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, 1060);
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20), ==, 1);
    g_assert_cmphex(readl(palette_254), ==, 0x01020304);

    memset(message, 0, sizeof(message));
    stl_le_p(message, sizeof(message));
    stl_le_p(message + 8, RPI_FWREQ_FRAMEBUFFER_GET_PALETTE);
    stl_le_p(message + 12, 256 * sizeof(uint32_t));
    stl_le_p(message + 1044, RPI_FWREQ_FRAMEBUFFER_SET_PALETTE);
    stl_le_p(message + 1048, 6 * sizeof(uint32_t));
    stl_le_p(message + 1056, 254);
    stl_le_p(message + 1060, 2);
    stl_le_p(message + 1064, 0xaabbccdd);
    stl_le_p(message + 1068, 0x55667788);
    stl_le_p(message + 1080, RPI_FWREQ_PROPERTY_END);
    memwrite(RASPI4_PROPERTY_BUFFER, message, sizeof(message));
    property_submit_raw();
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==,
                    PROPERTY_RESPONSE_SUCCESS);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==, 0x80000400);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20 + 254 * 4), ==,
                    0xaabbccdd);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 20 + 255 * 4), ==,
                    0x55667788);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 1052), ==, 0x80000004);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 1056), ==, 0);
    g_assert_cmphex(readl(palette_254), ==, 0xaabbccdd);
    g_assert_cmphex(readl(palette_255), ==, 0x55667788);
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
    const char *extra_args;
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
    if (data->extra_args) {
        g_string_append(cmd_line, data->extra_args);
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

/*
 * HDMI0 hot-plug: the "connected" QOM property models the hot-plug detect
 * line and must be changeable while the machine runs, because the VC4 DRM
 * connector is polled rather than interrupt-driven.  HDMI_HOTPLUG is what
 * the guest driver reads, so assert the register follows the property.
 */
#define RASPI4_HDMI0_CORE          0xfef00700
#define RASPI4_HDMI_HOTPLUG        (RASPI4_HDMI0_CORE + 0x1a8)
#define HDMI_HOTPLUG_CONNECTED     0x1
#define RASPI4_HDMI0_QOM_PATH      "/machine/soc/peripherals/hdmi0"

static bool hdmi_get_connected(void)
{
    g_autoptr(QDict) response = qtest_qmp(global_qtest,
        "{ 'execute': 'qom-get',"
        "  'arguments': { 'path': %s, 'property': 'connected' } }",
        RASPI4_HDMI0_QOM_PATH);

    g_assert(!qdict_haskey(response, "error"));
    return qdict_get_bool(response, "return");
}

static void hdmi_set_connected(bool value)
{
    qtest_qmp_assert_success(global_qtest,
        "{ 'execute': 'qom-set',"
        "  'arguments': { 'path': %s, 'property': 'connected',"
        "                 'value': %i } }",
        RASPI4_HDMI0_QOM_PATH, value);
}

static void test_hdmi_hotplug(void)
{
    /* A display is attached by default. */
    g_assert_true(hdmi_get_connected());
    g_assert_cmphex(readl(RASPI4_HDMI_HOTPLUG) & HDMI_HOTPLUG_CONNECTED,
                    ==, HDMI_HOTPLUG_CONNECTED);

    /* Unplugging clears the bit the guest driver polls. */
    hdmi_set_connected(false);
    g_assert_false(hdmi_get_connected());
    g_assert_cmphex(readl(RASPI4_HDMI_HOTPLUG) & HDMI_HOTPLUG_CONNECTED,
                    ==, 0);

    /* Re-plugging restores it. */
    hdmi_set_connected(true);
    g_assert_true(hdmi_get_connected());
    g_assert_cmphex(readl(RASPI4_HDMI_HOTPLUG) & HDMI_HOTPLUG_CONNECTED,
                    ==, HDMI_HOTPLUG_CONNECTED);

    /* Setting the current value again is a no-op, not an error. */
    hdmi_set_connected(true);
    g_assert_true(hdmi_get_connected());

    /*
     * Hot-plug state is guest-visible, so a reset must not silently change
     * it: an unplugged display stays unplugged across a system reset.
     */
    hdmi_set_connected(false);
    raspi4b_watchdog_reset();
    g_assert_false(hdmi_get_connected());
    g_assert_cmphex(readl(RASPI4_HDMI_HOTPLUG) & HDMI_HOTPLUG_CONNECTED,
                    ==, 0);
    hdmi_set_connected(true);
}

static void raspi4b_add_test_with_args(const char *path, void (*func)(void),
                                       const char *extra_args)
{
    Raspi4TestData *data = g_new(Raspi4TestData, 1);

    data->func = func;
    data->extra_args = extra_args;
    qtest_add_data_func_full(path, data, raspi4b_test_run, g_free);
}

static void raspi4b_add_test(const char *path, void (*func)(void))
{
    raspi4b_add_test_with_args(path, func, NULL);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    pcie_has_edu = qtest_has_device("edu");

    raspi4b_add_test("/raspi4b/asb/bridge_ids", test_asb_bridge_ids);
    raspi4b_add_test("/raspi4b/gic400/identity", test_gic400_identity);
    raspi4b_add_test("/raspi4b/gic400/masked_hppir_and_apr",
                     test_gic400_masked_hppir_and_apr);
    raspi4b_add_test("/raspi4b/cpu/configuration", test_cpu_configuration);
    raspi4b_add_test("/raspi4b/powermgt/watchdog", test_powermgt_watchdog);
    raspi4b_add_test("/raspi4b/interrupts/system_timer",
                     test_system_timer_interrupts);
    raspi4b_add_test("/raspi4b/interrupts/aon_l2",
                     test_aon_interrupts_and_reset);
    raspi4b_add_test("/raspi4b/interrupts/spi0", test_spi0_interrupt);
    raspi4b_add_test("/raspi4b/aux/modem_and_reset",
                     test_aux_uart_modem_and_reset);
    raspi4b_add_test("/raspi4b/aux/spi_pio_and_reset",
                     test_aux_spi_pio_and_reset);
    raspi4b_add_test_with_args("/raspi4b/aux/spi_flash_transaction",
                               test_aux_spi_flash_transaction,
                               " -device m25p80,id=spi1flash,bus=spi1"
                               " -device m25p80,id=spi2flash,bus=spi2");
#ifndef _WIN32
    raspi4b_add_test_with_args("/raspi4b/aux/spi_flash_migration",
                               test_aux_spi_flash_migration,
                               " -device m25p80,id=spi1flash,bus=spi1");
#endif
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
    raspi4b_add_test_with_args("/raspi4b/sd/cyw43455_sdio",
                               test_cyw43455_sdio_endpoint,
                               " -machine sdio-wifi=on"
                               " -global cyw43455-sdio."
                               "tx-credit-delay-ms=25"
                               " -global cyw43455-sdio."
                               "tx-credit-window=5");
    raspi4b_add_test_with_args("/raspi4b/sd/cyw43455_sdio_wlc_version",
                               test_cyw43455_sdio_wlc_version,
                               " -machine sdio-wifi=on");
    raspi4b_add_test_with_args(
        "/raspi4b/sd/cyw43455_sdio_bandwidth_channels",
        test_cyw43455_sdio_bandwidth_channels,
        " -machine sdio-wifi=on"
        " -global cyw43455-sdio.ap-chanspec=0x190b");
#ifndef _WIN32
    raspi4b_add_test_with_args(
        "/raspi4b/sd/cyw43455_sdio_power_save_migration",
        test_cyw43455_sdio_power_save_migration,
        " -machine sdio-wifi=on");
    raspi4b_add_test_with_args(
        "/raspi4b/sd/cyw43455_sdio_obss_coex_migration",
        test_cyw43455_sdio_obss_coex_migration,
        " -machine sdio-wifi=on");
    raspi4b_add_test("/raspi4b/sd/cyw43455_sdio_network_backend",
                     test_cyw43455_sdio_network_backend);
    raspi4b_add_test_with_args("/raspi4b/sd/cyw43455_sdio_migration",
                               test_cyw43455_sdio_migration,
                               " -machine sdio-wifi=on");
    raspi4b_add_test_with_args("/raspi4b/sd/cyw43455_sdio_scan_migration",
                               test_cyw43455_sdio_scan_migration,
                               " -machine sdio-wifi=on"
                               " -global cyw43455-sdio."
                               "ap-chanspec=0x190b");
    raspi4b_add_test_with_args("/raspi4b/sd/cyw43455_sdio_survey_migration",
                               test_cyw43455_sdio_survey_migration,
                               " -machine sdio-wifi=on");
#endif
    raspi4b_add_test("/raspi4b/firmware_gpio", test_firmware_gpio);
    raspi4b_add_test("/raspi4b/firmware_dma_channels",
                     test_firmware_dma_channels);
    raspi4b_add_test("/raspi4b/firmware/board_identity",
                     test_firmware_board_identity);
    raspi4b_add_test("/raspi4b/firmware/malformed_buffers",
                     test_firmware_malformed_buffers);
    raspi4b_add_test("/raspi4b/firmware/framebuffer_operation",
                     test_firmware_framebuffer_operation);
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
    raspi4b_add_test("/raspi4b/pcie/root_event_irq",
                     test_pcie_root_event_irq);
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
    raspi4b_add_test("/raspi4b/hdmi/hotplug", test_hdmi_hotplug);
#endif

    return g_test_run();
}
