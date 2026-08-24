/*
 * QTest testcase for the BCM2711 PWM controllers on Raspberry Pi 4.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"
#include "qobject/qdict.h"
#include "qobject/qnum.h"

#define RASPI4_PWM0_BASE            0xfe20c000
#define RASPI4_PWM1_BASE            0xfe20c800
#define RASPI4_CPRMAN_BASE          0xfe101000
#define RASPI4_DMA5_BASE            0xfe007500

#define PWM_CTL(_base)              ((_base) + 0x00)
#define PWM_STA(_base)              ((_base) + 0x04)
#define PWM_DMAC(_base)             ((_base) + 0x08)
#define PWM_RNG1(_base)             ((_base) + 0x10)
#define PWM_DAT1(_base)             ((_base) + 0x14)
#define PWM_FIF1(_base)             ((_base) + 0x18)
#define PWM_RNG2(_base)             ((_base) + 0x20)
#define PWM_DAT2(_base)             ((_base) + 0x24)

#define PWM_CTL_PWEN(_n)            BIT((_n) * 8)
#define PWM_CTL_MODE(_n)            BIT((_n) * 8 + 1)
#define PWM_CTL_RPTL(_n)            BIT((_n) * 8 + 2)
#define PWM_CTL_SBIT(_n)            BIT((_n) * 8 + 3)
#define PWM_CTL_POLA(_n)            BIT((_n) * 8 + 4)
#define PWM_CTL_USEF(_n)            BIT((_n) * 8 + 5)
#define PWM_CTL_CLRF                BIT(6)
#define PWM_CTL_MSEN(_n)            BIT((_n) * 8 + 7)

#define PWM_STA_FULL                BIT(0)
#define PWM_STA_EMPT                BIT(1)
#define PWM_STA_WERR                BIT(2)
#define PWM_STA_GAP(_n)             BIT(4 + (_n))
#define PWM_STA_ACTIVE(_n)          BIT(9 + (_n))

#define PWM_DMAC_ENAB               BIT(31)
#define PWM_DMAC_PANIC(_n)          ((_n) << 8)
#define PWM_DMAC_DREQ(_n)           (_n)
#define PWM_DMAC_RESET              0x00000707U

#define CM_PWMCTL                   (RASPI4_CPRMAN_BASE + 0x0a0)
#define CM_PWMDIV                   (RASPI4_CPRMAN_BASE + 0x0a4)
#define CM_PASSWORD                 0x5a000000U
#define CM_ENABLE                   BIT(4)
#define CM_SRC_XOSC                 1
#define CM_SRC_PLLD                 6
#define CM_DIV(_integer)            ((_integer) << 12)

#define DMA_CS                      (RASPI4_DMA5_BASE + 0x00)
#define DMA_ADDR                    (RASPI4_DMA5_BASE + 0x04)
#define DMA_TXFR_LEN                (RASPI4_DMA5_BASE + 0x14)
#define DMA_ACTIVE                  BIT(0)
#define DMA_END                     BIT(1)
#define DMA_ISHELD                  BIT(5)
#define DMA_D_DREQ                  BIT(6)
#define DMA_S_INC                   BIT(8)
#define DMA_PERMAP(_n)              ((_n) << 16)

#define DMA_CB                      0x1000
#define DMA_SOURCE                  0x2000

#define PWM0_QOM_PATH               "/machine/soc/peripherals/pwm0"
#define PWM1_QOM_PATH               "/machine/soc/peripherals/pwm1"

static QTestState *bcm2835_pwm_start(void)
{
    return qtest_init("-machine raspi4b -nic none");
}

static uint64_t bcm2835_pwm_qom_get(QTestState *qts, const char *path,
                                     const char *property)
{
    g_autoptr(QDict) response = qtest_qmp(
        qts, "{ 'execute': 'qom-get', 'arguments': { "
             "'path': %s, 'property': %s } }", path, property);

    g_assert_false(qdict_haskey(response, "error"));
    return qnum_get_uint(qobject_to(QNum, qdict_get(response, "return")));
}

static void bcm2835_pwm_program_1mhz_clock(QTestState *qts)
{
    /* 54 MHz XOSC / 54 = 1 MHz. */
    qtest_writel(qts, CM_PWMDIV, CM_PASSWORD | CM_DIV(54));
    qtest_writel(qts, CM_PWMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_XOSC);
}

