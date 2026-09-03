/*
 * QTests for the BCM2711 native display and HDMI service-plane blocks.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/bswap.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "libqtest.h"
#include "qobject/qdict.h"

#define MBOX_BASE                   0xfe00b800
#define MBOX_READ                   (MBOX_BASE + 0x80)
#define MBOX_WRITE                  (MBOX_BASE + 0xa0)
#define PROPERTY_BUFFER             0x1000

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
#define HVS_CTL_TILING_SHIFT        20
#define HVS_CTL_TILING_T            3
#define HVS_CTL_ORDER_SHIFT         13
#define HVS_CTL_SCL0_SHIFT          5
#define HVS_CTL_SCL_H_TPZ_V_TPZ     3
#define HVS_CTL_SCL_H_NONE_V_TPZ    6
#define HVS_CTL_SCL_H_TPZ_V_NONE    7
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
#define HVS_TILED_RGB565_BASE       0x01200000
#define HVS_TILED_RGBA8888_BASE     0x01300000
#define HVS_TILED_HEIGHT            64

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

static void framebuffer_property_request(QTestState *qts, uint32_t tag,
                                         const uint32_t *payload,
                                         size_t words, size_t response_bytes)
{
    uint32_t payload_bytes = words * sizeof(*payload);
    uint32_t total_bytes = 24 + payload_bytes;

    qtest_writel(qts, PROPERTY_BUFFER, total_bytes);
    qtest_writel(qts, PROPERTY_BUFFER + 4, 0);
    qtest_writel(qts, PROPERTY_BUFFER + 8, tag);
    qtest_writel(qts, PROPERTY_BUFFER + 12, payload_bytes);
    qtest_writel(qts, PROPERTY_BUFFER + 16, 0);
    for (size_t i = 0; i < words; i++) {
        qtest_writel(qts, PROPERTY_BUFFER + 20 + i * sizeof(uint32_t),
                     payload[i]);
    }
    qtest_writel(qts, PROPERTY_BUFFER + 20 + payload_bytes,
                 RPI_FWREQ_PROPERTY_END);

    qtest_writel(qts, MBOX_WRITE, PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(qtest_readl(qts, MBOX_READ), ==,
                    PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(qtest_readl(qts, PROPERTY_BUFFER + 4), ==, 0x80000000);
    g_assert_cmphex(qtest_readl(qts, PROPERTY_BUFFER + 16), ==,
                    0x80000000 | response_bytes);
}

static bool hdmi_get_connected(QTestState *qts)
{
    g_autoptr(QDict) response = qtest_qmp(
        qts, "{ 'execute': 'qom-get',"
        "  'arguments': { 'path': %s, 'property': 'connected' } }",
        HDMI0_QOM_PATH);

    g_assert_false(qdict_haskey(response, "error"));
    return qdict_get_bool(response, "return");
}

static void hdmi_set_connected(QTestState *qts, bool value)
{
    qtest_qmp_assert_success(qts,
        "{ 'execute': 'qom-set',"
        "  'arguments': { 'path': %s, 'property': 'connected',"
        "                 'value': %i } }",
        HDMI0_QOM_PATH, value);
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

static char *display_screendump(QTestState *qts, size_t *length)
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

static void hvs_assert_ppm_pixel_close(const char *contents, size_t length,
                                       unsigned int x, unsigned int y,
                                       uint8_t red, uint8_t green,
                                       uint8_t blue, uint8_t tolerance)
{
    static const char header[] = "P6\n8 8\n255\n";
    size_t offset = sizeof(header) - 1 + ((size_t)y * 8 + x) * 3;

    g_assert_cmpuint(length, ==, sizeof(header) - 1 + 8 * 8 * 3);
    g_assert_cmpmem(contents, sizeof(header) - 1,
                    header, sizeof(header) - 1);
    g_assert_cmpint((int)(uint8_t)contents[offset] - red, >=, -tolerance);
    g_assert_cmpint((int)(uint8_t)contents[offset] - red, <=, tolerance);
    g_assert_cmpint((int)(uint8_t)contents[offset + 1] - green,
                    >=, -tolerance);
    g_assert_cmpint((int)(uint8_t)contents[offset + 1] - green,
                    <=, tolerance);
    g_assert_cmpint((int)(uint8_t)contents[offset + 2] - blue,
                    >=, -tolerance);
    g_assert_cmpint((int)(uint8_t)contents[offset + 2] - blue,
                    <=, tolerance);
}

static void hvs_assert_ppm_pixel_4x4(const char *contents, size_t length,
                                      unsigned int x, unsigned int y,
                                      uint8_t red, uint8_t green,
                                      uint8_t blue)
{
    static const char header[] = "P6\n4 4\n255\n";
    size_t offset = sizeof(header) - 1 + ((size_t)y * 4 + x) * 3;

    g_assert_cmpuint(length, ==, sizeof(header) - 1 + 4 * 4 * 3);
    g_assert_cmpmem(contents, sizeof(header) - 1,
                    header, sizeof(header) - 1);
    g_assert_cmphex((uint8_t)contents[offset], ==, red);
    g_assert_cmphex((uint8_t)contents[offset + 1], ==, green);
    g_assert_cmphex((uint8_t)contents[offset + 2], ==, blue);
}

static void hvs_assert_ppm_pixel_sized(const char *contents, size_t length,
                                       unsigned int width, unsigned int height,
                                       unsigned int x, unsigned int y,
                                       uint8_t red, uint8_t green,
                                       uint8_t blue)
{
    char header[64];
    int header_length;
    size_t offset;

    header_length = g_snprintf(header, sizeof(header), "P6\n%u %u\n255\n",
                               width, height);
    g_assert_cmpint(header_length, >, 0);
    g_assert_cmpint(header_length, <, sizeof(header));
    g_assert_cmpuint(x, <, width);
    g_assert_cmpuint(y, <, height);
    g_assert_cmpuint(length, ==, (size_t)header_length +
                     (size_t)width * height * 3);
    g_assert_cmpmem(contents, header_length, header, header_length);
    offset = (size_t)header_length + ((size_t)y * width + x) * 3;
    g_assert_cmphex((uint8_t)contents[offset], ==, red);
    g_assert_cmphex((uint8_t)contents[offset + 1], ==, green);
    g_assert_cmphex((uint8_t)contents[offset + 2], ==, blue);
}

static void assert_ppm_header(const char *contents, size_t length,
                              const char *header)
{
    size_t header_length = strlen(header);

    g_assert_cmpuint(length, >=, header_length);
    g_assert_cmpmem(contents, header_length, header, header_length);
}

static void test_legacy_framebuffer_viewport(void)
{
    static const char header[] = "P6\n4 1\n255\n";
    const uint32_t physical_size[] = { 4, 1 };
    const uint32_t virtual_size[] = { 5, 1 };
    const uint32_t depth[] = { 24 };
    const uint32_t pixel_order[] = { 1 };
    const uint32_t offset[] = { 1, 0 };
    const uint32_t allocate[] = { 16, 0 };
    const uint8_t pixels[] = {
        0x10, 0x20, 0x30,
        0x40, 0x50, 0x60,
        0x70, 0x80, 0x90,
        0xa0, 0xb0, 0xc0,
        0xd0, 0xe0, 0xf0,
    };
    QTestState *qts = display_start();
    g_autofree char *contents = NULL;
    size_t length;
    uint32_t base;

    framebuffer_property_request(
        qts, RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT,
        physical_size, G_N_ELEMENTS(physical_size), sizeof(physical_size));
    framebuffer_property_request(
        qts, RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT,
        virtual_size, G_N_ELEMENTS(virtual_size), sizeof(virtual_size));
    framebuffer_property_request(qts, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH,
                                 depth, G_N_ELEMENTS(depth), sizeof(depth));
    framebuffer_property_request(qts, RPI_FWREQ_FRAMEBUFFER_SET_PIXEL_ORDER,
                                 pixel_order, G_N_ELEMENTS(pixel_order),
                                 sizeof(pixel_order));
    framebuffer_property_request(qts, RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_OFFSET,
                                 offset, G_N_ELEMENTS(offset), sizeof(offset));
    framebuffer_property_request(qts, RPI_FWREQ_FRAMEBUFFER_ALLOCATE,
                                 allocate, G_N_ELEMENTS(allocate),
                                 sizeof(allocate));
    base = qtest_readl(qts, PROPERTY_BUFFER + 20);
    g_assert_cmphex(qtest_readl(qts, PROPERTY_BUFFER + 24), ==,
                    sizeof(pixels));

    qtest_memwrite(qts, base, pixels, sizeof(pixels));
    contents = display_screendump(qts, &length);

    g_assert_cmpuint(length, ==, sizeof(header) - 1 + 4 * 3);
    g_assert_cmpmem(contents, sizeof(header) - 1,
                    header, sizeof(header) - 1);
    g_assert_cmpmem(contents + sizeof(header) - 1, 4 * 3,
                    pixels + 3, 4 * 3);

    qtest_system_reset(qts);
    g_clear_pointer(&contents, g_free);
    contents = display_screendump(qts, &length);
    assert_ppm_header(contents, length, "P6\n640 480\n255\n");
    qtest_quit(qts);
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
    contents = display_screendump(qts, &length);

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
    contents = display_screendump(qts, &length);
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
    contents = display_screendump(qts, &length);
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
    contents = display_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 124, 128, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    /* Plane alpha mixes with per-pixel coverage alpha. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 10,
                    HVS_CTL2_ALPHA_MIX |
                    (0x800U << HVS_CTL2_ALPHA_SHIFT));
    contents = display_screendump(qts, &length);
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
    contents = display_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 124, 128, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 255);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    /* The two reflection flags are independent of scaling. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 9,
                    HVS_POS0_VFLIP | HVS_POS0_HFLIP |
                    (2U << HVS_POS0_Y_SHIFT) | 2);
    contents = display_screendump(qts, &length);
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
    contents = display_screendump(qts, &length);
    hvs_assert_ppm_pixel(contents, length, 0, 0, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 7, 7, 248, 0, 0);
    qtest_quit(qts);
}

/* T tiles contain 8x8 64-byte microtiles, arranged in 4 KiB supertiles. */
#define HVS_T_TILE_BYTES 4096
#define HVS_TILED_RGB565_WIDTH 128
#define HVS_TILED_RGBA8888_WIDTH 64
#define HVS_TILED_COLUMNS 2
#define HVS_TILED_ROWS 2

