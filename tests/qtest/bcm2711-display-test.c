/*
 * QTests for the BCM2711 native display and HDMI service-plane blocks.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/bswap.h"
#include "libqtest.h"
#include "qobject/qdict.h"

#define DVP_BASE                    0xfef00000
#define DVP_CONTROL                 (DVP_BASE + 0x00)
#define DVP_SW_INIT                 (DVP_BASE + 0x04)
#define DVP_MISC_CONFIG             (DVP_BASE + 0x08)
#define DVP_SPARE                   (DVP_BASE + 0x0c)
#define DVP_SW_INIT_MASK            0x3f
#define DVP_MISC_CONFIG_MASK        0x18

#define HVS_BASE                    0xfe400000
#define HVS_DISPSTAT                (HVS_BASE + 0x0004)
#define HVS_DISPID                  (HVS_BASE + 0x0008)
#define HVS_DISPLIST0               (HVS_BASE + 0x0020)
#define HVS_DISPLACT0               (HVS_BASE + 0x0030)
#define HVS_DISPCTRL0               (HVS_BASE + 0x0040)
#define HVS_DISPSTAT0               (HVS_BASE + 0x0048)
#define HVS_CHANNEL_STRIDE          0x10
#define HVS_DLIST_BASE              (HVS_BASE + 0x4000)
#define HVS_DLIST_WORDS             4096
#define HVS_DISPCTRL_ENABLE         BIT(31)
#define HVS_DISPCTRL_RESET          BIT(30)
#define HVS_DISPSTAT_RUN            (2U << 30)
#define HVS_DISPSTAT_EMPTY          BIT(28)
#define HVS_CTL_END                 BIT(31)
#define HVS_CTL_VALID               BIT(30)
#define HVS_CTL_SIZE_SHIFT          24
#define HVS_CTL_ORDER_SHIFT         13
#define HVS_CTL_UNITY               BIT(15)
#define HVS_CTL_FORMAT_RGB565       4
#define HVS_CTL_FORMAT_RGB888       5
#define HVS_CTL_FORMAT_RGBA8888     7
#define HVS_CTL_ORDER_XRGB          2
#define HVS_CTL_ORDER_ARGB          2
#define HVS_POS0_VFLIP              BIT(31)
#define HVS_POS0_Y_SHIFT            16
#define HVS_POS0_HFLIP              BIT(15)
#define HVS_POS1_HEIGHT_SHIFT       16
#define HVS_POS2_HEIGHT_SHIFT       16
#define HVS_CTL2_ALPHA_MODE_FIXED   BIT(30)
#define HVS_CTL2_ALPHA_PREMULT      BIT(29)
#define HVS_CTL2_ALPHA_MIX          BIT(28)
#define HVS_CTL2_ALPHA_SHIFT        4
#define HVS_CTL2_ALPHA_OPAQUE       0xfff
#define HVS_PRIMARY_BASE            0x01000000
#define HVS_OVERLAY_BASE            0x01100000

#define PIXELVALVE2_BASE            0xfe20a000
#define PV_CONTROL                  (PIXELVALVE2_BASE + 0x00)
#define PV_V_CONTROL                (PIXELVALVE2_BASE + 0x04)
#define PV_INTEN                    (PIXELVALVE2_BASE + 0x24)
#define PV_INTSTAT                  (PIXELVALVE2_BASE + 0x28)
#define PV_CONTROL_ENABLE           BIT(0)
#define PV_CONTROL_FIFO_CLEAR       BIT(1)
#define PV_V_CONTROL_VIDEO_ENABLE   BIT(0)
#define PV_INT_VFP_START            BIT(7)
#define PV_INT_MASK                 0x3ff
#define PV_CONTROL_IDLE             0x00048000
#define PV_V_CONTROL_IDLE           0x01000000
#define PV_FRAME_PERIOD_NS          16666667
#define PIXELVALVE2_QOM_PATH \
    "/machine/soc/peripherals/pixelvalve2"

#define HDMI0_CORE_BASE             0xfef00700
#define HDMI0_DVP_BASE              0xfef00300
#define HDMI0_PHY_BASE              0xfef00f00
#define HDMI_SHARED_HD_BASE         0xfef20000
#define HDMI_FIFO_CTL               (HDMI0_CORE_BASE + 0x074)
#define HDMI_RAM_PACKET_CONFIG      (HDMI0_CORE_BASE + 0x0bc)
#define HDMI_RAM_PACKET_STATUS      (HDMI0_CORE_BASE + 0x0c4)
#define HDMI_SCHEDULER_CONTROL      (HDMI0_CORE_BASE + 0x0e0)
#define HDMI_HOTPLUG                (HDMI0_CORE_BASE + 0x1a8)
#define HDMI_DVP_CLOCK_STOP         (HDMI0_DVP_BASE + 0x0bc)
#define HDMI_FIFO_RECENTER_DONE     BIT(14)
#define HDMI_FIFO_WRITE_MASK        0xefff
#define HDMI_SCHEDULER_MODE_HDMI    BIT(0)
#define HDMI_SCHEDULER_HDMI_ACTIVE  BIT(1)
#define HDMI_MAI_CTL                (HDMI_SHARED_HD_BASE + 0x010)
#define HDMI_MAI_THR                (HDMI_SHARED_HD_BASE + 0x014)
#define HDMI_MAI_FMT                (HDMI_SHARED_HD_BASE + 0x018)
#define HDMI_MAI_DATA               (HDMI_SHARED_HD_BASE + 0x01c)
#define HDMI_MAI_CTL_DLATE          BIT(15)
#define HDMI_MAI_CTL_BUSY           BIT(14)
#define HDMI_MAI_CTL_CHALIGN        BIT(13)
#define HDMI_MAI_CTL_WHOLSMP        BIT(12)
#define HDMI_MAI_CTL_FULL           BIT(11)
#define HDMI_MAI_CTL_EMPTY          BIT(10)
#define HDMI_MAI_CTL_FLUSH          BIT(9)
#define HDMI_MAI_CTL_CHNUM_SHIFT    4
#define HDMI_MAI_CTL_ENABLE         BIT(3)
#define HDMI_MAI_CTL_ERRORE         BIT(2)
#define HDMI_MAI_CTL_ERRORF         BIT(1)
#define HDMI_MAI_CTL_RESET          BIT(0)
#define HDMI_MAI_FMT_PCM            (2U << 16)
#define HDMI_MAI_FMT_48000          (9U << 8)
#define HDMI_MAI_FIRST_FRAME_NS     20833
#define HDMI_MAI_FRAME_NS           20834
#define HDMI_MAI_FIFO_WORDS         64
#define HDMI0_QOM_PATH              "/machine/soc/peripherals/hdmi0"

#define DMA5_BASE                   0xfe007500
#define DMA_CS                      (DMA5_BASE + 0x00)
#define DMA_ADDR                    (DMA5_BASE + 0x04)
#define DMA_TXFR_LEN                (DMA5_BASE + 0x14)
#define DMA_ACTIVE                  BIT(0)
#define DMA_END                     BIT(1)
#define DMA_ISHELD                  BIT(5)
#define DMA_D_DREQ                 BIT(6)
#define DMA_S_INC                   BIT(8)
#define DMA_PERMAP(_n)              ((_n) << 16)
#define DMA_CB                      0x1000
#define DMA_SOURCE                  0x2000
#define HDMI_MAI_DATA_BUS           0x7ef2001c

#define HDMI0_AUTO_I2C_BASE         0xfef00b00
#define HDMI0_DDC_BASE              0xfef04500
#define HDMI1_AUTO_I2C_BASE         0xfef05b00
#define HDMI1_DDC_BASE              0xfef09500

#define BSC_CHIP_ADDRESS(_base)     ((_base) + 0x00)
#define BSC_DATA_IN(_base, _n)      ((_base) + 0x04 + 4 * (_n))
#define BSC_COUNT(_base)            ((_base) + 0x24)
#define BSC_CONTROL(_base)          ((_base) + 0x28)
#define BSC_IIC_ENABLE(_base)       ((_base) + 0x2c)
#define BSC_DATA_OUT(_base, _n)     ((_base) + 0x30 + 4 * (_n))
#define BSC_CONTROL_HIGH(_base)     ((_base) + 0x50)
#define BSC_SCL_PARAM(_base)        ((_base) + 0x54)

#define BSC_CONTROL_READ            BIT(0)
#define BSC_IIC_TRANSFER            BIT(0)
#define BSC_IIC_INTERRUPT           BIT(1)
#define BSC_IIC_NO_ACK              BIT(2)
#define BSC_IIC_NO_STOP             BIT(4)
#define BSC_IIC_NO_START            BIT(5)
#define BSC_IIC_RESTART             BIT(6)
#define BSC_CONTROL_HIGH_IGNORE_ACK BIT(1)

#define AUTO_I2C_CONTROL0(_base)    ((_base) + 0x26c)
#define AUTO_I2C_RELEASE_BSC        BIT(1)

#define EDID_ADDRESS_WRITE          0xa0
#define EDID_ADDRESS_READ           0xa1
#define EDID_BLOCK_LENGTH           128
#define EDID_LENGTH                 (2 * EDID_BLOCK_LENGTH)
#define EDID_EXTENSION_COUNT        126
#define CTA_EXTENSION_TAG           0x02
#define CTA_REVISION                0x03
#define CTA_BASIC_AUDIO             BIT(6)
#define CTA_DB_AUDIO                1
#define CTA_DB_VIDEO                2
#define CTA_DB_VENDOR               3
#define CTA_DB_SPEAKER              4
#define BSC_TRANSFER_LENGTH         32

static QTestState *display_start(void)
{
    return qtest_init("-machine raspi4b -nic none");
}

static void test_hvs_registers_and_reset(void)
{
    QTestState *qts = display_start();
    uint32_t control = HVS_DISPCTRL_ENABLE | (640U << 16) | 480;

    g_assert_cmphex(qtest_readl(qts, HVS_DISPID), ==, 0x64647276);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPSTAT), ==, 0);
    for (unsigned int channel = 0; channel < 3; channel++) {
        g_assert_cmphex(qtest_readl(qts, HVS_DISPCTRL0 +
                                        channel * HVS_CHANNEL_STRIDE),
                        ==, 0);
        g_assert_cmphex(qtest_readl(qts, HVS_DISPSTAT0 +
                                        channel * HVS_CHANNEL_STRIDE),
                        ==, HVS_DISPSTAT_EMPTY);
    }

    qtest_writel(qts, HVS_DLIST_BASE + 7 * sizeof(uint32_t), 0x12345678);
    g_assert_cmphex(qtest_readl(qts,
                               HVS_DLIST_BASE + 7 * sizeof(uint32_t)),
                    ==, 0x12345678);

    qtest_writel(qts, HVS_DISPLIST0, HVS_DLIST_WORDS + 0x24);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPLIST0), ==,
                    HVS_DLIST_WORDS + 0x24);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPLACT0), ==, 0x24);
    qtest_writel(qts, HVS_DISPLACT0, 0x55);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPLACT0), ==, 0x24);

    qtest_writel(qts, HVS_DISPCTRL0, control);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPCTRL0), ==, control);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPSTAT0), ==,
                    HVS_DISPSTAT_RUN);

    qtest_writel(qts, HVS_DISPCTRL0,
                 control | HVS_DISPCTRL_RESET);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPCTRL0), ==, 0);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPSTAT0), ==,
                    HVS_DISPSTAT_EMPTY);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPID), ==, 0x64647276);
    g_assert_cmphex(qtest_readl(qts,
                               HVS_DLIST_BASE + 7 * sizeof(uint32_t)),
                    ==, 0);
    g_assert_cmphex(qtest_readl(qts, HVS_DISPLIST0), ==, 0);
    qtest_quit(qts);
}

static void hvs_write_dlist(QTestState *qts, unsigned int index,
                            uint32_t value)
{
    qtest_writel(qts, HVS_DLIST_BASE + index * sizeof(uint32_t), value);
}

static char *hvs_screendump(QTestState *qts, size_t *length)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *path = NULL;
    char *contents = NULL;
    int fd;

    fd = g_file_open_tmp("bcm2711-hvs-XXXXXX.ppm", &path, &error);
    g_assert_no_error(error);
    g_assert_cmpint(fd, >=, 0);
    close(fd);
    qtest_qmp_assert_success(qts,
        "{ 'execute': 'screendump', 'arguments': { 'filename': %s } }",
        path);
    g_file_get_contents(path, &contents, length, &error);
    g_assert_no_error(error);
    unlink(path);
    return contents;
}

static void hvs_assert_ppm_pixel(const char *contents, size_t length,
                                 unsigned int x, unsigned int y,
                                 uint8_t red, uint8_t green, uint8_t blue)
{
    static const char header[] = "P6\n8 8\n255\n";
    size_t offset = sizeof(header) - 1 + ((size_t)y * 8 + x) * 3;

    g_assert_cmpuint(length, ==, sizeof(header) - 1 + 8 * 8 * 3);
    g_assert_cmpmem(contents, sizeof(header) - 1,
                    header, sizeof(header) - 1);
    g_assert_cmphex((uint8_t)contents[offset], ==, red);
    g_assert_cmphex((uint8_t)contents[offset + 1], ==, green);
    g_assert_cmphex((uint8_t)contents[offset + 2], ==, blue);
}

static void hvs_program_scaled_rgb565(QTestState *qts)
{
    uint8_t primary[8 * 8 * sizeof(uint16_t)];
    uint8_t overlay[2 * 2 * sizeof(uint16_t)];

    for (unsigned int pixel = 0; pixel < 8 * 8; pixel++) {
        stw_le_p(primary + pixel * sizeof(uint16_t), 0xf800);
    }
    stw_le_p(overlay + 0 * sizeof(uint16_t), 0x07e0);
    stw_le_p(overlay + 1 * sizeof(uint16_t), 0x001f);
    stw_le_p(overlay + 2 * sizeof(uint16_t), 0xffff);
    stw_le_p(overlay + 3 * sizeof(uint16_t), 0x0000);
    qtest_memwrite(qts, HVS_PRIMARY_BASE, primary, sizeof(primary));
    qtest_memwrite(qts, HVS_OVERLAY_BASE, overlay, sizeof(overlay));

    /* An opaque 8x8 primary plane followed by a scaled 2x2 overlay. */
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (8U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_XRGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_UNITY | HVS_CTL_FORMAT_RGB565);
    hvs_write_dlist(qts, 1, 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (8U << HVS_POS2_HEIGHT_SHIFT) | 8);
    hvs_write_dlist(qts, 4, 0xc0c0c0c0);
    hvs_write_dlist(qts, 5, HVS_PRIMARY_BASE);
    hvs_write_dlist(qts, 6, 0xc0c0c0c0);
    hvs_write_dlist(qts, 7, 8 * sizeof(uint16_t));

    hvs_write_dlist(qts, 8,
                    HVS_CTL_VALID | (9U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_XRGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_FORMAT_RGB565);
    hvs_write_dlist(qts, 9, (2U << HVS_POS0_Y_SHIFT) | 2);
    hvs_write_dlist(qts, 10,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 11, (4U << HVS_POS1_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 12, (2U << HVS_POS2_HEIGHT_SHIFT) | 2);
    hvs_write_dlist(qts, 13, 0xc0c0c0c0);
    hvs_write_dlist(qts, 14, HVS_OVERLAY_BASE);
    hvs_write_dlist(qts, 15, 0xc0c0c0c0);
    hvs_write_dlist(qts, 16, 2 * sizeof(uint16_t));
    hvs_write_dlist(qts, 17, HVS_CTL_END);
}

static void test_hvs_scaled_composition(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_scaled_rgb565(qts);

    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (8U << 16) | 8);
    contents = hvs_screendump(qts, &length);

    hvs_assert_ppm_pixel(contents, length, 0, 0, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 252, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 248);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 252, 248);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 7, 7, 248, 0, 0);

    /* Active display-list writes must update scanout without a list flip. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 9,
                    HVS_POS0_HFLIP | (2U << HVS_POS0_Y_SHIFT) | 2);
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 0, 248);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 252, 0);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 0, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 248, 252, 248);

    /* Linux DRM RGB888 uses B, G, R byte order on a little-endian guest. */
    g_clear_pointer(&contents, g_free);
    {
        static const uint8_t overlay_rgb888[] = {
            0x00, 0xff, 0x00, /* green */
            0xff, 0x00, 0x00, /* blue */
            0xff, 0xff, 0xff, /* white */
            0x00, 0x00, 0x00, /* black */
        };

        qtest_memwrite(qts, HVS_OVERLAY_BASE, overlay_rgb888,
                       sizeof(overlay_rgb888));
    }
    hvs_write_dlist(qts, 8,
                    HVS_CTL_VALID | (9U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_XRGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_FORMAT_RGB888);
    hvs_write_dlist(qts, 9, (2U << HVS_POS0_Y_SHIFT) | 2);
    hvs_write_dlist(qts, 16, 2 * 3);
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 255, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 255, 255, 255);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    /* ARGB8888 coverage alpha must blend with the lower red plane. */
    g_clear_pointer(&contents, g_free);
    {
        static const uint8_t overlay_argb8888[] = {
            0x00, 0xff, 0x00, 0x80, /* half-alpha green */
            0xff, 0x00, 0x00, 0xff, /* opaque blue */
            0xff, 0xff, 0xff, 0x00, /* transparent white */
            0x00, 0x00, 0x00, 0xff, /* opaque black */
        };

        qtest_memwrite(qts, HVS_OVERLAY_BASE, overlay_argb8888,
                       sizeof(overlay_argb8888));
    }
    hvs_write_dlist(qts, 8,
                    HVS_CTL_VALID | (9U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_ARGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_FORMAT_RGBA8888);
    hvs_write_dlist(qts, 10,
                    HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT);
    hvs_write_dlist(qts, 16, 2 * sizeof(uint32_t));
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 124, 128, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    /* Plane alpha mixes with per-pixel coverage alpha. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 10,
                    HVS_CTL2_ALPHA_MIX |
                    (0x800U << HVS_CTL2_ALPHA_SHIFT));
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 186, 64, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 124, 0, 128);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 124, 0, 0);

    /* Premultiplied pixels carry color channels scaled by their alpha. */
    g_clear_pointer(&contents, g_free);
    {
        static const uint8_t overlay_premult[] = {
            0x00, 0x80, 0x00, 0x80, /* half-alpha premultiplied green */
            0xff, 0x00, 0x00, 0xff, /* opaque blue */
            0x00, 0x00, 0x00, 0x00, /* transparent black */
            0x00, 0x00, 0x00, 0xff, /* opaque black */
        };

        qtest_memwrite(qts, HVS_OVERLAY_BASE, overlay_premult,
                       sizeof(overlay_premult));
    }
    hvs_write_dlist(qts, 10,
                    HVS_CTL2_ALPHA_PREMULT |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 124, 128, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    /* The two reflection flags are independent of scaling. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 9,
                    HVS_POS0_VFLIP | HVS_POS0_HFLIP |
                    (2U << HVS_POS0_Y_SHIFT) | 2);
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 124, 128, 0);

    /* A terminated list may use every advertised compositor layer. */
    g_clear_pointer(&contents, g_free);
    for (unsigned int layer = 0; layer < 16; layer++) {
        unsigned int word = layer * 8;

        hvs_write_dlist(qts, word,
                        HVS_CTL_VALID | (8U << HVS_CTL_SIZE_SHIFT) |
                        (HVS_CTL_ORDER_XRGB << HVS_CTL_ORDER_SHIFT) |
                        HVS_CTL_UNITY | HVS_CTL_FORMAT_RGB565);
        hvs_write_dlist(qts, word + 1, 0);
        hvs_write_dlist(qts, word + 2,
                        HVS_CTL2_ALPHA_MODE_FIXED |
                        (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
        hvs_write_dlist(qts, word + 3,
                        (8U << HVS_POS2_HEIGHT_SHIFT) | 8);
        hvs_write_dlist(qts, word + 4, 0xc0c0c0c0);
        hvs_write_dlist(qts, word + 5, HVS_PRIMARY_BASE);
        hvs_write_dlist(qts, word + 6, 0xc0c0c0c0);
        hvs_write_dlist(qts, word + 7, 8 * sizeof(uint16_t));
    }
    hvs_write_dlist(qts, 16 * 8, HVS_CTL_END);
    contents = hvs_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 0, 0, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 7, 7, 248, 0, 0);
    qtest_quit(qts);
}

static void test_pixelvalve_vblank_and_reset(void)
{
    QTestState *qts = display_start();

    qtest_irq_intercept_out_named(qts, PIXELVALVE2_QOM_PATH, "sysbus-irq");
    g_assert_cmphex(qtest_readl(qts, PV_CONTROL), ==, PV_CONTROL_IDLE);
    g_assert_cmphex(qtest_readl(qts, PV_V_CONTROL), ==, PV_V_CONTROL_IDLE);
    g_assert_cmphex(qtest_readl(qts, PV_INTEN), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(qts, 0));

    qtest_writel(qts, PV_INTEN, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, PV_INTEN), ==, PV_INT_MASK);
    qtest_writel(qts, PV_CONTROL,
                 PV_CONTROL_ENABLE | PV_CONTROL_FIFO_CLEAR);
    g_assert_cmphex(qtest_readl(qts, PV_CONTROL), ==, PV_CONTROL_ENABLE);
    qtest_writel(qts, PV_V_CONTROL, PV_V_CONTROL_VIDEO_ENABLE);

    qtest_clock_step(qts, PV_FRAME_PERIOD_NS / 2);

    /* A control write within a frame must not postpone its vblank deadline. */
    qtest_writel(qts, PV_INTEN, PV_INT_MASK);
    qtest_clock_step(qts, PV_FRAME_PERIOD_NS -
                          PV_FRAME_PERIOD_NS / 2 - 1);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_clock_step(qts, 1);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, PV_INT_VFP_START);
    g_assert_true(qtest_get_irq(qts, 0));

    qtest_writel(qts, PV_INTSTAT, PV_INT_VFP_START);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_writel(qts, PV_CONTROL, 0);
    qtest_clock_step(qts, PV_FRAME_PERIOD_NS);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, 0);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, PV_CONTROL), ==, PV_CONTROL_IDLE);
    g_assert_cmphex(qtest_readl(qts, PV_V_CONTROL), ==, PV_V_CONTROL_IDLE);
    g_assert_cmphex(qtest_readl(qts, PV_INTEN), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_quit(qts);
}

