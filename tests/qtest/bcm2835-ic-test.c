/*
 * BCM2835 interrupt controller reset tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest-single.h"

#define RASPI4_IC_PATH          "/machine/soc/peripherals/ic"
#define RASPI4_PERIPHERALS_PATH "/machine/soc/peripherals"
#define RASPI4_IC_BASE          0xfe00b200
#define RASPI4_IC_FIQ           (RASPI4_IC_BASE + 0x0c)
#define RASPI4_IC_ENABLE_1      (RASPI4_IC_BASE + 0x10)
#define RASPI4_MPHI_BASE        0xfe006000

static void test_reset_deasserts_outputs(void)
{
    /* The legacy IC output is not used by BCM2711, so intercept it directly. */
    qtest_irq_intercept_out_named(global_qtest, RASPI4_PERIPHERALS_PATH,
                                  "sysbus-irq");

    /* A producer may remain high while the controller is reset. */
    qtest_set_irq_in(global_qtest, RASPI4_IC_PATH, "gpu-irq", 5, 1);
    qtest_writel(global_qtest, RASPI4_IC_ENABLE_1, 1U << 5);
    qtest_writel(global_qtest, RASPI4_IC_FIQ, (1U << 7) | 5);
    g_assert_true(qtest_get_irq(global_qtest, 0));
    g_assert_true(qtest_get_irq(global_qtest, 1));

    qtest_system_reset(global_qtest);

    g_assert_false(qtest_get_irq(global_qtest, 0));
    g_assert_false(qtest_get_irq(global_qtest, 1));
}

static void test_mphi_is_not_overlaid(void)
{
    qtest_writel(global_qtest, RASPI4_MPHI_BASE + 0x28, 0x12345678);
    g_assert_cmphex(qtest_readl(global_qtest, RASPI4_MPHI_BASE + 0x28), ==,
                    0x12345678);
}

int main(int argc, char **argv)
{
    int ret;

    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2835-ic/reset/deasserts_outputs",
                   test_reset_deasserts_outputs);
    qtest_add_func("/bcm2835-ic/map/mphi_is_not_overlaid",
                   test_mphi_is_not_overlaid);
    qtest_start("-machine raspi4b -display none");
    ret = g_test_run();
    qtest_end();
    return ret;
}