static void bcm2835_pwm_write_dma_cb(QTestState *qts, uint32_t permap,
                                      uint32_t destination, uint32_t words)
{
    qtest_writel(qts, DMA_CB,
                 DMA_S_INC | DMA_D_DREQ | DMA_PERMAP(permap));
    qtest_writel(qts, DMA_CB + 4, DMA_SOURCE);
    qtest_writel(qts, DMA_CB + 8, destination);
    qtest_writel(qts, DMA_CB + 12, words * sizeof(uint32_t));
    qtest_writel(qts, DMA_CB + 16, 0);
    qtest_writel(qts, DMA_CB + 20, 0);
}

static void bcm2835_pwm_test_reset_and_registers(void)
{
    QTestState *qts = bcm2835_pwm_start();
    const uint64_t base[] = { RASPI4_PWM0_BASE, RASPI4_PWM1_BASE };
    const uint32_t fifo_id[] = { 0x70776d30, 0x70776d31 };
    unsigned int instance;

    for (instance = 0; instance < ARRAY_SIZE(base); instance++) {
        g_assert_cmphex(qtest_readl(qts, PWM_CTL(base[instance])), ==, 0);
        g_assert_cmphex(qtest_readl(qts, PWM_STA(base[instance])), ==,
                        PWM_STA_EMPT);
        g_assert_cmphex(qtest_readl(qts, PWM_DMAC(base[instance])), ==,
                        PWM_DMAC_RESET);
        g_assert_cmphex(qtest_readl(qts, PWM_RNG1(base[instance])), ==, 32);
        g_assert_cmphex(qtest_readl(qts, PWM_DAT1(base[instance])), ==, 0);
        g_assert_cmphex(qtest_readl(qts, PWM_FIF1(base[instance])), ==,
                        fifo_id[instance]);
        g_assert_cmphex(qtest_readl(qts, PWM_RNG2(base[instance])), ==, 32);
        g_assert_cmphex(qtest_readl(qts, PWM_DAT2(base[instance])), ==, 0);

        qtest_writel(qts, PWM_CTL(base[instance]), UINT32_MAX);
        g_assert_cmphex(qtest_readl(qts, PWM_CTL(base[instance])), ==,
                        0x0000bfbf);
        qtest_writel(qts, PWM_DMAC(base[instance]), UINT32_MAX);
        g_assert_cmphex(qtest_readl(qts, PWM_DMAC(base[instance])), ==,
                        0x8000ffff);
        qtest_writel(qts, PWM_RNG1(base[instance]), 0x12345678);
        qtest_writel(qts, PWM_DAT1(base[instance]), 0x89abcdef);
        qtest_writel(qts, PWM_RNG2(base[instance]), 0x23456789);
        qtest_writel(qts, PWM_DAT2(base[instance]), 0xabcdef01);
        g_assert_cmphex(qtest_readl(qts, PWM_RNG1(base[instance])), ==,
                        0x12345678);
        g_assert_cmphex(qtest_readl(qts, PWM_DAT1(base[instance])), ==,
                        0x89abcdef);
        g_assert_cmphex(qtest_readl(qts, PWM_RNG2(base[instance])), ==,
                        0x23456789);
        g_assert_cmphex(qtest_readl(qts, PWM_DAT2(base[instance])), ==,
                        0xabcdef01);
    }

    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM0_QOM_PATH,
                                         "fifo-depth"), ==, 64);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-depth"), ==, 64);
    qtest_quit(qts);
}

static void bcm2835_pwm_test_fifo_depth_status_and_clear(void)
{
    QTestState *qts = bcm2835_pwm_start();
    unsigned int i;

    for (i = 0; i < 63; i++) {
        qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 0x1000 + i);
    }
    g_assert_false(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)) &
                   PWM_STA_FULL);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 63);

    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 0x103f);
    g_assert_true(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)) &
                  PWM_STA_FULL);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 0xdeadbeef);
    g_assert_true(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)) &
                  PWM_STA_WERR);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 64);

    qtest_writel(qts, PWM_STA(RASPI4_PWM1_BASE), PWM_STA_WERR);
    g_assert_false(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)) &
                   PWM_STA_WERR);
    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE),
                 PWM_CTL_PWEN(0) | PWM_CTL_CLRF);
    g_assert_cmphex(qtest_readl(qts, PWM_CTL(RASPI4_PWM1_BASE)), ==,
                    PWM_CTL_PWEN(0));
    g_assert_cmphex(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)), ==,
                    PWM_STA_EMPT);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 0);

    /* FIF1 is write-only and reads return the bus identifier without RERR. */
    g_assert_cmphex(qtest_readl(qts, PWM_FIF1(RASPI4_PWM1_BASE)), ==,
                    0x70776d31);
    g_assert_cmphex(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)), ==,
                    PWM_STA_EMPT);
    qtest_quit(qts);
}

