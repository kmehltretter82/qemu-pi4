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
#define BCM2708_DMA_END        (1 << 1)
#define BCM2708_DMA_INT        (1 << 2)
#define BCM2708_DMA_DREQ       (1 << 3)
#define BCM2708_DMA_ISPAUSED   (1 << 4)
#define BCM2708_DMA_ISHELD     (1 << 5)
#define BCM2708_DMA_ABORT      (1 << 30)
#define BCM2708_DMA_RESET      (1 << 31)

#define BCM2708_DMA_ADDR       0x04
#define BCM2708_DMA_TXFR_LEN   0x14

#define BCM2708_DMA_INT_STATUS 0xfe0

/* DMA Transfer Info fields: */
#define BCM2708_DMA_INT_EN     (1 << 0)
#define BCM2708_DMA_D_INC      (1 << 4)
#define BCM2708_DMA_D_WIDTH    (1 << 5)
#define BCM2708_DMA_D_DREQ     (1 << 6)
#define BCM2708_DMA_S_INC      (1 << 8)
#define BCM2708_DMA_S_WIDTH    (1 << 9)
#define BCM2708_DMA_PERMAP(_n) ((_n) << 16)

#define RASPI4_DMA_QOM_PATH "/machine/soc/peripherals/dma"

/* Data for the test: */
#define SCB_ADDR   256
#define S_ADDR     32
#define D_ADDR     64
#define TXFR_LEN   32
const uint32_t check_data = 0x12345678;

static void bcm2835_dma_write_cb(uint32_t cb, uint32_t ti, uint32_t source,
                                 uint32_t dest, uint32_t length,
                                 uint32_t next)
{
    writel(cb, ti);
    writel(cb + 4, source);
    writel(cb + 8, dest);
    writel(cb + 12, length);
    writel(cb + 16, 0);
    writel(cb + 20, next);
}

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

static void bcm2835_dma_test_cyclic_yields(void)
{
    const uint64_t dma_base = RASPI4_DMA_BASE + 5 * 0x100;
    const uint32_t cb = 0x1000;
    const uint32_t source = 0x2000;
    const uint32_t dest = 0x3000;
    uint32_t cs;

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    bcm2835_dma_write_cb(cb, 0, source, dest, sizeof(uint32_t), cb);
    writel(source, check_data);
    writel(dest, 0);
    writel(dma_base + BCM2708_DMA_ADDR, cb);

    /*
     * A cyclic channel remains active, but starting it must return to the
     * event loop instead of executing the control-block ring forever.
     */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(readl(dest), ==, check_data);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(cs & BCM2708_DMA_ISPAUSED, ==, 0);

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, 0);
    g_assert_cmphex(cs & BCM2708_DMA_ISPAUSED, ==, BCM2708_DMA_ISPAUSED);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_ADDR), ==, 0);
}