static size_t hvs_tiled_offset(unsigned int bytes_per_pixel,
                               unsigned int tile_columns,
                               unsigned int x, unsigned int y)
{
    static const uint8_t even_subtile_map[] = { 0, 3, 1, 2 };
    static const uint8_t odd_subtile_map[] = { 2, 1, 3, 0 };
    unsigned int utile_width = bytes_per_pixel == 2 ? 8 : 4;
    unsigned int utile_x = x / utile_width;
    unsigned int utile_y = y / 4;
    unsigned int tile_x = utile_x / 8;
    unsigned int tile_y = utile_y / 8;
    unsigned int physical_tile_x = tile_y & 1 ?
        tile_columns - tile_x - 1 : tile_x;
    unsigned int subtile = ((utile_y >> 2) & 1) * 2 +
                           ((utile_x >> 2) & 1);
    unsigned int subtile_offset =
        (tile_y & 1 ? odd_subtile_map[subtile] : even_subtile_map[subtile]) *
        1024;
    unsigned int utile_offset = ((utile_y & 3) * 4 + (utile_x & 3)) * 64;
    unsigned int pixel_offset = ((y & 3) * utile_width +
                                 (x % utile_width)) * bytes_per_pixel;

    g_assert_cmpuint(tile_x, <, tile_columns);
    return ((size_t)tile_y * tile_columns + physical_tile_x) *
        HVS_T_TILE_BYTES + subtile_offset + utile_offset + pixel_offset;
}

