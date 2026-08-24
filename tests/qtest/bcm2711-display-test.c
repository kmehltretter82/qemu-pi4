/*
 * QTests for the BCM2711 native display and HDMI service-plane blocks.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
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
#define EDID_LENGTH                 128
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
    QTestState *qts = display_start();
    uint8_t edid[EDID_LENGTH];
    unsigned int checksum = 0;

    ddc_read_edid(qts, edid);
    g_assert_cmpmem(edid, sizeof(edid_header),
                    edid_header, sizeof(edid_header));
    for (unsigned int i = 0; i < sizeof(edid); i++) {
        checksum += edid[i];
    }
    g_assert_cmphex(checksum & 0xff, ==, 0);
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
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = display_start();
    QTestState *destination;
    uint8_t edid[EDID_LENGTH];
    uint32_t status;
    const uint32_t hvs_control =
        HVS_DISPCTRL_ENABLE | (640U << 16) | 480;
    const int64_t pixelvalve_elapsed_ns = 5 * 1000 * 1000;

    ddc_read_edid(source, edid);
    qtest_writel(source, DVP_SW_INIT, BIT(1) | BIT(5));
    qtest_writel(source, DVP_MISC_CONFIG, 0);

    qtest_writel(source, HVS_DLIST_BASE + 0x30 * sizeof(uint32_t),
                 0x12345678);
    qtest_writel(source, HVS_DISPLIST0, 0x30);
    qtest_writel(source, HVS_DISPCTRL0, hvs_control);

    qtest_writel(source, PV_INTEN, PV_INT_VFP_START);
    qtest_writel(source, PV_CONTROL, PV_CONTROL_ENABLE);
    qtest_writel(source, PV_V_CONTROL, PV_V_CONTROL_VIDEO_ENABLE);
    qtest_clock_step(source, pixelvalve_elapsed_ns);

    qtest_writel(source, HDMI_RAM_PACKET_CONFIG, BIT(16) | 0x55aa);
    qtest_writel(source, HDMI_SCHEDULER_CONTROL,
                 HDMI_SCHEDULER_MODE_HDMI);
    qtest_writel(source, HDMI0_PHY_BASE, 0x89abcdef);

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
                               HVS_DLIST_BASE + 0x30 * sizeof(uint32_t)),
                    ==, 0x12345678);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPLIST0), ==, 0x30);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPLACT0), ==, 0x30);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPCTRL0), ==,
                    hvs_control);
    g_assert_cmphex(qtest_readl(destination, HVS_DISPSTAT0), ==,
                    HVS_DISPSTAT_RUN);

    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_CONFIG), ==,
                    BIT(16) | 0x55aa);
    g_assert_cmphex(qtest_readl(destination, HDMI_RAM_PACKET_STATUS), ==,
                    0x55aa);
    g_assert_cmphex(qtest_readl(destination, HDMI_SCHEDULER_CONTROL), ==,
                    HDMI_SCHEDULER_MODE_HDMI |
                    HDMI_SCHEDULER_HDMI_ACTIVE);
    g_assert_cmphex(qtest_readl(destination, HDMI0_PHY_BASE), ==,
                    0x89abcdef);

    qtest_irq_intercept_out_named(destination, PIXELVALVE2_QOM_PATH,
                                  "sysbus-irq");
    g_assert_cmphex(qtest_readl(destination, PV_INTSTAT), ==, 0);
    g_assert_false(qtest_get_irq(destination, 0));
    qtest_qmp_assert_success(destination, "{ 'execute': 'cont' }");
    g_assert_cmpint(qtest_clock_step_next(destination), >, 0);
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
    qtest_add_func("/bcm2711/display/pixelvalve/vblank",
                   test_pixelvalve_vblank_and_reset);
    qtest_add_func("/bcm2711/display/hdmi/transmitter",
                   test_hdmi_transmitter_registers_and_reset);
    qtest_add_func("/bcm2711/display/dvp", test_dvp_reset_and_controls);
    qtest_add_func("/bcm2711/display/ddc/registers_reset_and_nack",
                   test_ddc_registers_reset_and_nack);
    qtest_add_func("/bcm2711/display/ddc/edid", test_ddc_edid);
#ifndef _WIN32
    qtest_add_func("/bcm2711/display/migration", test_display_migration);
#endif
    return g_test_run();
}