static void bcm2835_dma_test_pause_and_abort(void)
{
    const uint64_t dma_base = RASPI4_DMA_BASE + 5 * 0x100;
    const uint32_t cb0 = 0x1000;
    const uint32_t cb1 = 0x1040;
    const uint32_t source0 = 0x2000;
    const uint32_t source1 = 0x4000;
    const uint32_t dest0 = 0x3000;
    const uint32_t dest1 = 0x5000;
    const uint32_t length = 3072;
    uint32_t cs;
    unsigned offset;

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    bcm2835_dma_write_cb(cb0, BCM2708_DMA_S_INC | BCM2708_DMA_D_INC,
                         source0, dest0, length, cb1);
    bcm2835_dma_write_cb(cb1, 0, source1, dest1, sizeof(uint32_t), 0);
    for (offset = 0; offset < length; offset += sizeof(uint32_t)) {
        writel(source0 + offset, check_data + offset);
        writel(dest0 + offset, 0);
    }
    writel(source1, ~check_data);
    writel(dest1, 0);
    writel(dma_base + BCM2708_DMA_ADDR, cb0);

    /* The first bounded slice copies 1 KiB and leaves the channel active. */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(readl(dest0 + 1020), ==, check_data + 1020);
    g_assert_cmphex(readl(dest0 + 1024), ==, 0);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_TXFR_LEN), ==, 2048);

    /* Clearing ACTIVE pauses in-flight progress until it is set again. */
    writel(dma_base + BCM2708_DMA_CS, 0);
    clock_step(100 * 1000);
    g_assert_cmphex(readl(dest0 + 1024), ==, 0);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ISPAUSED, ==, BCM2708_DMA_ISPAUSED);

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(readl(dest0 + 2044), ==, check_data + 2044);
    g_assert_cmphex(readl(dest0 + 2048), ==, 0);

    /* ABORT skips the current CB even when this write also pauses the DMA. */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ABORT);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_ADDR), ==, cb1);
    g_assert_cmphex(readl(dest1), ==, 0);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, 0);
    g_assert_cmphex(cs & BCM2708_DMA_ISPAUSED, ==, BCM2708_DMA_ISPAUSED);

    /* Re-enabling executes the successor selected by ABORT. */
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(readl(dest1), ==, ~check_data);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, 0);
    g_assert_cmphex(cs & BCM2708_DMA_END, ==, BCM2708_DMA_END);
    g_assert_cmphex(cs & BCM2708_DMA_ISPAUSED, ==, BCM2708_DMA_ISPAUSED);

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
}

static void bcm2835_dma_test_dreq_and_width(void)
{
    const uint64_t dma_base = RASPI4_DMA_BASE + 5 * 0x100;
    const uint32_t cb = 0x1000;
    const uint32_t source = 0x2000;
    const uint32_t dest = 0x3000;
    const uint32_t length = 2048;
    const uint32_t ti = BCM2708_DMA_S_INC | BCM2708_DMA_D_INC |
                        BCM2708_DMA_S_WIDTH | BCM2708_DMA_D_WIDTH |
                        BCM2708_DMA_D_DREQ | BCM2708_DMA_PERMAP(2);
    uint32_t cs;
    unsigned offset;

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    qtest_set_irq_in(global_qtest, RASPI4_DMA_QOM_PATH, "dreq", 2, 0);
    bcm2835_dma_write_cb(cb, ti, source, dest, length, 0);
    for (offset = 0; offset < length; offset += sizeof(uint32_t)) {
        writel(source + offset, check_data + offset);
        writel(dest + offset, 0);
    }
    writel(dma_base + BCM2708_DMA_ADDR, cb);
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);

    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, BCM2708_DMA_ACTIVE);
    g_assert_cmphex(cs & BCM2708_DMA_ISHELD, ==, BCM2708_DMA_ISHELD);
    g_assert_cmphex(cs & BCM2708_DMA_DREQ, ==, 0);
    g_assert_cmphex(readl(dest), ==, 0);

    qtest_set_irq_in(global_qtest, RASPI4_DMA_QOM_PATH, "dreq", 2, 1);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_CS) & BCM2708_DMA_DREQ,
                    ==, BCM2708_DMA_DREQ);
    clock_step(1000);
    for (offset = 0; offset < 1024; offset += sizeof(uint32_t)) {
        g_assert_cmphex(readl(dest + offset), ==, check_data + offset);
    }
    g_assert_cmphex(readl(dest + 1024), ==, 0);

    /* A falling DREQ holds the channel and cancels its pending slice. */
    qtest_set_irq_in(global_qtest, RASPI4_DMA_QOM_PATH, "dreq", 2, 0);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ISHELD, ==, BCM2708_DMA_ISHELD);
    g_assert_cmphex(cs & BCM2708_DMA_DREQ, ==, 0);
    clock_step(100 * 1000);
    g_assert_cmphex(readl(dest + 1024), ==, 0);

    qtest_set_irq_in(global_qtest, RASPI4_DMA_QOM_PATH, "dreq", 2, 1);
    clock_step(1000);
    for (offset = 1024; offset < length; offset += sizeof(uint32_t)) {
        g_assert_cmphex(readl(dest + offset), ==, check_data + offset);
    }
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, 0);
    g_assert_cmphex(cs & BCM2708_DMA_END, ==, BCM2708_DMA_END);

    qtest_set_irq_in(global_qtest, RASPI4_DMA_QOM_PATH, "dreq", 2, 0);
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
}