static void test_hdmi_transmitter_registers_and_reset(void)
{
    QTestState *qts = display_start();

    g_assert_cmphex(qtest_readl(qts, HDMI_HOTPLUG), ==, 1);
    g_assert_cmphex(qtest_readl(qts, HDMI_FIFO_CTL), ==,
                    HDMI_FIFO_RECENTER_DONE);
    g_assert_cmphex(qtest_readl(qts, HDMI_DVP_CLOCK_STOP), ==, 3);

    qtest_writel(qts, HDMI_HOTPLUG, 0);
    g_assert_cmphex(qtest_readl(qts, HDMI_HOTPLUG), ==, 1);
    qtest_writel(qts, HDMI_FIFO_CTL, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, HDMI_FIFO_CTL), ==,
                    HDMI_FIFO_WRITE_MASK);

    qtest_writel(qts, HDMI_RAM_PACKET_CONFIG, BIT(16) | 0x1234);
    g_assert_cmphex(qtest_readl(qts, HDMI_RAM_PACKET_STATUS), ==, 0x1234);
    qtest_writel(qts, HDMI_RAM_PACKET_STATUS, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, HDMI_RAM_PACKET_STATUS), ==, 0x1234);

    qtest_writel(qts, HDMI_SCHEDULER_CONTROL,
                 HDMI_SCHEDULER_MODE_HDMI);
    g_assert_cmphex(qtest_readl(qts, HDMI_SCHEDULER_CONTROL), ==,
                    HDMI_SCHEDULER_MODE_HDMI |
                    HDMI_SCHEDULER_HDMI_ACTIVE);
    qtest_writel(qts, HDMI0_PHY_BASE, 0x89abcdef);
    g_assert_cmphex(qtest_readl(qts, HDMI0_PHY_BASE), ==, 0x89abcdef);

    /* HDMI0 is reset by DVP software-reset output zero. */
    qtest_writel(qts, DVP_SW_INIT, BIT(0));
    g_assert_cmphex(qtest_readl(qts, HDMI_FIFO_CTL), ==,
                    HDMI_FIFO_RECENTER_DONE);
    g_assert_cmphex(qtest_readl(qts, HDMI_RAM_PACKET_CONFIG), ==, 0);
    g_assert_cmphex(qtest_readl(qts, HDMI_SCHEDULER_CONTROL), ==, 0);
    g_assert_cmphex(qtest_readl(qts, HDMI0_PHY_BASE), ==, 0);
    g_assert_cmphex(qtest_readl(qts, HDMI_DVP_CLOCK_STOP), ==, 3);
    qtest_writel(qts, DVP_SW_INIT, 0);

    qtest_writel(qts, HDMI0_PHY_BASE, 0x10203040);
    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, HDMI_HOTPLUG), ==, 1);
    g_assert_cmphex(qtest_readl(qts, HDMI_FIFO_CTL), ==,
                    HDMI_FIFO_RECENTER_DONE);
    g_assert_cmphex(qtest_readl(qts, HDMI0_PHY_BASE), ==, 0);
    qtest_quit(qts);
}