static void bcm2835_pwm_test_output_modes(void)
{
    QTestState *qts = bcm2835_pwm_start();

    bcm2835_pwm_program_1mhz_clock(qts);
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_DAT1(RASPI4_PWM1_BASE), 25);
    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE),
                 PWM_CTL_PWEN(0) | PWM_CTL_MSEN(0));
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "freq[0]"),
                     ==, 10000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 250000);
    g_assert_true(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)) &
                  PWM_STA_ACTIVE(0));

    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE),
                 PWM_CTL_PWEN(0) | PWM_CTL_MSEN(0) | PWM_CTL_POLA(0));
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 750000);

    /* Serial mode transmits the most-significant range bits. */
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 8);
    qtest_writel(qts, PWM_DAT1(RASPI4_PWM1_BASE), 0xa0000000);
    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE),
                 PWM_CTL_PWEN(0) | PWM_CTL_MODE(0));
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "freq[0]"),
                     ==, 125000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 250000);

    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE), PWM_CTL_SBIT(0));
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "freq[0]"),
                     ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 1000000);
    qtest_quit(qts);
}

static void bcm2835_pwm_test_immediate_fifo_claim(void)
{
    QTestState *qts = bcm2835_pwm_start();
    uint32_t ctl = PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) | PWM_CTL_MSEN(0);
    uint32_t sta;

    bcm2835_pwm_program_1mhz_clock(qts);
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 25);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 1);

    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE), ctl);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 250000);
    sta = qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_true(sta & PWM_STA_EMPT);
    g_assert_true(sta & PWM_STA_ACTIVE(0));

    qtest_clock_step(qts, 100000);
    sta = qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_true(sta & PWM_STA_GAP(0));
    g_assert_false(sta & PWM_STA_ACTIVE(0));
    qtest_writel(qts, PWM_STA(RASPI4_PWM1_BASE), PWM_STA_GAP(0));

    /* An enabled, idle channel also claims a newly written word at once. */
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 75);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 750000);
    sta = qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_false(sta & PWM_STA_GAP(0));
    g_assert_true(sta & PWM_STA_ACTIVE(0));
    qtest_quit(qts);
}

static void bcm2835_pwm_test_shared_fifo_timing(void)
{
    QTestState *qts = bcm2835_pwm_start();
    uint32_t ctl = PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) | PWM_CTL_MSEN(0) |
                   PWM_CTL_PWEN(1) | PWM_CTL_USEF(1) | PWM_CTL_MSEN(1);
    uint32_t sta;

    bcm2835_pwm_program_1mhz_clock(qts);
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_RNG2(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 25);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 75);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 50);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE), ctl);

    /* Enabling both channels immediately claims their first FIFO words. */
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 2);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 250000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[1]"),
                     ==, 750000);
    qtest_clock_step(qts, 99999);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 2);
    qtest_clock_step(qts, 1);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 500000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[1]"),
                     ==, 1000000);
    sta = qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_true(sta & PWM_STA_ACTIVE(0));
    g_assert_true(sta & PWM_STA_ACTIVE(1));

    qtest_clock_step(qts, 100000);
    sta = qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_true(sta & PWM_STA_GAP(0));
    g_assert_true(sta & PWM_STA_GAP(1));
    g_assert_false(sta & PWM_STA_ACTIVE(0));
    g_assert_false(sta & PWM_STA_ACTIVE(1));
    qtest_quit(qts);
}

