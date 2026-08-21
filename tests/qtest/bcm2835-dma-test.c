/*
 * QTest testcase for BCM283x DMA engine (on Raspberry Pi 4)
 * and its interrupts coming to the GIC.
 *
 * Copyright (c) 2022 Auriga LLC
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest-single.h"

/* Offsets in the raspi4b platform. */
#define RASPI4_DMA_BASE 0xfe007000

/* Used register/fields definitions */

/* DMA engine registers: */
#define BCM2708_DMA_CS         0
#define BCM2708_DMA_ACTIVE     (1 << 0)
#define BCM2708_DMA_INT        (1 << 2)

#define BCM2708_DMA_ADDR       0x04

#define BCM2708_DMA_INT_STATUS 0xfe0

/* DMA Transfer Info fields: */
#define BCM2708_DMA_INT_EN     (1 << 0)
#define BCM2708_DMA_D_INC      (1 << 4)
#define BCM2708_DMA_S_INC      (1 << 8)

/* Data for the test: */
#define SCB_ADDR   256
#define S_ADDR     32
#define D_ADDR     64
#define TXFR_LEN   32
const uint32_t check_data = 0x12345678;

static void bcm2835_dma_test_interrupt(int dma_c, int gic_irq)
{
    uint64_t dma_base = RASPI4_DMA_BASE + dma_c * 0x100;

    /* Check that interrupts are silent by default: */
    g_assert_false(get_irq(gic_irq));
    int isr = readl(dma_base + BCM2708_DMA_INT_STATUS);
    g_assert_cmpint(isr, ==, 0);
    uint32_t reg0 = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmpint(reg0, ==, 0);

    /* Prepare Control Block: */
    writel(SCB_ADDR + 0, BCM2708_DMA_S_INC | BCM2708_DMA_D_INC |
                         BCM2708_DMA_INT_EN); /* transfer info */
    writel(SCB_ADDR + 4, S_ADDR);             /* source address */
    writel(SCB_ADDR + 8, D_ADDR);             /* destination address */
    writel(SCB_ADDR + 12, TXFR_LEN);          /* transfer length */
    writel(dma_base + BCM2708_DMA_ADDR, SCB_ADDR);

    writel(S_ADDR, check_data);
    for (int word = S_ADDR + 4; word < S_ADDR + TXFR_LEN; word += 4) {
        writel(word, ~check_data);
    }
    /* Perform the transfer: */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);

    /* Check that destination == source: */
    uint32_t data = readl(D_ADDR);
    g_assert_cmpint(data, ==, check_data);
    for (int word = D_ADDR + 4; word < D_ADDR + TXFR_LEN; word += 4) {
        data = readl(word);
        g_assert_cmpint(data, ==, ~check_data);
    }

    /* Check that interrupt status is set both in DMA and the GIC input. */
    isr = readl(RASPI4_DMA_BASE + BCM2708_DMA_INT_STATUS);
    g_assert_cmpint(isr, ==, 1 << dma_c);
    g_assert_true(get_irq(gic_irq));

    /* Clean up, clear interrupt: */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_INT);
    g_assert_false(get_irq(gic_irq));
}

static void bcm2835_dma_test_interrupts(void)
{
    /* Pi 4 has separate and paired DMA inputs on the GIC. */
    bcm2835_dma_test_interrupt(0, 80);
    bcm2835_dma_test_interrupt(6, 86);
    bcm2835_dma_test_interrupt(7, 87);
    bcm2835_dma_test_interrupt(8, 87);
    bcm2835_dma_test_interrupt(9, 88);
    bcm2835_dma_test_interrupt(10, 88);
}

int main(int argc, char **argv)
{
    int ret;
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2835/dma/test_interrupts",
                   bcm2835_dma_test_interrupts);
    qtest_start("-machine raspi4b");
    qtest_irq_intercept_in(global_qtest, "/machine/soc/peripherals");
    ret = g_test_run();
    qtest_end();
    return ret;
}
