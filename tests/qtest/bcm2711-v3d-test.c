/*
 * QTests for the BCM2711 V3D 4.2 register substrate.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "libqtest.h"
#include "qobject/qdict.h"

#define V3D_HUB_BASE                 0xfec00000
#define V3D_CORE0_BASE               0xfec04000
#define ASB_BASE                     0xfe00a000
#define RPIVID_ASB_BASE              0xfec11000

#define V3D_HUB_AXICFG               (V3D_HUB_BASE + 0x0000)
#define V3D_HUB_UIFCFG               (V3D_HUB_BASE + 0x0004)
#define V3D_HUB_IDENT0               (V3D_HUB_BASE + 0x0008)
#define V3D_HUB_IDENT1               (V3D_HUB_BASE + 0x000c)
#define V3D_HUB_IDENT2               (V3D_HUB_BASE + 0x0010)
#define V3D_HUB_IDENT3               (V3D_HUB_BASE + 0x0014)
#define V3D_HUB_INT_STS              (V3D_HUB_BASE + 0x0050)
#define V3D_HUB_INT_SET              (V3D_HUB_BASE + 0x0054)
#define V3D_HUB_INT_CLR              (V3D_HUB_BASE + 0x0058)
#define V3D_HUB_INT_MSK_STS          (V3D_HUB_BASE + 0x005c)
#define V3D_HUB_INT_MSK_SET          (V3D_HUB_BASE + 0x0060)
#define V3D_HUB_INT_MSK_CLR          (V3D_HUB_BASE + 0x0064)
#define V3D_MMUC_CONTROL             (V3D_HUB_BASE + 0x1000)
#define V3D_MMU_CTL                  (V3D_HUB_BASE + 0x1200)
#define V3D_MMU_DEBUG_INFO           (V3D_HUB_BASE + 0x1238)

#define V3D_CTL_IDENT0               (V3D_CORE0_BASE + 0x0000)
#define V3D_CTL_IDENT1               (V3D_CORE0_BASE + 0x0004)
#define V3D_CTL_IDENT2               (V3D_CORE0_BASE + 0x0008)
#define V3D_CTL_MISCCFG              (V3D_CORE0_BASE + 0x0018)
#define V3D_CTL_L2TCACTL             (V3D_CORE0_BASE + 0x0030)
#define V3D_CTL_INT_STS              (V3D_CORE0_BASE + 0x0050)
#define V3D_CTL_INT_SET              (V3D_CORE0_BASE + 0x0054)
#define V3D_CTL_INT_CLR              (V3D_CORE0_BASE + 0x0058)
#define V3D_CTL_INT_MSK_STS          (V3D_CORE0_BASE + 0x005c)
#define V3D_CTL_INT_MSK_SET          (V3D_CORE0_BASE + 0x0060)
#define V3D_CTL_INT_MSK_CLR          (V3D_CORE0_BASE + 0x0064)

#define V3D_MMUC_CONTROL_FLUSH       BIT(1)
#define V3D_MMU_CTL_TLB_CLEAR        BIT(2)
#define V3D_L2TCACTL_TMUWCF          BIT(8)
#define V3D_L2TCACTL_L2TFLS          BIT(0)

#define ASB_V3D_S_CTRL               0x08
#define ASB_V3D_M_CTRL               0x0c
#define ASB_AXI_BRDG_ID               0x20
#define ASB_REQ_STOP                  BIT(0)
#define ASB_ACK                       BIT(1)
#define ASB_EMPTY                     BIT(2)
#define PM_PASSWORD                   0x5a000000

#define V3D_QOM_PATH                 "/machine/soc/peripherals/v3d"
#define V3D_GIC_SPI                  74
#define GIC_DIST_BASE                 0xff841000
#define V3D_GIC_IRQ_ID                (32 + V3D_GIC_SPI)
#define V3D_GIC_ISPENDR               (GIC_DIST_BASE + 0x200 + \
                                       (V3D_GIC_IRQ_ID / 32) * 4)
#define V3D_GIC_PENDING               BIT(V3D_GIC_IRQ_ID % 32)

static QTestState *v3d_start_unintercepted(void)
{
    return qtest_init("-machine raspi4b -nic none");
}

static QTestState *v3d_start(void)
{
    QTestState *qts = v3d_start_unintercepted();

    qtest_irq_intercept_out_named(qts, V3D_QOM_PATH, "sysbus-irq");
    return qts;
}

static void test_v3d_pi400_identity(void)
{
    QTestState *qts = v3d_start();

    /* Pi 400 values captured via the Linux V3D driver's debugfs interface. */
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_AXICFG), ==, 0x0000000f);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_UIFCFG), ==, 0x00000045);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_IDENT0), ==, 0x42554856);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_IDENT1), ==, 0x000e1124);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_IDENT2), ==, 0x00000100);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_IDENT3), ==, 0x00000e00);
    g_assert_cmphex(qtest_readl(qts, V3D_MMU_DEBUG_INFO), ==, 0x00000550);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_IDENT0), ==, 0x04443356);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_IDENT1), ==, 0x81001422);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_IDENT2), ==, 0x40078121);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_MISCCFG), ==, 0x00000006);

    qtest_quit(qts);
}