static void test_hdmi_mai_audio_fifo(void)
{
    QTestState *qts = display_start();
    uint32_t control = (2U << HDMI_MAI_CTL_CHNUM_SHIFT) |
                       HDMI_MAI_CTL_WHOLSMP |
                       HDMI_MAI_CTL_CHALIGN |
                       HDMI_MAI_CTL_ENABLE;
    uint32_t threshold = (6U << 8) | 4;

    qtest_irq_intercept_out_named(qts, HDMI0_QOM_PATH, "audio-dreq");
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    HDMI_MAI_CTL_EMPTY);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_THR), ==, 0);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_FMT), ==, 0);
    g_assert_false(qtest_get_irq(qts, 0));

    /* DVP clock enable makes an enabled, formatted MAI stream run. */
    qtest_writel(qts, DVP_MISC_CONFIG, 0);
    qtest_writel(qts, HDMI_MAI_THR, threshold);
    qtest_writel(qts, HDMI_MAI_FMT,
                 HDMI_MAI_FMT_PCM | HDMI_MAI_FMT_48000);
    qtest_writel(qts, HDMI_MAI_CTL, control);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY);
    g_assert_true(qtest_get_irq(qts, 0));

    for (unsigned int word = 0; word < 6; word++) {
        qtest_writel(qts, HDMI_MAI_DATA, (word + 1) << 4);
    }
    g_assert_false(qtest_get_irq(qts, 0));
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY);

    qtest_clock_step(qts, HDMI_MAI_FRAME_NS);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_clock_step(qts, HDMI_MAI_FRAME_NS);
    g_assert_true(qtest_get_irq(qts, 0));

    /* The final complete frame drains, then the next frame underflows. */
    qtest_clock_step(qts, HDMI_MAI_FRAME_NS);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY);
    qtest_clock_step(qts, HDMI_MAI_FRAME_NS);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY |
                    HDMI_MAI_CTL_DLATE | HDMI_MAI_CTL_ERRORE);
    qtest_writel(qts, HDMI_MAI_CTL,
                 control | HDMI_MAI_CTL_DLATE | HDMI_MAI_CTL_ERRORE);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY);

    /* Stop consumption, fill the FIFO and prove overflow and flush state. */
    qtest_writel(qts, HDMI_MAI_CTL, 0);
    for (unsigned int word = 0; word < HDMI_MAI_FIFO_WORDS; word++) {
        qtest_writel(qts, HDMI_MAI_DATA, word << 4);
    }
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    HDMI_MAI_CTL_FULL);
    qtest_writel(qts, HDMI_MAI_DATA, 0x12345670);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    HDMI_MAI_CTL_FULL | HDMI_MAI_CTL_ERRORF);
    qtest_writel(qts, HDMI_MAI_CTL,
                 HDMI_MAI_CTL_FLUSH | HDMI_MAI_CTL_ERRORF);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    HDMI_MAI_CTL_EMPTY);

    /* A late timer callback catches up every elapsed stereo frame. */
    qtest_writel(qts, HDMI_MAI_CTL, control);
    for (unsigned int word = 0; word < 8; word++) {
        qtest_writel(qts, HDMI_MAI_DATA, word << 4);
    }
    qtest_clock_step(qts, 4 * HDMI_MAI_FRAME_NS);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY);

    qtest_writel(qts, HDMI_MAI_CTL, control | HDMI_MAI_CTL_RESET);
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    control | HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_EMPTY);
    qtest_writel(qts, DVP_SW_INIT, BIT(0));
    g_assert_cmphex(qtest_readl(qts, HDMI_MAI_CTL), ==,
                    HDMI_MAI_CTL_EMPTY);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_quit(qts);
}