static uint16_t hvs_tiled_rgb565_pixel(unsigned int x, unsigned int y)
{
    uint16_t red = (x * 5 + y * 3) & 0x1f;
    uint16_t green = (x * 11 + y * 7) & 0x3f;
    uint16_t blue = (x * 13 + y * 17) & 0x1f;

    return (red << 11) | (green << 5) | blue;
}

static void hvs_write_tiled_rgb565_buffer(QTestState *qts)
{
    const size_t tiled_size = HVS_TILED_COLUMNS * HVS_TILED_ROWS *
        HVS_T_TILE_BYTES;
    g_autofree uint8_t *tiles = g_malloc0(tiled_size);

    for (unsigned int y = 0; y < HVS_TILED_HEIGHT; y++) {
        for (unsigned int x = 0; x < HVS_TILED_RGB565_WIDTH; x++) {
            size_t offset = hvs_tiled_offset(2, HVS_TILED_COLUMNS, x, y);

            stw_le_p(tiles + offset, hvs_tiled_rgb565_pixel(x, y));
        }
    }
    qtest_memwrite(qts, HVS_TILED_RGB565_BASE, tiles, tiled_size);
}

static void hvs_rgb565_components(uint16_t pixel, uint8_t *red,
                                  uint8_t *green, uint8_t *blue)
{
    *red = ((pixel >> 11) & 0x1f) << 3;
    *green = ((pixel >> 5) & 0x3f) << 2;
    *blue = (pixel & 0x1f) << 3;
}

static void hvs_tiled_rgba8888_components(unsigned int x, unsigned int y,
                                           uint8_t *red, uint8_t *green,
                                           uint8_t *blue)
{
    *red = x * 37 + y * 13;
    *green = x * 17 + y * 29;
    *blue = x * 7 + y * 53;
}

static void hvs_program_tiled_plane(QTestState *qts, uint32_t base,
                                    unsigned int width, unsigned int height,
                                    unsigned int columns, uint32_t format,
                                    uint32_t order, bool hflip)
{
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (8U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_TILING_T << HVS_CTL_TILING_SHIFT) |
                    (order << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_UNITY | format);
    hvs_write_dlist(qts, 1, hflip ? HVS_POS0_HFLIP : 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (height << HVS_POS2_HEIGHT_SHIFT) | width);
    hvs_write_dlist(qts, 4, 0);
    hvs_write_dlist(qts, 5, base);
    hvs_write_dlist(qts, 6, 0);
    hvs_write_dlist(qts, 7, columns);
    hvs_write_dlist(qts, 8, HVS_CTL_END);
}