static void test_v3d_interrupt_masks_and_shared_irq(void)
{
    QTestState *qts = v3d_start();
    const uint32_t hub_bit = BIT(1);
    const uint32_t core_bit = BIT(0);

    g_assert_false(qtest_get_irq(qts, 0));

    qtest_writel(qts, V3D_HUB_INT_SET, hub_bit);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_INT_STS), ==, hub_bit);
    g_assert_true(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_HUB_INT_MSK_SET, hub_bit);
    g_assert_cmphex(qtest_readl(qts, V3D_HUB_INT_MSK_STS), ==, hub_bit);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_HUB_INT_MSK_CLR, hub_bit);
    g_assert_true(qtest_get_irq(qts, 0));

    qtest_writel(qts, V3D_CTL_INT_SET, core_bit);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_INT_STS), ==, core_bit);
    g_assert_true(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_HUB_INT_CLR, hub_bit);
    g_assert_true(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_CTL_INT_MSK_SET, core_bit);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_INT_MSK_STS), ==, core_bit);
    g_assert_false(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_CTL_INT_MSK_CLR, core_bit);
    g_assert_true(qtest_get_irq(qts, 0));
    qtest_writel(qts, V3D_CTL_INT_CLR, core_bit);
    g_assert_false(qtest_get_irq(qts, 0));

    qtest_quit(qts);
}

static void test_v3d_kernel_maintenance_polls_complete(void)
{
    QTestState *qts = v3d_start();

    qtest_writel(qts, V3D_MMUC_CONTROL, V3D_MMUC_CONTROL_FLUSH);
    g_assert_cmphex(qtest_readl(qts, V3D_MMUC_CONTROL), ==, 0);
    qtest_writel(qts, V3D_MMU_CTL, V3D_MMU_CTL_TLB_CLEAR);
    g_assert_cmphex(qtest_readl(qts, V3D_MMU_CTL), ==, 0);
    qtest_writel(qts, V3D_CTL_L2TCACTL,
                 V3D_L2TCACTL_TMUWCF | V3D_L2TCACTL_L2TFLS);
    g_assert_cmphex(qtest_readl(qts, V3D_CTL_L2TCACTL), ==, 0);

    qtest_quit(qts);
}

static void test_v3d_gic_wiring(void)
{
    QTestState *qts = v3d_start_unintercepted();

    g_assert_false(qtest_readl(qts, V3D_GIC_ISPENDR) & V3D_GIC_PENDING);
    qtest_writel(qts, V3D_HUB_INT_SET, BIT(1));
    g_assert_true(qtest_readl(qts, V3D_GIC_ISPENDR) & V3D_GIC_PENDING);
    qtest_writel(qts, V3D_HUB_INT_CLR, BIT(1));
    g_assert_false(qtest_readl(qts, V3D_GIC_ISPENDR) & V3D_GIC_PENDING);

    qtest_quit(qts);
}