static void test_hdmi_mai_audio_dma(void)
{
    QTestState *qts = display_start();
    const unsigned int words = 16;
    uint32_t control = (2U << HDMI_MAI_CTL_CHNUM_SHIFT) |
                       HDMI_MAI_CTL_WHOLSMP |
                       HDMI_MAI_CTL_CHALIGN |
                       HDMI_MAI_CTL_ENABLE;
    uint32_t dma_cs;

    qtest_writel(qts, DMA_CB,
                 DMA_S_INC | DMA_D_DREQ | DMA_PERMAP(10));
    qtest_writel(qts, DMA_CB + 4, DMA_SOURCE);
    qtest_writel(qts, DMA_CB + 8, HDMI_MAI_DATA_BUS);
    qtest_writel(qts, DMA_CB + 12, words * sizeof(uint32_t));
    qtest_writel(qts, DMA_CB + 16, 0);
    qtest_writel(qts, DMA_CB + 20, 0);
    for (unsigned int word = 0; word < words; word++) {
        qtest_writel(qts, DMA_SOURCE + word * sizeof(uint32_t),
                     (word + 1) << 4);
    }

    qtest_writel(qts, DVP_MISC_CONFIG, 0);
    qtest_writel(qts, HDMI_MAI_THR, (8U << 8) | 8);
    qtest_writel(qts, HDMI_MAI_FMT,
                 HDMI_MAI_FMT_PCM | HDMI_MAI_FMT_48000);
    qtest_writel(qts, HDMI_MAI_CTL, control);
    qtest_writel(qts, DMA_ADDR, DMA_CB);
    qtest_writel(qts, DMA_CS, DMA_ACTIVE);

    /* DREQ10 fills eight words, then holds the remaining source data. */
    g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==,
                    8 * sizeof(uint32_t));
    dma_cs = qtest_readl(qts, DMA_CS);
    g_assert_true(dma_cs & DMA_ACTIVE);
    g_assert_true(dma_cs & DMA_ISHELD);

    for (unsigned int frame = 1; frame <= 4; frame++) {
        qtest_clock_step(qts, HDMI_MAI_FRAME_NS);
        g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==,
                        (8 - frame * 2) * sizeof(uint32_t));
    }
    dma_cs = qtest_readl(qts, DMA_CS);
    g_assert_false(dma_cs & DMA_ACTIVE);
    g_assert_true(dma_cs & DMA_END);
    qtest_quit(qts);
}

