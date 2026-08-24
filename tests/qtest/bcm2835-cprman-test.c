/*
 * QTest testcase for the BCM2835/BCM2711 clock manager.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qobject/qdict.h"

#define RASPI4_CPRMAN_BASE          0xfe101000

#define CM_VPUCTL                   (RASPI4_CPRMAN_BASE + 0x008)
#define CM_VPUDIV                   (RASPI4_CPRMAN_BASE + 0x00c)
#define CM_PCMCTL                   (RASPI4_CPRMAN_BASE + 0x098)
#define CM_PCMDIV                   (RASPI4_CPRMAN_BASE + 0x09c)
#define CM_PLLD                     (RASPI4_CPRMAN_BASE + 0x10c)
#define CM_UARTCTL                  (RASPI4_CPRMAN_BASE + 0x0f0)
#define CM_UARTDIV                  (RASPI4_CPRMAN_BASE + 0x0f4)
#define CM_EMMC2CTL                 (RASPI4_CPRMAN_BASE + 0x1d0)
#define CM_EMMC2DIV                 (RASPI4_CPRMAN_BASE + 0x1d4)

#define A2W_PLLA_ANA1               (RASPI4_CPRMAN_BASE + 0x1014)
#define A2W_PLLA_CTRL               (RASPI4_CPRMAN_BASE + 0x1100)
#define A2W_PLLA_FRAC               (RASPI4_CPRMAN_BASE + 0x1200)
#define A2W_PLLC_CTRL               (RASPI4_CPRMAN_BASE + 0x1120)
#define A2W_PLLC_FRAC               (RASPI4_CPRMAN_BASE + 0x1220)
#define A2W_PLLD_ANA1               (RASPI4_CPRMAN_BASE + 0x1054)
#define A2W_PLLD_ANA2               (RASPI4_CPRMAN_BASE + 0x1058)
#define A2W_PLLD_ANA3               (RASPI4_CPRMAN_BASE + 0x105c)
#define A2W_PLLD_CTRL               (RASPI4_CPRMAN_BASE + 0x1140)
#define A2W_PLLD_FRAC               (RASPI4_CPRMAN_BASE + 0x1240)
#define A2W_PLLD_PER                (RASPI4_CPRMAN_BASE + 0x1540)
#define A2W_PLLB_CTRL               (RASPI4_CPRMAN_BASE + 0x11e0)
#define A2W_PLLB_FRAC               (RASPI4_CPRMAN_BASE + 0x12e0)
#define A2W_PLLB_ARM                (RASPI4_CPRMAN_BASE + 0x13e0)

#define CM_PASSWORD                 0x5a000000U
#define CM_ENABLE                   (1U << 4)
#define CM_SRC_XOSC                 1
#define CM_SRC_PLLD                 6
#define CM_DIV(_integer)            ((_integer) << 12)

#define CPRMAN_QOM_PATH             "/machine/soc/peripherals/cprman"
#define CPRMAN_CLOCK_PATH(_name)    CPRMAN_QOM_PATH "/" _name

/* Clock periods use units of 2^-32 ns. */
#define CLOCK_PERIOD_1SEC           (1000000000ULL << 32)

static QTestState *cprman_start(void)
{
    return qtest_init("-machine raspi4b -nic none");
}

static uint64_t qom_get_uint64(QTestState *qts, const char *path,
                               const char *property)
{
    g_autoptr(QDict) response = qtest_qmp(
        qts, "{ 'execute': 'qom-get', 'arguments': { "
             "'path': %s, 'property': %s } }", path, property);

    g_assert_false(qdict_haskey(response, "error"));
    return qdict_get_int(response, "return");
}

static uint64_t clock_period(QTestState *qts, const char *path)
{
    return qom_get_uint64(qts, path, "qtest-clock-period");
}

static void assert_clock_hz(QTestState *qts, const char *path, uint64_t hz)
{
    g_assert_cmpuint(clock_period(qts, path), ==, CLOCK_PERIOD_1SEC / hz);
}

static void cprman_test_bcm2711_reset_profile(void)
{
    QTestState *qts = cprman_start();

    g_assert_true(qtest_qom_get_bool(qts, CPRMAN_QOM_PATH, "is-bcm2711"));
    g_assert_cmpuint(qom_get_uint64(qts, CPRMAN_QOM_PATH,
                                   "xosc-freq-hz"), ==, 54000000);

    /* Firmware/debugfs state captured on a Raspberry Pi 400. */
    g_assert_cmphex(qtest_readl(qts, CM_PLLD), ==, 0x00000000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLA_CTRL), ==, 0x00021037);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLA_FRAC), ==, 0x0008e38e);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLA_ANA1), ==, 0x0011c000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLC_CTRL), ==, 0x00021030);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLC_FRAC), ==, 0x00000000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_CTRL), ==, 0x00021037);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_FRAC), ==, 0x0008e390);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA1), ==, 0x00118000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA2), ==, 0x00d00000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA3), ==, 0x00000052);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_PER), ==, 4);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLB_CTRL), ==, 0x00021042);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLB_FRAC), ==, 0x000aaaab);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLB_ARM), ==, 2);

    /* The model derives BUSY (bit 7) from the captured ENABLE bit. */
    g_assert_cmphex(qtest_readl(qts, CM_VPUCTL), ==, 0x000002d4);
    g_assert_cmphex(qtest_readl(qts, CM_VPUDIV), ==, 0x00001000);
    g_assert_cmphex(qtest_readl(qts, CM_UARTCTL), ==, 0x00000296);
    g_assert_cmphex(qtest_readl(qts, CM_UARTDIV), ==, 0x0000fa00);
    g_assert_cmphex(qtest_readl(qts, CM_EMMC2CTL), ==, 0x00000296);
    g_assert_cmphex(qtest_readl(qts, CM_EMMC2DIV), ==, 0x00007800);

    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("xosc"), 54000000);
    /* Bit 14 in ANA1 is a VCO-range bit on BCM2711, not a predivider. */
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("plla/out"), 2999999988ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pllc/out"), 2592000000ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("plld/out"), 3000000091ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pllb/out"), 3600000017ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("plld-per/out"), 750000023ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pllc-per/out"), 648000000ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("emmc2/out"), 100000003ULL);

    qtest_quit(qts);
}