static void hvs_program_unsupported_scaled_tiled_plane(QTestState *qts)
{
    const unsigned int dlist = 32;

    hvs_write_dlist(qts, dlist,
                    HVS_CTL_VALID | (9U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_TILING_T << HVS_CTL_TILING_SHIFT) |
                    (HVS_CTL_ORDER_XRGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_FORMAT_RGB565);
    hvs_write_dlist(qts, dlist + 1, 0);
    hvs_write_dlist(qts, dlist + 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, dlist + 3,
                    (32U << HVS_POS1_HEIGHT_SHIFT) | 64);
    hvs_write_dlist(qts, dlist + 4,
                    (HVS_TILED_HEIGHT << HVS_POS2_HEIGHT_SHIFT) | 128);
    hvs_write_dlist(qts, dlist + 5, 0);
    hvs_write_dlist(qts, dlist + 6, HVS_TILED_RGB565_BASE);
    hvs_write_dlist(qts, dlist + 7, 0);
    hvs_write_dlist(qts, dlist + 8, 2);
    hvs_write_dlist(qts, dlist + 9, HVS_CTL_END);
}

static void test_hvs_t_tiled_rgb_scanout(void)
{
    static const unsigned int sample_x[] = {
        0, 7, 8, 31, 32, 63, 64, 95, 96, 127,
    };
    static const unsigned int sample_y[] = {
        0, 3, 4, 15, 16, 31, 32, 35, 48, 63,
    };
    const size_t tiled_size = HVS_TILED_COLUMNS * HVS_TILED_ROWS *
        HVS_T_TILE_BYTES;
    QTestState *qts = display_start();
    g_autofree uint8_t *tiles = g_malloc0(tiled_size);
    g_autofree char *contents = NULL;
    size_t length;

    hvs_write_tiled_rgb565_buffer(qts);
    hvs_program_tiled_plane(qts, HVS_TILED_RGB565_BASE,
                            HVS_TILED_RGB565_WIDTH, HVS_TILED_HEIGHT,
                            HVS_TILED_COLUMNS,
                            HVS_CTL_FORMAT_RGB565, HVS_CTL_ORDER_XRGB,
                            false);
    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (HVS_TILED_RGB565_WIDTH << 16) |
                 HVS_TILED_HEIGHT);
    contents = display_screendump(qts, &length);
    for (unsigned int y_index = 0; y_index < G_N_ELEMENTS(sample_y);
         y_index++) {
        for (unsigned int x_index = 0; x_index < G_N_ELEMENTS(sample_x);
             x_index++) {
            uint8_t red, green, blue;
            uint16_t pixel = hvs_tiled_rgb565_pixel(sample_x[x_index],
                                                     sample_y[y_index]);

            hvs_rgb565_components(pixel, &red, &green, &blue);
            hvs_assert_ppm_pixel_sized(contents, length,
                                       HVS_TILED_RGB565_WIDTH,
                                       HVS_TILED_HEIGHT, sample_x[x_index],
                                       sample_y[y_index], red, green, blue);
        }
    }

    /* HVS horizontal reflection is independent of T-tile traversal. */
    g_clear_pointer(&contents, g_free);
    hvs_write_dlist(qts, 1, HVS_POS0_HFLIP);
    contents = display_screendump(qts, &length);
    for (unsigned int y_index = 0; y_index < G_N_ELEMENTS(sample_y);
         y_index++) {
        for (unsigned int x_index = 0; x_index < G_N_ELEMENTS(sample_x);
             x_index++) {
            uint8_t red, green, blue;
            uint16_t pixel = hvs_tiled_rgb565_pixel(
                HVS_TILED_RGB565_WIDTH - 1 - sample_x[x_index],
                sample_y[y_index]);

            hvs_rgb565_components(pixel, &red, &green, &blue);
            hvs_assert_ppm_pixel_sized(contents, length,
                                       HVS_TILED_RGB565_WIDTH,
                                       HVS_TILED_HEIGHT, sample_x[x_index],
                                       sample_y[y_index], red, green, blue);
        }
    }

    /* A scaled T plane must leave the last valid scanout untouched. */
    g_clear_pointer(&contents, g_free);
    hvs_program_unsupported_scaled_tiled_plane(qts);
    qtest_writel(qts, HVS_DISPLIST0, 32);
    contents = display_screendump(qts, &length);
    {
        uint8_t red, green, blue;
        uint16_t pixel = hvs_tiled_rgb565_pixel(
            HVS_TILED_RGB565_WIDTH - 1, 0);

        hvs_rgb565_components(pixel, &red, &green, &blue);
        hvs_assert_ppm_pixel_sized(contents, length,
                                   HVS_TILED_RGB565_WIDTH,
                                   HVS_TILED_HEIGHT, 0, 0,
                                   red, green, blue);
    }

    /* 32bpp T tiles have a narrower 32-pixel 4 KiB tile. */
    g_clear_pointer(&contents, g_free);
    memset(tiles, 0, tiled_size);
    for (unsigned int y = 0; y < HVS_TILED_HEIGHT; y++) {
        for (unsigned int x = 0; x < HVS_TILED_RGBA8888_WIDTH; x++) {
            uint8_t red, green, blue;
            size_t offset = hvs_tiled_offset(4, HVS_TILED_COLUMNS, x, y);

            hvs_tiled_rgba8888_components(x, y, &red, &green, &blue);
            /* Guest little-endian ARGB is stored B, G, R, A. */
            tiles[offset] = blue;
            tiles[offset + 1] = green;
            tiles[offset + 2] = red;
            tiles[offset + 3] = 0xff;
        }
    }
    qtest_memwrite(qts, HVS_TILED_RGBA8888_BASE, tiles, tiled_size);
    hvs_program_tiled_plane(qts, HVS_TILED_RGBA8888_BASE,
                            HVS_TILED_RGBA8888_WIDTH, HVS_TILED_HEIGHT,
                            HVS_TILED_COLUMNS,
                            HVS_CTL_FORMAT_RGBA8888, HVS_CTL_ORDER_ARGB,
                            false);
    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (HVS_TILED_RGBA8888_WIDTH << 16) |
                 HVS_TILED_HEIGHT);
    contents = display_screendump(qts, &length);
    for (unsigned int y_index = 0; y_index < G_N_ELEMENTS(sample_y);
         y_index++) {
        for (unsigned int x_index = 0; x_index < G_N_ELEMENTS(sample_x);
             x_index++) {
            unsigned int x = sample_x[x_index] % HVS_TILED_RGBA8888_WIDTH;
            uint8_t red, green, blue;

            hvs_tiled_rgba8888_components(x, sample_y[y_index],
                                           &red, &green, &blue);
            hvs_assert_ppm_pixel_sized(contents, length,
                                       HVS_TILED_RGBA8888_WIDTH,
                                       HVS_TILED_HEIGHT, x, sample_y[y_index],
                                       red, green, blue);
        }
    }

    qtest_quit(qts);
}