static void ddc_release(QTestState *qts, uint64_t auto_i2c_base)
{
    qtest_writel(qts, AUTO_I2C_CONTROL0(auto_i2c_base),
                 AUTO_I2C_RELEASE_BSC);
    g_assert_cmphex(qtest_readl(qts,
                               AUTO_I2C_CONTROL0(auto_i2c_base)), ==, 0);
}

static uint32_t ddc_transfer(QTestState *qts, uint64_t base,
                             uint8_t address, bool read,
                             unsigned int length, uint32_t conditions)
{
    qtest_writel(qts, BSC_CHIP_ADDRESS(base), address);
    qtest_writel(qts, BSC_CONTROL(base), read ? BSC_CONTROL_READ : 0);
    qtest_writel(qts, BSC_COUNT(base), length);
    qtest_writel(qts, BSC_IIC_ENABLE(base),
                 conditions | BSC_IIC_TRANSFER);
    return qtest_readl(qts, BSC_IIC_ENABLE(base));
}

static void ddc_clear_command(QTestState *qts, uint64_t base)
{
    qtest_writel(qts, BSC_COUNT(base), 0);
    qtest_writel(qts, BSC_IIC_ENABLE(base), 0);
}

static void ddc_set_pointer(QTestState *qts, uint64_t base, uint8_t pointer,
                            bool keep_bus)
{
    uint32_t conditions = BSC_IIC_RESTART;
    uint32_t status;

    if (keep_bus) {
        conditions |= BSC_IIC_NO_STOP;
    }
    qtest_writel(qts, BSC_DATA_IN(base, 0), pointer);
    status = ddc_transfer(qts, base, EDID_ADDRESS_WRITE, false, 1,
                          conditions);
    g_assert_cmphex(status, ==, conditions | BSC_IIC_INTERRUPT);
    ddc_clear_command(qts, base);
}