static void test_v3d_asb_stop_and_resume(void)
{
    QTestState *qts = v3d_start_unintercepted();

    /*
     * Pi 400 bridge identity and idle values captured through read-only MMIO.
     */
    g_assert_cmphex(qtest_readl(qts, ASB_BASE + ASB_AXI_BRDG_ID), ==,
                    0x62726467);
    g_assert_cmphex(qtest_readl(qts, ASB_BASE + ASB_V3D_S_CTRL), ==,
                    ASB_REQ_STOP | ASB_EMPTY);
    g_assert_cmphex(qtest_readl(qts, ASB_BASE + ASB_V3D_M_CTRL), ==,
                    ASB_REQ_STOP | ASB_EMPTY);
    g_assert_cmphex(qtest_readl(qts, RPIVID_ASB_BASE + ASB_V3D_S_CTRL), ==,
                    ASB_REQ_STOP | ASB_ACK | ASB_EMPTY);
    g_assert_cmphex(qtest_readl(qts, RPIVID_ASB_BASE + ASB_V3D_M_CTRL), ==,
                    ASB_REQ_STOP | ASB_ACK | ASB_EMPTY);

    qtest_writel(qts, RPIVID_ASB_BASE + ASB_V3D_S_CTRL,
                 PM_PASSWORD | ASB_REQ_STOP);
    g_assert_cmphex(qtest_readl(qts, RPIVID_ASB_BASE + ASB_V3D_S_CTRL), ==,
                    ASB_REQ_STOP | ASB_ACK | ASB_EMPTY);
    qtest_writel(qts, RPIVID_ASB_BASE + ASB_V3D_S_CTRL, PM_PASSWORD);
    g_assert_cmphex(qtest_readl(qts, RPIVID_ASB_BASE + ASB_V3D_S_CTRL), ==,
                    ASB_EMPTY);

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
    g_error("timed out waiting for BCM2711 V3D migration");
}

static void test_v3d_migration(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = v3d_start_unintercepted();
    QTestState *destination;

    qtest_writel(source, V3D_HUB_INT_SET, BIT(3));
    qtest_writel(source, RPIVID_ASB_BASE + ASB_V3D_S_CTRL, PM_PASSWORD);
    qtest_writel(source, RPIVID_ASB_BASE + ASB_V3D_M_CTRL,
                 PM_PASSWORD | ASB_REQ_STOP);
    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");

    tmpdir = g_dir_make_tmp("bcm2711-v3d-migration-XXXXXX", &error);
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

    g_assert_cmphex(qtest_readl(destination, V3D_HUB_INT_STS), ==, BIT(3));
    g_assert_true(qtest_readl(destination, V3D_GIC_ISPENDR) &
                  V3D_GIC_PENDING);
    g_assert_cmphex(qtest_readl(destination,
                                RPIVID_ASB_BASE + ASB_V3D_S_CTRL), ==,
                    ASB_EMPTY);
    g_assert_cmphex(qtest_readl(destination,
                                RPIVID_ASB_BASE + ASB_V3D_M_CTRL), ==,
                    ASB_REQ_STOP | ASB_ACK | ASB_EMPTY);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/bcm2711-v3d/pi400_identity", test_v3d_pi400_identity);
    qtest_add_func("/bcm2711-v3d/interrupts/shared_irq",
                   test_v3d_interrupt_masks_and_shared_irq);
    qtest_add_func("/bcm2711-v3d/maintenance/polls_complete",
                   test_v3d_kernel_maintenance_polls_complete);
    qtest_add_func("/bcm2711-v3d/interrupts/gic_wiring",
                   test_v3d_gic_wiring);
    qtest_add_func("/bcm2711-v3d/asb/stop_and_resume",
                   test_v3d_asb_stop_and_resume);
#ifndef _WIN32
    qtest_add_func("/bcm2711-v3d/migration", test_v3d_migration);
#endif

    return g_test_run();
}