static void bcm2835_pwm_test_dma_pacing(void)
{
    const unsigned int words = 16;
    QTestState *qts = bcm2835_pwm_start();
    uint32_t ctl = PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) | PWM_CTL_MSEN(0) |
                   PWM_CTL_PWEN(1) | PWM_CTL_USEF(1) | PWM_CTL_MSEN(1);
    unsigned int i;

    bcm2835_pwm_program_1mhz_clock(qts);
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_RNG2(RASPI4_PWM1_BASE), 100);
    bcm2835_pwm_write_dma_cb(qts, 1, 0x7e20c818, words);
    for (i = 0; i < words; i++) {
        qtest_writel(qts, DMA_SOURCE + i * sizeof(uint32_t), i * 5);
    }

    qtest_writel(qts, PWM_DMAC(RASPI4_PWM1_BASE),
                 PWM_DMAC_ENAB | PWM_DMAC_PANIC(7) | PWM_DMAC_DREQ(7));
    qtest_writel(qts, DMA_ADDR, DMA_CB);
    qtest_writel(qts, DMA_CS, DMA_ACTIVE);

    /* DREQ1 drops after the 64-word FIFO reaches eight queued words. */
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 8);
    g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==, 8 * 4);
    g_assert_true(qtest_readl(qts, DMA_CS) & DMA_ISHELD);

    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE), ctl);
    g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==, 6 * 4);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 8);
    for (i = 0; i < 3; i++) {
        qtest_clock_step(qts, 100000);
        g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==,
                        (4 - 2 * i) * 4);
        g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                             "fifo-level"), ==, 8);
    }
    g_assert_false(qtest_readl(qts, DMA_CS) & DMA_ACTIVE);
    g_assert_true(qtest_readl(qts, DMA_CS) & DMA_END);
    qtest_clock_step(qts, 100000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 6);

    qtest_quit(qts);
}

static void bcm2835_pwm_test_pwm0_dreq5(void)
{
    QTestState *qts = bcm2835_pwm_start();

    bcm2835_pwm_write_dma_cb(qts, 5, 0x7e20c018, 1);
    qtest_writel(qts, DMA_SOURCE, 0x12345678);
    qtest_writel(qts, PWM_DMAC(RASPI4_PWM0_BASE),
                 PWM_DMAC_ENAB | PWM_DMAC_DREQ(0));
    qtest_writel(qts, DMA_ADDR, DMA_CB);
    qtest_writel(qts, DMA_CS, DMA_ACTIVE);

    g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==, 0);
    g_assert_true(qtest_readl(qts, DMA_CS) & DMA_END);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM0_QOM_PATH,
                                         "fifo-level"), ==, 1);
    qtest_quit(qts);
}

static void bcm2835_pwm_test_reset(void)
{
    QTestState *qts = bcm2835_pwm_start();

    bcm2835_pwm_program_1mhz_clock(qts);
    qtest_writel(qts, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_RNG2(RASPI4_PWM1_BASE), 100);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 25);
    qtest_writel(qts, PWM_FIF1(RASPI4_PWM1_BASE), 75);
    qtest_writel(qts, PWM_DMAC(RASPI4_PWM1_BASE),
                 PWM_DMAC_ENAB | PWM_DMAC_DREQ(3));
    qtest_writel(qts, PWM_CTL(RASPI4_PWM1_BASE),
                 PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) |
                 PWM_CTL_PWEN(1) | PWM_CTL_USEF(1));
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "freq[0]"),
                     ==, 10000);

    qtest_system_reset(qts);
    g_assert_cmphex(qtest_readl(qts, PWM_CTL(RASPI4_PWM1_BASE)), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PWM_STA(RASPI4_PWM1_BASE)), ==,
                    PWM_STA_EMPT);
    g_assert_cmphex(qtest_readl(qts, PWM_DMAC(RASPI4_PWM1_BASE)), ==,
                    PWM_DMAC_RESET);
    g_assert_cmphex(qtest_readl(qts, PWM_RNG1(RASPI4_PWM1_BASE)), ==, 32);
    g_assert_cmphex(qtest_readl(qts, PWM_RNG2(RASPI4_PWM1_BASE)), ==, 32);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "freq[0]"),
                     ==, 0);
    g_assert_cmpuint(bcm2835_pwm_qom_get(qts, PWM1_QOM_PATH, "duty[0]"),
                     ==, 0);
    qtest_quit(qts);
}

#ifndef _WIN32
static void bcm2835_pwm_wait_for_migration(QTestState *qts)
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
    g_error("timed out waiting for BCM2711 PWM migration");
}