static void hvs_program_ppf_reference(QTestState *qts)
{
    uint8_t source[4 * 4 * sizeof(uint32_t)];

    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            uint8_t *pixel = source + (y * 4 + x) * sizeof(uint32_t);
            bool blue = x >= 2;

            /* Guest little-endian ARGB: B, G, R, A. */
            pixel[0] = blue ? 0xff : 0x00;
            pixel[1] = 0x00;
            pixel[2] = blue ? 0x00 : 0xff;
            pixel[3] = 0xff;
        }
    }
    qtest_memwrite(qts, HVS_OVERLAY_BASE, source, sizeof(source));

    /*
     * A full RGB PPF list has LBM, horizontal and vertical PPF state, and
     * four coefficient pointers after the source pitch.  The values are not
     * decoded by the bounded compositor yet; their presence selects its
     * Linux PPF approximation instead of the short-list nearest path.
     */
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (17U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_ARGB << HVS_CTL_ORDER_SHIFT) |
                    HVS_CTL_FORMAT_RGBA8888);
    hvs_write_dlist(qts, 1, 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (8U << HVS_POS1_HEIGHT_SHIFT) | 8);
    hvs_write_dlist(qts, 4, (4U << HVS_POS2_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 5, 0xc0c0c0c0);
    hvs_write_dlist(qts, 6, HVS_OVERLAY_BASE);
    hvs_write_dlist(qts, 7, 0xc0c0c0c0);
    hvs_write_dlist(qts, 8, 4 * sizeof(uint32_t));
    hvs_write_dlist(qts, 9, 0);  /* LBM base */
    hvs_write_dlist(qts, 10, 0); /* horizontal PPF */
    hvs_write_dlist(qts, 11, 0); /* vertical PPF */
    hvs_write_dlist(qts, 12, 0); /* vertical PPF context */
    hvs_write_dlist(qts, 13, 0); /* horizontal luma PPF kernel */
    hvs_write_dlist(qts, 14, 0); /* vertical luma PPF kernel */
    hvs_write_dlist(qts, 15, 0); /* horizontal chroma PPF kernel */
    hvs_write_dlist(qts, 16, 0); /* vertical chroma PPF kernel */
    hvs_write_dlist(qts, 17, HVS_CTL_END);
}

static void test_hvs_ppf_filter_reference(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_ppf_reference(qts);

    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (8U << 16) | 8);
    contents = display_screendump(qts, &length);

    hvs_assert_ppm_pixel(contents, length, 2, 0, 255, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 6, 0, 0, 0, 255);

    /*
     * Pi 400 HVS reference, XRGB8888 4x4 -> 8x8 first row:
     * ff0000 ff0000 ff0000 ed0012 6f0090 0200fd 0000ff 0000ff.
     * The software Mitchell filter is intentionally close to, rather than
     * bit-identical with, the hardware's quantized coefficient datapath.
     */
    hvs_assert_ppm_pixel_close(contents, length, 3, 0, 237, 0, 18, 20);
    hvs_assert_ppm_pixel_close(contents, length, 4, 0, 111, 0, 144, 20);
    hvs_assert_ppm_pixel_close(contents, length, 5, 0, 2, 0, 253, 20);

    qtest_quit(qts);
}

static void hvs_program_tpz_reference(QTestState *qts)
{
    uint8_t source[8 * 8 * sizeof(uint32_t)];

    for (unsigned int y = 0; y < 8; y++) {
        for (unsigned int x = 0; x < 8; x++) {
            uint8_t *pixel = source + (y * 8 + x) * sizeof(uint32_t);
            bool blue = (x + y) & 1;

            /* Guest little-endian ARGB: B, G, R, A. */
            pixel[0] = blue ? 0xff : 0x00;
            pixel[1] = 0x00;
            pixel[2] = blue ? 0x00 : 0xff;
            pixel[3] = 0xff;
        }
    }
    qtest_memwrite(qts, HVS_OVERLAY_BASE, source, sizeof(source));

    /* A full RGB TPZ list has LBM plus two horizontal and three vertical
     * parameter words, but no PPF coefficient pointers. */
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (15U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_ARGB << HVS_CTL_ORDER_SHIFT) |
                    (HVS_CTL_SCL_H_TPZ_V_TPZ << HVS_CTL_SCL0_SHIFT) |
                    HVS_CTL_FORMAT_RGBA8888);
    hvs_write_dlist(qts, 1, 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (4U << HVS_POS1_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 4, (8U << HVS_POS2_HEIGHT_SHIFT) | 8);
    hvs_write_dlist(qts, 5, 0xc0c0c0c0);
    hvs_write_dlist(qts, 6, HVS_OVERLAY_BASE);
    hvs_write_dlist(qts, 7, 0xc0c0c0c0);
    hvs_write_dlist(qts, 8, 8 * sizeof(uint32_t));
    hvs_write_dlist(qts, 9, 0);  /* LBM base */
    hvs_write_dlist(qts, 10, 0); /* horizontal TPZ */
    hvs_write_dlist(qts, 11, 0); /* horizontal TPZ reciprocal */
    hvs_write_dlist(qts, 12, 0); /* vertical TPZ */
    hvs_write_dlist(qts, 13, 0); /* vertical TPZ reciprocal */
    hvs_write_dlist(qts, 14, 0); /* vertical TPZ context */
    hvs_write_dlist(qts, 15, HVS_CTL_END);
}