static void ddc_read_edid(QTestState *qts, uint8_t edid[EDID_LENGTH])
{
    ddc_release(qts, HDMI0_AUTO_I2C_BASE);
    ddc_set_pointer(qts, HDMI0_DDC_BASE, 0, true);

    for (unsigned int chunk = 0; chunk <
         EDID_LENGTH / BSC_TRANSFER_LENGTH; chunk++) {
        uint32_t conditions;
        uint32_t status;

        if (chunk == 0) {
            conditions = BSC_IIC_NO_STOP;
        } else if (chunk == EDID_LENGTH / BSC_TRANSFER_LENGTH - 1) {
            conditions = BSC_IIC_NO_START;
        } else {
            conditions = BSC_IIC_NO_START | BSC_IIC_NO_STOP;
        }

        status = ddc_transfer(qts, HDMI0_DDC_BASE, EDID_ADDRESS_READ,
                              true, BSC_TRANSFER_LENGTH, conditions);
        g_assert_cmphex(status, ==, conditions | BSC_IIC_INTERRUPT);
        for (unsigned int word = 0; word < BSC_TRANSFER_LENGTH / 4; word++) {
            uint32_t value = qtest_readl(
                qts, BSC_DATA_OUT(HDMI0_DDC_BASE, word));

            for (unsigned int byte = 0; byte < 4; byte++) {
                edid[chunk * BSC_TRANSFER_LENGTH + word * 4 + byte] =
                    extract32(value, byte * 8, 8);
            }
        }
        ddc_clear_command(qts, HDMI0_DDC_BASE);
    }
}

static const uint8_t *edid_cta_data_block(const uint8_t edid[EDID_LENGTH],
                                          unsigned int tag)
{
    const uint8_t *cta = edid + EDID_BLOCK_LENGTH;
    unsigned int end = cta[2];

    g_assert_cmpuint(end, >, 4);
    g_assert_cmpuint(end, <, EDID_BLOCK_LENGTH);
    for (unsigned int offset = 4; offset < end; ) {
        unsigned int length = cta[offset] & 0x1f;

        g_assert_cmpuint(offset + length + 1, <=, end);
        if (cta[offset] >> 5 == tag) {
            return cta + offset;
        }
        offset += length + 1;
    }
    return NULL;
}

static void test_dvp_reset_and_controls(void)
{
    QTestState *qts = display_start();

    /* Firmware-configured idle values captured on the project's Pi 400. */
    g_assert_cmphex(qtest_readl(qts, DVP_CONTROL), ==, 0x00000200);
    g_assert_cmphex(qtest_readl(qts, DVP_SW_INIT), ==, 0);
    g_assert_cmphex(qtest_readl(qts, DVP_MISC_CONFIG), ==,
                    DVP_MISC_CONFIG_MASK);
    g_assert_cmphex(qtest_readl(qts, DVP_SPARE), ==, 0xffff0000);

    qtest_writel(qts, DVP_SW_INIT, UINT32_MAX);
    qtest_writel(qts, DVP_MISC_CONFIG, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, DVP_SW_INIT), ==, DVP_SW_INIT_MASK);
    g_assert_cmphex(qtest_readl(qts, DVP_MISC_CONFIG), ==,
                    DVP_MISC_CONFIG_MASK);

    qtest_writel(qts, DVP_CONTROL, 0);
    qtest_writel(qts, DVP_SPARE, 0);
    g_assert_cmphex(qtest_readl(qts, DVP_CONTROL), ==, 0x00000200);
    g_assert_cmphex(qtest_readl(qts, DVP_SPARE), ==, 0xffff0000);

    qtest_writel(qts, DVP_SW_INIT, BIT(1) | BIT(4));
    qtest_writel(qts, DVP_MISC_CONFIG, 0);
    g_assert_cmphex(qtest_readl(qts, DVP_SW_INIT), ==, BIT(1) | BIT(4));
    g_assert_cmphex(qtest_readl(qts, DVP_MISC_CONFIG), ==, 0);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, DVP_SW_INIT), ==, 0);
    g_assert_cmphex(qtest_readl(qts, DVP_MISC_CONFIG), ==,
                    DVP_MISC_CONFIG_MASK);
    qtest_quit(qts);
}

static void test_ddc_registers_reset_and_nack(void)
{
    QTestState *qts = display_start();
    uint32_t status;

    g_assert_cmphex(qtest_readl(qts, BSC_CHIP_ADDRESS(HDMI0_DDC_BASE)),
                    ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL(HDMI0_DDC_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_IIC_ENABLE(HDMI0_DDC_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL_HIGH(HDMI0_DDC_BASE)),
                    ==, 0);

    /* Auto-I2C owns the engine until Linux performs the release write. */
    status = ddc_transfer(qts, HDMI0_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, 0, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT | BSC_IIC_NO_ACK);
    ddc_clear_command(qts, HDMI0_DDC_BASE);

    ddc_release(qts, HDMI1_AUTO_I2C_BASE);
    status = ddc_transfer(qts, HDMI1_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, 0, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT | BSC_IIC_NO_ACK);
    ddc_clear_command(qts, HDMI1_DDC_BASE);

    /* IGNORE_ACK suppresses the status bit used by no-ACK commands. */
    qtest_writel(qts, BSC_CONTROL_HIGH(HDMI1_DDC_BASE),
                 BSC_CONTROL_HIGH_IGNORE_ACK);
    status = ddc_transfer(qts, HDMI1_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, 0, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT);

    qtest_writel(qts, BSC_CHIP_ADDRESS(HDMI0_DDC_BASE), UINT32_MAX);
    qtest_writel(qts, BSC_DATA_IN(HDMI0_DDC_BASE, 7), 0x12345678);
    qtest_writel(qts, BSC_COUNT(HDMI0_DDC_BASE), UINT32_MAX);
    qtest_writel(qts, BSC_CONTROL(HDMI0_DDC_BASE), UINT32_MAX);
    qtest_writel(qts, BSC_CONTROL_HIGH(HDMI0_DDC_BASE), UINT32_MAX);
    qtest_writel(qts, BSC_SCL_PARAM(HDMI0_DDC_BASE), 0x89abcdef);
    g_assert_cmphex(qtest_readl(qts, BSC_CHIP_ADDRESS(HDMI0_DDC_BASE)),
                    ==, 0xff);
    g_assert_cmphex(qtest_readl(qts, BSC_DATA_IN(HDMI0_DDC_BASE, 7)),
                    ==, 0x12345678);
    g_assert_cmphex(qtest_readl(qts, BSC_COUNT(HDMI0_DDC_BASE)), ==, 0x3f);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL(HDMI0_DDC_BASE)), ==, 0xf3);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL_HIGH(HDMI0_DDC_BASE)),
                    ==, 0xc3);
    g_assert_cmphex(qtest_readl(qts, BSC_SCL_PARAM(HDMI0_DDC_BASE)),
                    ==, 0x89abcdef);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, BSC_CHIP_ADDRESS(HDMI0_DDC_BASE)),
                    ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_DATA_IN(HDMI0_DDC_BASE, 7)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_COUNT(HDMI0_DDC_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL(HDMI0_DDC_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_CONTROL_HIGH(HDMI0_DDC_BASE)),
                    ==, 0);
    g_assert_cmphex(qtest_readl(qts, BSC_SCL_PARAM(HDMI0_DDC_BASE)), ==, 0);

    /* Reset returns ownership to auto-I2C. */
    status = ddc_transfer(qts, HDMI0_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, 0, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT | BSC_IIC_NO_ACK);

    /* A malformed command cannot strand an earlier no-STOP transaction. */
    ddc_release(qts, HDMI0_AUTO_I2C_BASE);
    ddc_set_pointer(qts, HDMI0_DDC_BASE, 0, true);
    status = ddc_transfer(qts, HDMI0_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, BSC_TRANSFER_LENGTH + 1, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT | BSC_IIC_NO_ACK);
    ddc_clear_command(qts, HDMI0_DDC_BASE);
    status = ddc_transfer(qts, HDMI0_DDC_BASE, EDID_ADDRESS_WRITE,
                          false, 0, BSC_IIC_NO_START);
    g_assert_cmphex(status, ==, BSC_IIC_NO_START | BSC_IIC_INTERRUPT |
                               BSC_IIC_NO_ACK);
    qtest_quit(qts);
}