static void cprman_test_mux_register_updates(void)
{
    QTestState *qts = cprman_start();

    g_assert_cmpuint(clock_period(qts, CPRMAN_CLOCK_PATH("pcm/out")), ==, 0);

    qtest_writel(qts, CM_PCMDIV, CM_PASSWORD | CM_DIV(9));
    qtest_writel(qts, CM_PCMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_XOSC);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 6000000);
    g_assert_cmphex(qtest_readl(qts, CM_PCMCTL), ==,
                    0x80 | CM_ENABLE | CM_SRC_XOSC);

    /* A divider write must update the output while the mux is running. */
    qtest_writel(qts, CM_PCMDIV, CM_PASSWORD | CM_DIV(18));
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 3000000);

    /* A write without the CPRMAN password must leave state unchanged. */
    qtest_writel(qts, CM_PCMDIV, CM_DIV(27));
    g_assert_cmphex(qtest_readl(qts, CM_PCMDIV), ==, CM_DIV(18));
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 3000000);

    qtest_writel(qts, CM_PCMCTL, CM_PASSWORD | CM_SRC_XOSC);
    g_assert_cmpuint(clock_period(qts, CPRMAN_CLOCK_PATH("pcm/out")), ==, 0);

    qtest_quit(qts);
}

static void cprman_test_pll_channel_updates(void)
{
    QTestState *qts = cprman_start();

    qtest_writel(qts, CM_PCMDIV, CM_PASSWORD | CM_DIV(1));
    qtest_writel(qts, CM_PCMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_PLLD);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 750000023ULL);

    qtest_writel(qts, A2W_PLLD_PER, CM_PASSWORD | 5);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("plld-per/out"), 600000018ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 600000018ULL);

    /* The eight-bit divider encodes its maximum value, 256, as zero. */
    qtest_writel(qts, A2W_PLLD_PER, CM_PASSWORD);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("plld-per/out"), 11718750ULL);
    assert_clock_hz(qts, CPRMAN_CLOCK_PATH("pcm/out"), 11718750ULL);

    qtest_quit(qts);
}

#ifndef _WIN32
static void cprman_wait_for_migration(QTestState *qts)
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

    g_error("timed out waiting for BCM2835 CPRMAN migration");
}

static void cprman_test_migration_rebuilds_outputs(void)
{
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = cprman_start();
    QTestState *destination;

    qtest_writel(source, A2W_PLLD_PER, CM_PASSWORD | 5);
    qtest_writel(source, CM_PCMDIV, CM_PASSWORD | CM_DIV(2));
    qtest_writel(source, CM_PCMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_PLLD);
    assert_clock_hz(source, CPRMAN_CLOCK_PATH("plld-per/out"),
                    600000018ULL);
    assert_clock_hz(source, CPRMAN_CLOCK_PATH("pcm/out"), 300000009ULL);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2835-cprman-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);

    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    cprman_wait_for_migration(source);

    destination_args = g_strdup_printf(
        "-machine raspi4b -nic none -incoming %s", uri);
    destination = qtest_init(destination_args);
    cprman_wait_for_migration(destination);

    g_assert_cmphex(qtest_readl(destination, A2W_PLLD_PER), ==, 5);
    g_assert_cmphex(qtest_readl(destination, CM_PCMDIV), ==, CM_DIV(2));
    g_assert_cmphex(qtest_readl(destination, CM_PCMCTL), ==,
                    0x80 | CM_ENABLE | CM_SRC_PLLD);
    assert_clock_hz(destination, CPRMAN_CLOCK_PATH("plld-per/out"),
                    600000018ULL);
    assert_clock_hz(destination, CPRMAN_CLOCK_PATH("pcm/out"), 300000009ULL);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/bcm2835-cprman/bcm2711/reset-profile",
                   cprman_test_bcm2711_reset_profile);
    qtest_add_func("/bcm2835-cprman/mux/register-updates",
                   cprman_test_mux_register_updates);
    qtest_add_func("/bcm2835-cprman/pll/channel-updates",
                   cprman_test_pll_channel_updates);
#ifndef _WIN32
    qtest_add_func("/bcm2835-cprman/migration/rebuild-outputs",
                   cprman_test_migration_rebuilds_outputs);
#endif

    return g_test_run();
}