static void test_hvs_tpz_filter_reference(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_tpz_reference(qts);

    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (4U << 16) | 4);
    contents = display_screendump(qts, &length);

    /* Pi 400 reference: an 8x8 one-pixel red/blue checker scaled to 4x4
     * produces 7f007f in every output pixel. */
    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            hvs_assert_ppm_pixel_4x4(contents, length, x, y,
                                     127, 0, 127);
        }
    }

    qtest_quit(qts);
}

static void hvs_program_horizontal_tpz_reference(QTestState *qts,
                                                 uint32_t source_width)
{
    uint8_t source[12 * 4 * sizeof(uint32_t)];
    uint32_t scale;
    uint32_t reciprocal;

    g_assert_cmpuint(source_width, <=, 12);
    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < source_width; x++) {
            uint8_t *pixel = source +
                (y * source_width + x) * sizeof(uint32_t);
            bool blue = (x + y) & 1;

            /* Guest little-endian ARGB: B, G, R, A. */
            pixel[0] = blue ? 0xff : 0x00;
            pixel[1] = 0x00;
            pixel[2] = blue ? 0x00 : 0xff;
            pixel[3] = 0xff;
        }
    }
    qtest_memwrite(qts, HVS_OVERLAY_BASE, source,
                   source_width * 4 * sizeof(uint32_t));

    /* Match vc4_write_tpz(): source/destination is 16.16 in TPZ0 and
     * TPZ1 receives ~0 divided by that scale. */
    scale = (source_width << 16) / 4;
    reciprocal = UINT32_MAX / scale;
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (11U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_ARGB << HVS_CTL_ORDER_SHIFT) |
                    (HVS_CTL_SCL_H_TPZ_V_NONE << HVS_CTL_SCL0_SHIFT) |
                    HVS_CTL_FORMAT_RGBA8888);
    hvs_write_dlist(qts, 1, 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (4U << HVS_POS1_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 4, (4U << HVS_POS2_HEIGHT_SHIFT) | source_width);
    hvs_write_dlist(qts, 5, 0xc0c0c0c0);
    hvs_write_dlist(qts, 6, HVS_OVERLAY_BASE);
    hvs_write_dlist(qts, 7, 0xc0c0c0c0);
    hvs_write_dlist(qts, 8, source_width * sizeof(uint32_t));
    hvs_write_dlist(qts, 9, scale << 8); /* horizontal TPZ scale */
    hvs_write_dlist(qts, 10, reciprocal & 0xffff); /* reciprocal */
    hvs_write_dlist(qts, 11, HVS_CTL_END);
}

static void test_hvs_tpz_three_to_one_reference(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_horizontal_tpz_reference(qts, 12);
    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (4U << 16) | 4);
    contents = display_screendump(qts, &length);

    /* Pi 400 reference, a horizontal-only 12x4 -> 4x4 one-pixel checker:
     * aa0055 5500aa aa0055 5500aa.  Each TPZ destination pixel covers
     * three source pixels rather than just the leading two. */
    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            bool blue = (x + y) & 1;

            hvs_assert_ppm_pixel_4x4(contents, length, x, y,
                                     blue ? 85 : 170, 0,
                                     blue ? 170 : 85);
        }
    }

    qtest_quit(qts);
}

static void hvs_program_vertical_tpz_three_to_one_reference(QTestState *qts)
{
    uint8_t source[4 * 12 * sizeof(uint32_t)];

    for (unsigned int y = 0; y < 12; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            uint8_t *pixel = source + (y * 4 + x) * sizeof(uint32_t);
            bool blue = (x + y) & 1;

            /* Guest little-endian ARGB: B, G, R, A. */
            pixel[0] = blue ? 0xff : 0x00;
            pixel[1] = 0x00;
            pixel[2] = blue ? 0x00 : 0xff;
            pixel[3] = 0xff;
        }
    }
    qtest_memwrite(qts, HVS_OVERLAY_BASE, source, sizeof(source));

    /* A vertical-only TPZ list retains the line-buffer word, then carries
     * TPZ scale, reciprocal and context. */
    hvs_write_dlist(qts, 0,
                    HVS_CTL_VALID | (13U << HVS_CTL_SIZE_SHIFT) |
                    (HVS_CTL_ORDER_ARGB << HVS_CTL_ORDER_SHIFT) |
                    (HVS_CTL_SCL_H_NONE_V_TPZ << HVS_CTL_SCL0_SHIFT) |
                    HVS_CTL_FORMAT_RGBA8888);
    hvs_write_dlist(qts, 1, 0);
    hvs_write_dlist(qts, 2,
                    HVS_CTL2_ALPHA_MODE_FIXED |
                    (HVS_CTL2_ALPHA_OPAQUE << HVS_CTL2_ALPHA_SHIFT));
    hvs_write_dlist(qts, 3, (4U << HVS_POS1_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 4, (12U << HVS_POS2_HEIGHT_SHIFT) | 4);
    hvs_write_dlist(qts, 5, 0xc0c0c0c0);
    hvs_write_dlist(qts, 6, HVS_OVERLAY_BASE);
    hvs_write_dlist(qts, 7, 0xc0c0c0c0);
    hvs_write_dlist(qts, 8, 4 * sizeof(uint32_t));
    hvs_write_dlist(qts, 9, 0);          /* LBM base */
    hvs_write_dlist(qts, 10, 0x03000000); /* vertical TPZ scale */
    hvs_write_dlist(qts, 11, 0x5555);    /* reciprocal */
    hvs_write_dlist(qts, 12, 0xc0c0c0c0); /* vertical context */
    hvs_write_dlist(qts, 13, HVS_CTL_END);
}