static void test_ddc_edid(void)
{
    static const uint8_t edid_header[] = {
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    };
    static const uint8_t stereo_lpcm[] = { 0x09, 0x07, 0x07 };
    static const uint8_t stereo_speakers[] = { 0x01, 0x00, 0x00 };
    static const uint8_t hdmi_vsdb[] = {
        0x03, 0x0c, 0x00, 0x10, 0x00, 0x00, 0x21,
    };
    QTestState *qts = display_start();
    const uint8_t *audio;
    const uint8_t *speaker;
    const uint8_t *video;
    const uint8_t *vendor;
    uint8_t edid[EDID_LENGTH];

    ddc_read_edid(qts, edid);
    g_assert_cmpmem(edid, sizeof(edid_header),
                    edid_header, sizeof(edid_header));
    g_assert_cmphex(edid[20], ==, 0xa2);
    g_assert_cmphex(edid[EDID_EXTENSION_COUNT], ==, 1);
    for (unsigned int block = 0; block < 2; block++) {
        unsigned int checksum = 0;

        for (unsigned int i = 0; i < EDID_BLOCK_LENGTH; i++) {
            checksum += edid[block * EDID_BLOCK_LENGTH + i];
        }
        g_assert_cmphex(checksum & 0xff, ==, 0);
    }

    g_assert_cmphex(edid[EDID_BLOCK_LENGTH], ==, CTA_EXTENSION_TAG);
    g_assert_cmphex(edid[EDID_BLOCK_LENGTH + 1], ==, CTA_REVISION);
    g_assert_true(edid[EDID_BLOCK_LENGTH + 3] & CTA_BASIC_AUDIO);

    video = edid_cta_data_block(edid, CTA_DB_VIDEO);
    audio = edid_cta_data_block(edid, CTA_DB_AUDIO);
    speaker = edid_cta_data_block(edid, CTA_DB_SPEAKER);
    vendor = edid_cta_data_block(edid, CTA_DB_VENDOR);
    g_assert_nonnull(video);
    g_assert_nonnull(audio);
    g_assert_nonnull(speaker);
    g_assert_nonnull(vendor);
    g_assert_cmpuint(audio[0] & 0x1f, ==, sizeof(stereo_lpcm));
    g_assert_cmpmem(audio + 1, sizeof(stereo_lpcm),
                    stereo_lpcm, sizeof(stereo_lpcm));
    g_assert_cmpuint(speaker[0] & 0x1f, ==, sizeof(stereo_speakers));
    g_assert_cmpmem(speaker + 1, sizeof(stereo_speakers),
                    stereo_speakers, sizeof(stereo_speakers));
    g_assert_cmpuint(vendor[0] & 0x1f, ==, sizeof(hdmi_vsdb));
    g_assert_cmpmem(vendor + 1, sizeof(hdmi_vsdb),
                    hdmi_vsdb, sizeof(hdmi_vsdb));
    qtest_quit(qts);
}

#ifndef _WIN32
static void wait_for_migration(QTestState *qts)
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
    g_error("timed out waiting for BCM2711 display migration");
}

