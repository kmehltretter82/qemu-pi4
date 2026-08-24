/*
 * QTests for the BCM2711 HDMI DVP and DDC service-plane blocks.
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

    ddc_read_edid(source, edid);
    qtest_writel(source, DVP_SW_INIT, BIT(0) | BIT(5));
    qtest_writel(source, DVP_MISC_CONFIG, 0);

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
                    BIT(0) | BIT(5));
    g_assert_cmphex(qtest_readl(destination, DVP_MISC_CONFIG), ==, 0);
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
    qtest_add_func("/bcm2711/display/dvp", test_dvp_reset_and_controls);
    qtest_add_func("/bcm2711/display/ddc/registers_reset_and_nack",
                   test_ddc_registers_reset_and_nack);
    qtest_add_func("/bcm2711/display/ddc/edid", test_ddc_edid);
#ifndef _WIN32
    qtest_add_func("/bcm2711/display/migration", test_display_migration);
#endif
    return g_test_run();
}