static void test_hvs_vertical_tpz_three_to_one_reference(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_vertical_tpz_three_to_one_reference(qts);
    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (4U << 16) | 4);
    contents = display_screendump(qts, &length);

    /* Pi 400 vertical-only 4x12 -> 4x4 reference has the same alternating
     * aa0055 / 5500aa output as the horizontal 3:1 capture. */
    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            bool blue = (x + y) & 1;

            hvs_assert_ppm_pixel_4x4(contents, length, x, y,
                                     blue ? 85 : 170, 0,
                                     blue ? 170 : 85);
        }
    }

    qtest_quit(qts);
}

static void test_hvs_tpz_fractional_reference(void)
{
    g_autofree char *contents = NULL;
    QTestState *qts = display_start();
    size_t length;

    hvs_program_horizontal_tpz_reference(qts, 10);
    qtest_writel(qts, HVS_DISPLIST0, 0);
    qtest_writel(qts, HVS_DISPCTRL0,
                 HVS_DISPCTRL_ENABLE | (4U << 16) | 4);
    contents = display_screendump(qts, &length);

    /* Pi 400 reference, horizontal-only 10x4 -> 4x4: the first two
     * columns are 990066, the last two are 660099.  This anchors a
     * fractional 2.5:1 coverage interval as well as the integer 3:1 case. */
    for (unsigned int y = 0; y < 4; y++) {
        for (unsigned int x = 0; x < 4; x++) {
            uint8_t red = x < 2 ? 153 : 102;
            uint8_t blue = 255 - red;

            if (y & 1) {
                uint8_t temporary = red;

                red = blue;
                blue = temporary;
            }
            hvs_assert_ppm_pixel_4x4(contents, length, x, y,
                                     red, 0, blue);
        }
    }

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

static void test_legacy_framebuffer_migration(void)
{
    static const char header[] = "P6\n4 1\n255\n";
    const uint32_t physical_size[] = { 4, 1 };
    const uint32_t virtual_size[] = { 4, 1 };
    const uint32_t depth[] = { 24 };
    const uint32_t pixel_order[] = { 1 };
    const uint32_t offset[] = { 0, 0 };
    const uint32_t allocate[] = { 16, 0 };
    const uint8_t pixels[] = {
        0x11, 0x22, 0x33,
        0x44, 0x55, 0x66,
        0x77, 0x88, 0x99,
        0xaa, 0xbb, 0xcc,
    };
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    size_t length;
    uint32_t base;

    framebuffer_property_request(
        source, RPI_FWREQ_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT,
        physical_size, G_N_ELEMENTS(physical_size), sizeof(physical_size));
    framebuffer_property_request(
        source, RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT,
        virtual_size, G_N_ELEMENTS(virtual_size), sizeof(virtual_size));
    framebuffer_property_request(source, RPI_FWREQ_FRAMEBUFFER_SET_DEPTH,
                                 depth, G_N_ELEMENTS(depth), sizeof(depth));
    framebuffer_property_request(source, RPI_FWREQ_FRAMEBUFFER_SET_PIXEL_ORDER,
                                 pixel_order, G_N_ELEMENTS(pixel_order),
                                 sizeof(pixel_order));
    framebuffer_property_request(source,
                                 RPI_FWREQ_FRAMEBUFFER_SET_VIRTUAL_OFFSET,
                                 offset, G_N_ELEMENTS(offset), sizeof(offset));
    framebuffer_property_request(source, RPI_FWREQ_FRAMEBUFFER_ALLOCATE,
                                 allocate, G_N_ELEMENTS(allocate),
                                 sizeof(allocate));
    base = qtest_readl(source, PROPERTY_BUFFER + 20);
    qtest_memwrite(source, base, pixels, sizeof(pixels));

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2711-fb-migration-XXXXXX", &error);
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

    contents = display_screendump(destination, &length);
    assert_ppm_header(contents, length, header);
    g_assert_cmpuint(length, ==, sizeof(header) - 1 + sizeof(pixels));
    g_assert_cmpmem(contents + sizeof(header) - 1, sizeof(pixels),
                    pixels, sizeof(pixels));

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
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
    hdmi_set_connected(source, false);
    g_assert_false(hdmi_get_connected(source));
    g_assert_cmphex(qtest_readl(source, HDMI_HOTPLUG), ==, 0);

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
    contents = display_screendump(destination, &length);
    hvs_assert_ppm_pixel(contents, length, 0, 0, 248, 0, 0);
    hvs_assert_ppm_pixel(contents, length, 2, 2, 0, 252, 0);
    hvs_assert_ppm_pixel(contents, length, 5, 2, 0, 0, 248);
    hvs_assert_ppm_pixel(contents, length, 2, 5, 248, 252, 248);
    hvs_assert_ppm_pixel(contents, length, 5, 5, 0, 0, 0);

    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_CONFIG), ==,
                    BIT(16) | 0x55aa);
    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_STATUS), ==,
                    0x55aa);
    g_assert_false(hdmi_get_connected(destination));
    g_assert_cmphex(qtest_readl(destination, HDMI_HOTPLUG), ==, 0);
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

    hdmi_set_connected(destination, true);

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