static void test_display_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    uint8_t edid[EDID_LENGTH];
    size_t length;
    uint32_t status;
    int64_t mai_deadline_ns;
    const uint32_t hvs_control =
        HVS_DISPCTRL_ENABLE | (8U << 16) | 8;
    const uint32_t mai_control =
        (2U << HDMI_MAI_CTL_CHNUM_SHIFT) |
        HDMI_MAI_CTL_WHOLSMP |
        HDMI_MAI_CTL_CHALIGN |
        HDMI_MAI_CTL_ENABLE;
    const int64_t pixelvalve_elapsed_ns = 5 * 1000 * 1000;

    ddc_read_edid(source, edid);
    qtest_writel(source, DVP_SW_INIT, BIT(1) | BIT(5));
    qtest_writel(source, DVP_MISC_CONFIG, 0);

    hvs_program_scaled_rgb565(source);
    qtest_writel(source, HVS_DISPLIST0, 0);
    qtest_writel(source, HVS_DISPCTRL0, hvs_control);

    qtest_writel(source, PV_INTEN, PV_INT_VFP_START);
    qtest_writel(source, PV_CONTROL, PV_CONTROL_ENABLE);
    qtest_writel(source, PV_V_CONTROL, PV_V_CONTROL_VIDEO_ENABLE);
    qtest_clock_step(source, pixelvalve_elapsed_ns);

    qtest_writel(source, HDMI_RAM_PACKET_CONFIG, BIT(16) | 0x55aa);
    qtest_writel(source, HDMI_SCHEDULER_CONTROL,
                 HDMI_SCHEDULER_MODE_HDMI);
    qtest_writel(source, HDMI0_PHY_BASE, 0x89abcdef);

    /*
     * Migrate a live stereo MAI stream with two frames queued and a DMA
     * transfer held at the DREQ high-water mark.  The first sample after
     * resume must consume two words and make DREQ10 refill exactly one frame.
     */
    qtest_writel(source, HDMI_MAI_THR, (4U << 8) | 4);
    qtest_writel(source, HDMI_MAI_FMT,
                 HDMI_MAI_FMT_PCM | HDMI_MAI_FMT_48000);
    qtest_writel(source, HDMI_MAI_CTL, mai_control);
    for (unsigned int word = 0; word < 4; word++) {
        qtest_writel(source, HDMI_MAI_DATA, (word + 1) << 4);
    }
    qtest_writel(source, DMA_CB,
                 DMA_S_INC | DMA_D_DREQ | DMA_PERMAP(10));
    qtest_writel(source, DMA_CB + 4, DMA_SOURCE);
    qtest_writel(source, DMA_CB + 8, HDMI_MAI_DATA_BUS);
    qtest_writel(source, DMA_CB + 12, 4 * sizeof(uint32_t));
    qtest_writel(source, DMA_CB + 16, 0);
    qtest_writel(source, DMA_CB + 20, 0);
    for (unsigned int word = 0; word < 4; word++) {
        qtest_writel(source, DMA_SOURCE + word * sizeof(uint32_t),
                     (word + 5) << 4);
    }
    qtest_writel(source, DMA_ADDR, DMA_CB);
    qtest_writel(source, DMA_CS, DMA_ACTIVE);
    g_assert_cmphex(qtest_readl(source, DMA_TXFR_LEN), ==,
                    4 * sizeof(uint32_t));
    g_assert_true(qtest_readl(source, DMA_CS) & DMA_ISHELD);

    /* Leave the EDID pointer and an open repeated-start transaction live. */
    ddc_set_pointer(source, HDMI0_DDC_BASE, 0x10, true);
    g_assert_cmphex(qtest_readl(source, BSC_DATA_IN(HDMI0_DDC_BASE, 0)),
                    ==, 0x10);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2711-display-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);
    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    wait_for_migration(source);

    destination_args = g_strdup_printf(
        "-machine raspi4b -nic none -incoming %s", uri);
    destination = qtest_init(destination_args);
    wait_for_migration(destination);

    g_assert_cmphex(qtest_readl(destination, DVP_SW_INIT), ==,
                    BIT(1) | BIT(5));
    g_assert_cmphex(qtest_readl(destination, DVP_MISC_CONFIG), ==, 0);
    g_assert_cmphex(qtest_readl(destination,
                               HVS_DLIST_BASE + 17 * sizeof(uint32_t)),
                    ==, HVS_CTL_END);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPLIST0), ==, 0);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPLACT0), ==, 0);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPCTRL0), ==,
                    hvs_control);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPSTAT0), ==,
                    HVS_DISPSTAT_RUN);
    contents = hvs_screendump(destination, &length);
    hvs_assert_ppm_pixel(contents, length, 0, 0, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 252, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 248);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 252, 248);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_CONFIG), ==,
                    BIT(16) | 0x55aa);
    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_STATUS), ==,
                    0x55aa);
    g_assert_cmphex(qtest_readl(destination, HDMI_SCHEDULER_CONTROL), ==,
                    HDMI_SCHEDULER_MODE_HDMI |
                    HDMI_SCHEDULER_HDMI_ACTIVE);
    g_assert_cmphex(qtest_readl(destination, HDMI0_PHY_BASE), ==,
                    0x89abcdef);
    g_assert_cmphex(qtest_readl(destination, HDMI_MAI_THR), ==,
                    (4U << 8) | 4);
    g_assert_cmphex(qtest_readl(destination, HDMI_MAI_FMT), ==,
                    HDMI_MAI_FMT_PCM | HDMI_MAI_FMT_48000);
    g_assert_cmphex(qtest_readl(destination, HDMI_MAI_CTL), ==,
                    mai_control | HDMI_MAI_CTL_BUSY);
    g_assert_cmphex(qtest_readl(destination, DMA_TXFR_LEN), ==,
                    4 * sizeof(uint32_t));
    g_assert_true(qtest_readl(destination, DMA_CS) & DMA_ISHELD);

    qtest_irq_intercept_out_named(destination, PIXELVALVE2_QOM_PATH,
                                  "sysbus-irq");
    g_assert_cmphex(qtest_readl(destination, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(destination, 0));
    qtest_qmp_assert_success(destination, "{ 'execute': 'cont' }");

    /* The migrated MAI deadline precedes the pixel-valve vblank deadline. */
    mai_deadline_ns = qtest_clock_step_next(destination);
    g_assert_cmpint(mai_deadline_ns, ==,
                    pixelvalve_elapsed_ns + HDMI_MAI_FIRST_FRAME_NS);
    g_assert_cmphex(qtest_readl(destination, DMA_TXFR_LEN), ==,
                    2 * sizeof(uint32_t));
    g_assert_true(qtest_readl(destination, DMA_CS) & DMA_ISHELD);
    g_assert_cmphex(qtest_readl(destination, HDMI_MAI_CTL), ==,
                    mai_control | HDMI_MAI_CTL_BUSY);
    g_assert_cmphex(qtest_readl(destination, PV_INTSTAT), ==, 0);

    /* Stop MAI, then advance exactly to the migrated vblank deadline. */
    qtest_writel(destination, HDMI_MAI_CTL, 0);
    g_assert_cmpint(qtest_clock_set(destination, PV_FRAME_PERIOD_NS), ==,
                    PV_FRAME_PERIOD_NS);
    g_assert_cmphex(qtest_readl(destination, PV_INTSTAT), ==,
                    PV_INT_VFP_START);
    g_assert_true(qtest_get_irq(destination, 0));
    qtest_writel(destination, PV_INTSTAT, PV_INT_VFP_START);
    g_assert_false(qtest_get_irq(destination, 0));
    g_assert_cmphex(qtest_readl(destination,
                               BSC_DATA_IN(HDMI0_DDC_BASE, 0)), ==, 0x10);

    /* The migrated bus session and EDID cursor remain usable immediately. */
    status = ddc_transfer(destination, HDMI0_DDC_BASE,
                          EDID_ADDRESS_READ, true, 1, 0);
    g_assert_cmphex(status, ==, BSC_IIC_INTERRUPT);
    g_assert_cmphex(qtest_readl(destination,
                               BSC_DATA_OUT(HDMI0_DDC_BASE, 0)) & 0xff,
                    ==, edid[0x10]);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2711/display/hvs", test_hvs_registers_and_reset);
    qtest_add_func("/bcm2711/display/hvs/scaled_composition",
                   test_hvs_scaled_composition);
    qtest_add_func("/bcm2711/display/pixelvalve/vblank",
                   test_pixelvalve_vblank_and_reset);
    qtest_add_func("/bcm2711/display/hdmi/transmitter",
                   test_hdmi_transmitter_registers_and_reset);
    qtest_add_func("/bcm2711/display/hdmi/mai_audio_fifo",
                   test_hdmi_mai_audio_fifo);
    qtest_add_func("/bcm2711/display/hdmi/mai_audio_dma",
                   test_hdmi_mai_audio_dma);
    qtest_add_func("/bcm2711/display/dvp", test_dvp_reset_and_controls);
    qtest_add_func("/bcm2711/display/ddc/registers_reset_and_nack",
                   test_ddc_registers_reset_and_nack);
    qtest_add_func("/bcm2711/display/ddc/edid", test_ddc_edid);
#ifndef _WIN32
    qtest_add_func("/bcm2711/display/migration", test_display_migration);
#endif
    return g_test_run();
}