static void bcm2835_dma_test_dreq_zero_is_permanent(void)
{
    const uint64_t dma_base = RASPI4_DMA_BASE + 5 * 0x100;
    const uint32_t cb = 0x1000;
    const uint32_t source = 0x2000;
    const uint32_t dest = 0x3000;
    const uint32_t ti = BCM2708_DMA_S_INC | BCM2708_DMA_D_INC |
                        BCM2708_DMA_D_DREQ | BCM2708_DMA_PERMAP(0);
    uint32_t cs;

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    bcm2835_dma_write_cb(cb, ti, source, dest, sizeof(uint32_t), 0);
    writel(source, check_data);
    writel(dest, 0);
    writel(dma_base + BCM2708_DMA_ADDR, cb | 0x1f);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_ADDR), ==, cb);
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);

    g_assert_cmphex(readl(dest), ==, check_data);
    cs = readl(dma_base + BCM2708_DMA_CS);
    g_assert_cmphex(cs & BCM2708_DMA_DREQ, ==, BCM2708_DMA_DREQ);
    g_assert_cmphex(cs & BCM2708_DMA_ISHELD, ==, 0);
    g_assert_cmphex(cs & BCM2708_DMA_ACTIVE, ==, 0);

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
}

static void bcm2835_dma_test_unaligned_bytes(void)
{
    const uint64_t dma_base = RASPI4_DMA_BASE + 5 * 0x100;
    const uint32_t cb = 0x1000;
    const uint32_t source = 0x2001;
    const uint32_t dest = 0x3003;
    const uint32_t length = 7;
    unsigned offset;

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
    bcm2835_dma_write_cb(cb, BCM2708_DMA_S_INC | BCM2708_DMA_D_INC,
                         source, dest, length, 0);
    writeb(dest - 1, 0xa5);
    writeb(dest + length, 0x5a);
    for (offset = 0; offset < length; offset++) {
        writeb(source + offset, 0x80 + offset);
        writeb(dest + offset, 0);
    }
    writel(dma_base + BCM2708_DMA_ADDR, cb);
    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_ACTIVE);

    for (offset = 0; offset < length; offset++) {
        g_assert_cmphex(readb(dest + offset), ==, 0x80 + offset);
    }
    g_assert_cmphex(readb(dest - 1), ==, 0xa5);
    g_assert_cmphex(readb(dest + length), ==, 0x5a);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_TXFR_LEN), ==, 0);
    g_assert_cmphex(readl(dma_base + BCM2708_DMA_CS) & BCM2708_DMA_ACTIVE,
                    ==, 0);

    writel(dma_base + BCM2708_DMA_CS, BCM2708_DMA_RESET);
}

int main(int argc, char **argv)
{
    int ret;
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2835/dma/test_interrupts",
                   bcm2835_dma_test_interrupts);
    qtest_add_func("/bcm2835/dma/cyclic_yields",
                   bcm2835_dma_test_cyclic_yields);
    qtest_add_func("/bcm2835/dma/pause_and_abort",
                   bcm2835_dma_test_pause_and_abort);
    qtest_add_func("/bcm2835/dma/dreq_and_width",
                   bcm2835_dma_test_dreq_and_width);
    qtest_add_func("/bcm2835/dma/dreq_zero_is_permanent",
                   bcm2835_dma_test_dreq_zero_is_permanent);
    qtest_add_func("/bcm2835/dma/unaligned_bytes",
                   bcm2835_dma_test_unaligned_bytes);
    qtest_start("-machine raspi4b");
    qtest_irq_intercept_in(global_qtest, "/machine/soc/peripherals");
    ret = g_test_run();
    qtest_end();
    return ret;
}