static void test_hvs_ppf_filter_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    size_t length;
    const uint32_t hvs_control = HVS_DISPCTRL_ENABLE | (8U << 16) | 8;

    hvs_program_ppf_reference(source);
    qtest_writel(source, HVS_DISPLIST0, 0);
    qtest_writel(source, HVS_DISPCTRL0, hvs_control);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2711-hvs-ppf-migration-XXXXXX", &error);
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

    g_assert_cmphex(qtest_readl(destination, HVS_DISPLIST0), ==, 0);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPCTRL0), ==,
                    hvs_control);
    contents = display_screendump(destination, &length);
    hvs_assert_ppm_pixel_close(contents, length, 3, 0, 237, 0, 18, 20);
    hvs_assert_ppm_pixel_close(contents, length, 4, 0, 111, 0, 144, 20);
    hvs_assert_ppm_pixel_close(contents, length, 5, 0, 2, 0, 253, 20);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_hvs_tpz_filter_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    size_t length;
    const uint32_t hvs_control = HVS_DISPCTRL_ENABLE | (4U << 16) | 4;

    hvs_program_tpz_reference(source);
    qtest_writel(source, HVS_DISPLIST0, 0);
    qtest_writel(source, HVS_DISPCTRL0, hvs_control);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2711-hvs-tpz-migration-XXXXXX", &error);
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

    g_assert_cmphex(qtest_readl(destination, HVS_DISPLIST0), ==, 0);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPCTRL0), ==,
                    hvs_control);
    contents = display_screendump(destination, &length);
    hvs_assert_ppm_pixel_4x4(contents, length, 0, 0, 127, 0, 127);
    hvs_assert_ppm_pixel_4x4(contents, length, 3, 3, 127, 0, 127);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}

static void test_hvs_t_tiled_rgb_scanout_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *contents = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    const uint32_t hvs_control = HVS_DISPCTRL_ENABLE |
        (HVS_TILED_RGB565_WIDTH << 16) | HVS_TILED_HEIGHT;
    size_t length;
    uint8_t red, green, blue;
    uint16_t pixel = hvs_tiled_rgb565_pixel(96, 35);

    hvs_write_tiled_rgb565_buffer(source);
    hvs_program_tiled_plane(source, HVS_TILED_RGB565_BASE,
                            HVS_TILED_RGB565_WIDTH, HVS_TILED_HEIGHT,
                            HVS_TILED_COLUMNS,
                            HVS_CTL_FORMAT_RGB565, HVS_CTL_ORDER_XRGB,
                            false);
    qtest_writel(source, HVS_DISPLIST0, 0);
    qtest_writel(source, HVS_DISPCTRL0, hvs_control);

    /* Populate the source-side scratch cache before migration. */
    contents = display_screendump(source, &length);
    hvs_rgb565_components(pixel, &red, &green, &blue);
    hvs_assert_ppm_pixel_sized(contents, length, HVS_TILED_RGB565_WIDTH,
                               HVS_TILED_HEIGHT, 96, 35, red, green, blue);
    g_clear_pointer(&contents, g_free);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2711-hvs-t-tile-migration-XXXXXX", &error);
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

    g_assert_cmphex(qtest_readl(destination, HVS_DISPLIST0), ==, 0);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPCTRL0), ==,
                    hvs_control);
    contents = display_screendump(destination, &length);
    hvs_assert_ppm_pixel_sized(contents, length, HVS_TILED_RGB565_WIDTH,
                               HVS_TILED_HEIGHT, 96, 35, red, green, blue);

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
    qtest_add_func("/bcm2711/display/hvs/t_tiled_rgb_scanout",
                   test_hvs_t_tiled_rgb_scanout);
    qtest_add_func("/bcm2711/display/hvs/ppf_filter_reference",
                   test_hvs_ppf_filter_reference);
    qtest_add_func("/bcm2711/display/hvs/tpz_filter_reference",
                   test_hvs_tpz_filter_reference);
    qtest_add_func("/bcm2711/display/hvs/tpz_three_to_one_reference",
                   test_hvs_tpz_three_to_one_reference);
    qtest_add_func("/bcm2711/display/hvs/tpz_vertical_three_to_one_reference",
                   test_hvs_vertical_tpz_three_to_one_reference);
    qtest_add_func("/bcm2711/display/hvs/tpz_fractional_reference",
                   test_hvs_tpz_fractional_reference);
    qtest_add_func("/bcm2711/display/framebuffer/viewport",
                   test_legacy_framebuffer_viewport);
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
    qtest_add_func("/bcm2711/display/framebuffer/migration",
                   test_legacy_framebuffer_migration);
    qtest_add_func("/bcm2711/display/migration", test_display_migration);
    qtest_add_func("/bcm2711/display/hvs/ppf_filter_migration",
                   test_hvs_ppf_filter_migration);
    qtest_add_func("/bcm2711/display/hvs/tpz_filter_migration",
                   test_hvs_tpz_filter_migration);
    qtest_add_func("/bcm2711/display/hvs/t_tiled_rgb_scanout_migration",
                   test_hvs_t_tiled_rgb_scanout_migration);
#endif
    return g_test_run();
}