static void bcm2835_pwm_test_migration(void)
{
    const uint32_t ctl = PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) |
                         PWM_CTL_MSEN(0) | PWM_CTL_PWEN(1) |
                         PWM_CTL_USEF(1) | PWM_CTL_MSEN(1);
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = bcm2835_pwm_start();
    QTestState *destination;
    uint32_t sta;

    bcm2835_pwm_program_1mhz_clock(source);
    qtest_writel(source, PWM_RNG1(RASPI4_PWM1_BASE), 100);
    qtest_writel(source, PWM_RNG2(RASPI4_PWM1_BASE), 100);
    qtest_writel(source, PWM_FIF1(RASPI4_PWM1_BASE), 25);
    qtest_writel(source, PWM_FIF1(RASPI4_PWM1_BASE), 75);
    qtest_writel(source, PWM_FIF1(RASPI4_PWM1_BASE), 50);
    qtest_writel(source, PWM_FIF1(RASPI4_PWM1_BASE), 100);
    qtest_writel(source, PWM_DMAC(RASPI4_PWM1_BASE),
                 PWM_DMAC_ENAB | PWM_DMAC_PANIC(7) | PWM_DMAC_DREQ(2));
    qtest_writel(source, PWM_CTL(RASPI4_PWM1_BASE), ctl);

    qtest_clock_step(source, 50000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(source, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 2);
    g_assert_cmpuint(bcm2835_pwm_qom_get(source, PWM1_QOM_PATH, "duty[0]"),
                     ==, 250000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(source, PWM1_QOM_PATH, "duty[1]"),
                     ==, 750000);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2835-pwm-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);
    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    bcm2835_pwm_wait_for_migration(source);

    destination_args = g_strdup_printf(
        "-machine raspi4b -nic none -incoming %s", uri);
    destination = qtest_init(destination_args);
    bcm2835_pwm_wait_for_migration(destination);

    g_assert_cmphex(qtest_readl(destination, PWM_CTL(RASPI4_PWM1_BASE)), ==,
                    ctl);
    g_assert_cmphex(qtest_readl(destination, PWM_DMAC(RASPI4_PWM1_BASE)), ==,
                    PWM_DMAC_ENAB | PWM_DMAC_PANIC(7) | PWM_DMAC_DREQ(2));
    g_assert_cmphex(qtest_readl(destination, PWM_RNG1(RASPI4_PWM1_BASE)), ==,
                    100);
    g_assert_cmphex(qtest_readl(destination, PWM_RNG2(RASPI4_PWM1_BASE)), ==,
                    100);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 2);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "duty[0]"), ==, 250000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "duty[1]"), ==, 750000);
    sta = qtest_readl(destination, PWM_STA(RASPI4_PWM1_BASE));
    g_assert_true(sta & PWM_STA_ACTIVE(0));
    g_assert_true(sta & PWM_STA_ACTIVE(1));

    /* Post-load output reconstruction leaves DREQ1 usable immediately. */
    bcm2835_pwm_write_dma_cb(destination, 1, 0x7e20c818, 1);
    qtest_writel(destination, DMA_SOURCE, 0x12345678);
    qtest_writel(destination, DMA_ADDR, DMA_CB);
    qtest_writel(destination, DMA_CS, DMA_ACTIVE);
    g_assert_cmphex(qtest_readl(destination, DMA_TXFR_LEN), ==, 0);
    g_assert_true(qtest_readl(destination, DMA_CS) & DMA_END);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 3);

    qtest_qmp_assert_success(destination, "{ 'execute': 'cont' }");
    g_assert_cmpint(qtest_clock_step(destination, 49999), ==, 49999);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 3);
    g_assert_cmpint(qtest_clock_step_next(destination), ==, 100000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "fifo-level"), ==, 1);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "duty[0]"), ==, 500000);
    g_assert_cmpuint(bcm2835_pwm_qom_get(destination, PWM1_QOM_PATH,
                                         "duty[1]"), ==, 1000000);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2835/pwm/reset_and_registers",
                   bcm2835_pwm_test_reset_and_registers);
    qtest_add_func("/bcm2835/pwm/fifo_depth_status_and_clear",
                   bcm2835_pwm_test_fifo_depth_status_and_clear);
    qtest_add_func("/bcm2835/pwm/output_modes",
                   bcm2835_pwm_test_output_modes);
    qtest_add_func("/bcm2835/pwm/immediate_fifo_claim",
                   bcm2835_pwm_test_immediate_fifo_claim);
    qtest_add_func("/bcm2835/pwm/shared_fifo_timing",
                   bcm2835_pwm_test_shared_fifo_timing);
    qtest_add_func("/bcm2835/pwm/dma_pacing",
                   bcm2835_pwm_test_dma_pacing);
    qtest_add_func("/bcm2835/pwm/pwm0_dreq5",
                   bcm2835_pwm_test_pwm0_dreq5);
    qtest_add_func("/bcm2835/pwm/reset", bcm2835_pwm_test_reset);
#ifndef _WIN32
    qtest_add_func("/bcm2835/pwm/migration", bcm2835_pwm_test_migration);
#endif
    return g_test_run();
}
