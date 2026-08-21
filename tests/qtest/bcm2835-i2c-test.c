/*
 * QTest testcase for Broadcom Serial Controller (BSC)
 *
 * Copyright (c) 2024 Rayhan Faizel <rayhan.faizel@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "libqtest-single.h"

#include "hw/i2c/bcm2835_i2c.h"

#define RASPI4_GIC_I2C_IRQ 117

static const uint32_t bsc_base_addrs[] = {
    0xfe205000,                         /* I2C0 */
    0xfe804000,                         /* I2C1 */
    0xfe805000,                         /* I2C2 */
};

static void test_i2c_registers_and_nack(gconstpointer data)
{
    intptr_t index = (intptr_t)data;
    uint32_t base_addr = bsc_base_addrs[index];
    uint32_t status;

    g_assert_cmphex(readl(base_addr + BCM2835_I2C_C), ==, 0);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_S), ==,
                    BCM2835_I2C_S_TXD | BCM2835_I2C_S_TXE);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_DLEN), ==, 0);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_A), ==, 0);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_DIV), ==, 0x5dc);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_DEL), ==, 0x00300030);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_CLKT), ==, 0x40);

    writel(base_addr + BCM2835_I2C_DIV, 0x1234);
    writel(base_addr + BCM2835_I2C_DEL, 0x5678);
    writel(base_addr + BCM2835_I2C_CLKT, 0x9abc);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_DIV), ==, 0x1234);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_DEL), ==, 0x5678);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_CLKT), ==, 0x9abc);

    /* A zero-length transfer to an unattached address must report NACK. */
    writel(base_addr + BCM2835_I2C_A, 0x50);
    writel(base_addr + BCM2835_I2C_DLEN, 0);
    g_assert_false(get_irq(RASPI4_GIC_I2C_IRQ));
    writel(base_addr + BCM2835_I2C_C,
           BCM2835_I2C_C_I2CEN | BCM2835_I2C_C_INTD |
           BCM2835_I2C_C_ST | BCM2835_I2C_C_CLEAR);

    status = readl(base_addr + BCM2835_I2C_S);
    g_assert_cmphex(status & (BCM2835_I2C_S_ERR | BCM2835_I2C_S_DONE), ==,
                    BCM2835_I2C_S_ERR | BCM2835_I2C_S_DONE);
    g_assert_false(status & BCM2835_I2C_S_TA);
    g_assert_true(get_irq(RASPI4_GIC_I2C_IRQ));

    writel(base_addr + BCM2835_I2C_S,
           BCM2835_I2C_S_DONE | BCM2835_I2C_S_ERR | BCM2835_I2C_S_CLKT);
    g_assert_cmphex(readl(base_addr + BCM2835_I2C_S), ==,
                    BCM2835_I2C_S_TXD | BCM2835_I2C_S_TXE);
    g_assert_false(get_irq(RASPI4_GIC_I2C_IRQ));
}

int main(int argc, char **argv)
{
    int ret;
    int i;

    g_test_init(&argc, &argv, NULL);

    for (i = 0; i < 3; i++) {
        g_autofree char *test_name =
            g_strdup_printf("/bcm2835/bcm2835-i2c%d/registers_and_nack", i);
        qtest_add_data_func(test_name, (void *)(intptr_t) i,
                            test_i2c_registers_and_nack);
    }

    qtest_start("-M raspi4b");
    qtest_irq_intercept_in(global_qtest, "/machine/soc/peripherals");
    ret = g_test_run();
    qtest_end();

    return ret;
}
