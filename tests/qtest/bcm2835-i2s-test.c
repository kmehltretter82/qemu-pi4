/*
 * QTest testcase for the BCM2835 PCM / I2S controller on Raspberry Pi 4.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qemu/bitops.h"
#include "qobject/qdict.h"

#define RASPI4_I2S_BASE             0xfe203000
#define RASPI4_CPRMAN_BASE          0xfe101000
#define RASPI4_DMA5_BASE            0xfe007500

#define PCM_CS_A                    (RASPI4_I2S_BASE + 0x00)
#define PCM_FIFO_A                  (RASPI4_I2S_BASE + 0x04)
#define PCM_MODE_A                  (RASPI4_I2S_BASE + 0x08)
#define PCM_RXC_A                   (RASPI4_I2S_BASE + 0x0c)
#define PCM_TXC_A                   (RASPI4_I2S_BASE + 0x10)
#define PCM_DREQ_A                  (RASPI4_I2S_BASE + 0x14)
#define PCM_INTEN_A                 (RASPI4_I2S_BASE + 0x18)
#define PCM_INTSTC_A                (RASPI4_I2S_BASE + 0x1c)
#define PCM_GRAY                    (RASPI4_I2S_BASE + 0x20)

#define PCM_CS_STBY                 BIT(25)
#define PCM_CS_SYNC                 BIT(24)
#define PCM_CS_RXF                  BIT(22)
#define PCM_CS_TXE                  BIT(21)
#define PCM_CS_RXD                  BIT(20)
#define PCM_CS_TXD                  BIT(19)
#define PCM_CS_RXR                  BIT(18)
#define PCM_CS_TXW                  BIT(17)
#define PCM_CS_RXERR                BIT(16)
#define PCM_CS_TXERR                BIT(15)
#define PCM_CS_DMAEN                BIT(9)
#define PCM_CS_RXTHR(_n)            ((_n) << 7)
#define PCM_CS_TXTHR(_n)            ((_n) << 5)
#define PCM_CS_RXCLR                BIT(4)
#define PCM_CS_TXCLR                BIT(3)
#define PCM_CS_TXON                 BIT(2)
#define PCM_CS_RXON                 BIT(1)
#define PCM_CS_EN                   BIT(0)

#define PCM_MODE_CLK_DIS            BIT(28)
#define PCM_MODE_FRXP               BIT(25)
#define PCM_MODE_FTXP               BIT(24)
#define PCM_MODE_CLKM               BIT(23)
#define PCM_MODE_FSM                BIT(21)
#define PCM_MODE_FLEN(_n)           ((_n) << 10)

#define PCM_CH1_WEX                 BIT(31)
#define PCM_CH1_EN                  BIT(30)
#define PCM_CH1_POS(_n)             ((_n) << 20)
#define PCM_CH2_WEX                 BIT(15)
#define PCM_CH2_EN                  BIT(14)
#define PCM_CH2_POS(_n)             ((_n) << 4)

#define PCM_DREQ_RESET              0x10303020U
#define PCM_DREQ_TX(_n)             ((_n) << 8)
#define PCM_DREQ_RX(_n)             (_n)

#define PCM_INT_RXERR               BIT(3)
#define PCM_INT_TXERR               BIT(2)
#define PCM_INT_RXR                 BIT(1)
#define PCM_INT_TXW                 BIT(0)

#define PCM_GIC_IRQ                 119

#define CM_PCMCTL                   (RASPI4_CPRMAN_BASE + 0x098)
#define CM_PCMDIV                   (RASPI4_CPRMAN_BASE + 0x09c)
#define CM_PLLD                     (RASPI4_CPRMAN_BASE + 0x10c)
#define A2W_PLLD_ANA1               (RASPI4_CPRMAN_BASE + 0x1054)
#define A2W_PLLD_ANA2               (RASPI4_CPRMAN_BASE + 0x1058)
#define A2W_PLLD_ANA3               (RASPI4_CPRMAN_BASE + 0x105c)
#define A2W_PLLD_CTRL               (RASPI4_CPRMAN_BASE + 0x1140)
#define A2W_PLLD_FRAC               (RASPI4_CPRMAN_BASE + 0x1240)
#define A2W_PLLD_PER                (RASPI4_CPRMAN_BASE + 0x1540)
#define CM_PASSWORD                 0x5a000000U
#define CM_ENABLE                   BIT(4)
#define CM_SRC_XOSC                 1
#define CM_SRC_PLLD                 6
#define CM_DIV_17_578125            0x11940
#define CM_DIV_244_140625           0xf4240

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

#define PCM_FRAME_NS                20833

static QTestState *bcm2835_i2s_start(void)
{
    return qtest_init("-machine raspi4b -nic none");
}

static void bcm2835_i2s_program_48khz_clock(QTestState *qts)
{
    /* 54 MHz XOSC / 17.578125 = 3.072 MHz; 64 clocks = 48 kHz. */
    qtest_writel(qts, CM_PCMDIV, CM_PASSWORD | CM_DIV_17_578125);
    qtest_writel(qts, CM_PCMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_XOSC);
}

static void bcm2835_i2s_program_circle_48khz_clock(QTestState *qts)
{
    /* Pi 4 firmware provides PLLD_PER at 750 MHz. */
    qtest_writel(qts, CM_PCMDIV, CM_PASSWORD | CM_DIV_244_140625);
    qtest_writel(qts, CM_PCMCTL,
                 CM_PASSWORD | CM_ENABLE | CM_SRC_PLLD);
}

static void bcm2835_i2s_configure_stereo(QTestState *qts)
{
    uint32_t channels = PCM_CH1_WEX | PCM_CH1_EN | PCM_CH1_POS(1) |
                        PCM_CH2_WEX | PCM_CH2_EN | PCM_CH2_POS(33);

    qtest_writel(qts, PCM_MODE_A, PCM_MODE_FLEN(63) | 32);
    qtest_writel(qts, PCM_RXC_A, channels);
    qtest_writel(qts, PCM_TXC_A, channels);
}

static void bcm2835_i2s_test_reset_and_registers(void)
{
    QTestState *qts = bcm2835_i2s_start();
    uint32_t cs;

    cs = qtest_readl(qts, PCM_CS_A);
    g_assert_cmphex(cs, ==, PCM_CS_TXE | PCM_CS_TXD | PCM_CS_TXW);
    g_assert_cmphex(qtest_readl(qts, PCM_DREQ_A), ==, PCM_DREQ_RESET);
    g_assert_cmphex(qtest_readl(qts, PCM_MODE_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_RXC_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_TXC_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_INTEN_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_INTSTC_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_GRAY), ==, 0);

    qtest_writel(qts, PCM_MODE_A, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, PCM_MODE_A), ==, 0x1fffffff);
    qtest_writel(qts, PCM_RXC_A, UINT32_MAX);
    qtest_writel(qts, PCM_TXC_A, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, PCM_RXC_A), ==, 0xffffffff);
    g_assert_cmphex(qtest_readl(qts, PCM_TXC_A), ==, 0xffffffff);
    qtest_writel(qts, PCM_DREQ_A, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, PCM_DREQ_A), ==, 0x7f7f7f7f);
    qtest_writel(qts, PCM_INTEN_A, UINT32_MAX);
    g_assert_cmphex(qtest_readl(qts, PCM_INTEN_A), ==, 0xf);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_bcm2711_clock_profile(void)
{
    QTestState *qts = bcm2835_i2s_start();

    /* Pi 400 firmware/debugfs state: PLLD=3 GHz, PLLD_PER=750 MHz. */
    g_assert_cmphex(qtest_readl(qts, CM_PLLD), ==, 0x00000000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_CTRL), ==, 0x00021037);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_FRAC), ==, 0x0008e390);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA1), ==, 0x00118000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA2), ==, 0x00d00000);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_ANA3), ==, 0x00000052);
    g_assert_cmphex(qtest_readl(qts, A2W_PLLD_PER), ==, 4);

    bcm2835_i2s_program_circle_48khz_clock(qts);
    bcm2835_i2s_configure_stereo(qts);
    qtest_writel(qts, PCM_FIFO_A, 0x11111111);
    qtest_writel(qts, PCM_FIFO_A, 0x22222222);
    qtest_writel(qts, PCM_CS_A,
                 PCM_CS_STBY | PCM_CS_EN | PCM_CS_TXON);

    qtest_clock_step(qts, PCM_FRAME_NS - 1);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_fifo_errors_irq_and_control_timing(void)
{
    QTestState *qts = bcm2835_i2s_start();
    uint32_t cs;
    unsigned int i;

    qtest_irq_intercept_in(qts, "/machine/soc/peripherals");
    bcm2835_i2s_program_48khz_clock(qts);
    qtest_writel(qts, PCM_CS_A, PCM_CS_TXTHR(1));

    for (i = 0; i < 15; i++) {
        qtest_writel(qts, PCM_FIFO_A, 0x100 + i);
    }
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXW);
    qtest_writel(qts, PCM_FIFO_A, 0x10f);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXW);

    for (i = 16; i < 64; i++) {
        qtest_writel(qts, PCM_FIFO_A, 0x100 + i);
    }
    cs = qtest_readl(qts, PCM_CS_A);
    g_assert_false(cs & PCM_CS_TXD);
    g_assert_false(cs & PCM_CS_TXE);

    qtest_writel(qts, PCM_INTSTC_A, 0xf);
    qtest_writel(qts, PCM_INTEN_A, PCM_INT_TXERR);
    qtest_writel(qts, PCM_FIFO_A, 0xdeadbeef);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXERR);
    g_assert_true(qtest_readl(qts, PCM_INTSTC_A) & PCM_INT_TXERR);
    g_assert_true(qtest_get_irq(qts, PCM_GIC_IRQ));

    qtest_writel(qts, PCM_CS_A, PCM_CS_TXERR);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXERR);
    g_assert_true(qtest_get_irq(qts, PCM_GIC_IRQ));
    qtest_writel(qts, PCM_INTSTC_A, PCM_INT_TXERR);
    g_assert_false(qtest_get_irq(qts, PCM_GIC_IRQ));

    /* FIFO clear and SYNC echo both complete after two PCM bit clocks. */
    qtest_writel(qts, PCM_CS_A, PCM_CS_TXCLR);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    qtest_clock_step(qts, 651);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);

    qtest_writel(qts, PCM_CS_A, PCM_CS_SYNC);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);
    qtest_clock_step(qts, 651);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);

    /* A later write can cancel an in-flight transition back to zero. */
    qtest_writel(qts, PCM_CS_A, 0);
    qtest_writel(qts, PCM_CS_A, PCM_CS_SYNC);
    qtest_clock_step(qts, 652);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);
    qtest_writel(qts, PCM_CS_A, 0);
    qtest_clock_step(qts, 651);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);
    qtest_clock_step(qts, 1);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);

    qtest_writel(qts, PCM_INTSTC_A, 0xf);
    g_assert_cmphex(qtest_readl(qts, PCM_FIFO_A), ==, 0);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXERR);
    g_assert_true(qtest_readl(qts, PCM_INTSTC_A) & PCM_INT_RXERR);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_clock_disable_holds_control_actions(void)
{
    QTestState *qts = bcm2835_i2s_start();

    bcm2835_i2s_program_48khz_clock(qts);
    qtest_writel(qts, PCM_FIFO_A, 0x12345678);
    qtest_writel(qts, PCM_CS_A, PCM_CS_TXCLR | PCM_CS_SYNC);
    qtest_writel(qts, PCM_MODE_A, PCM_MODE_CLK_DIS);

    qtest_clock_step(qts, 10000);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);

    qtest_writel(qts, PCM_MODE_A, 0);
    qtest_clock_step(qts, 651);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_SYNC);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_direction_change_preserves_frame_phase(void)
{
    QTestState *qts = bcm2835_i2s_start();
    uint32_t active = PCM_CS_STBY | PCM_CS_EN | PCM_CS_TXON;

    bcm2835_i2s_program_48khz_clock(qts);
    bcm2835_i2s_configure_stereo(qts);
    qtest_writel(qts, PCM_FIFO_A, 0x11111111);
    qtest_writel(qts, PCM_FIFO_A, 0x22222222);
    qtest_writel(qts, PCM_CS_A, active);

    qtest_clock_step(qts, 10000);
    qtest_writel(qts, PCM_CS_A, active | PCM_CS_RXON);
    qtest_clock_step(qts, PCM_FRAME_NS - 10001);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_configured_slave_clock(void)
{
    QTestState *qts = qtest_init(
        "-machine raspi4b -nic none "
        "-global bcm2835-i2s.slave-clock-frequency=3072000");

    bcm2835_i2s_configure_stereo(qts);
    qtest_writel(qts, PCM_MODE_A,
                 PCM_MODE_CLKM | PCM_MODE_FSM | PCM_MODE_FLEN(63) | 32);
    qtest_writel(qts, PCM_FIFO_A, 0x11111111);
    qtest_writel(qts, PCM_FIFO_A, 0x22222222);
    qtest_writel(qts, PCM_CS_A,
                 PCM_CS_STBY | PCM_CS_EN | PCM_CS_TXON | PCM_CS_RXON);

    qtest_clock_step(qts, PCM_FRAME_NS - 1);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);
    qtest_clock_step(qts, 1);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_frame_timing_and_receive(void)
{
    QTestState *qts = bcm2835_i2s_start();
    uint32_t cs;

    bcm2835_i2s_program_48khz_clock(qts);
    bcm2835_i2s_configure_stereo(qts);
    qtest_writel(qts, PCM_FIFO_A, 0x00123456);
    qtest_writel(qts, PCM_FIFO_A, 0x00abcdef);
    qtest_writel(qts, PCM_CS_A, PCM_CS_STBY | PCM_CS_EN | PCM_CS_TXON |
                                   PCM_CS_RXON);

    qtest_clock_step(qts, PCM_FRAME_NS - 1);
    cs = qtest_readl(qts, PCM_CS_A);
    g_assert_false(cs & PCM_CS_TXE);
    g_assert_false(cs & PCM_CS_RXD);

    qtest_clock_step(qts, 1);
    cs = qtest_readl(qts, PCM_CS_A);
    g_assert_true(cs & PCM_CS_TXE);
    g_assert_true(cs & PCM_CS_RXD);
    g_assert_false(cs & PCM_CS_TXERR);
    g_assert_cmphex(qtest_readl(qts, PCM_FIFO_A), ==, 0);
    g_assert_cmphex(qtest_readl(qts, PCM_FIFO_A), ==, 0);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);

    qtest_clock_step(qts, PCM_FRAME_NS);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXERR);

    qtest_quit(qts);
}

static void bcm2835_i2s_write_dma_cb(QTestState *qts, uint32_t words)
{
    qtest_writel(qts, DMA_CB,
                 DMA_S_INC | DMA_D_DREQ | DMA_PERMAP(2));
    qtest_writel(qts, DMA_CB + 4, DMA_SOURCE);
    qtest_writel(qts, DMA_CB + 8, 0x7e203004);
    qtest_writel(qts, DMA_CB + 12, words * sizeof(uint32_t));
    qtest_writel(qts, DMA_CB + 16, 0);
    qtest_writel(qts, DMA_CB + 20, 0);
}

static void bcm2835_i2s_test_dma_pacing(void)
{
    const unsigned int words = 64;
    QTestState *qts = bcm2835_i2s_start();
    uint32_t dma_cs;
    unsigned int i;

    bcm2835_i2s_program_48khz_clock(qts);
    bcm2835_i2s_configure_stereo(qts);
    bcm2835_i2s_write_dma_cb(qts, words);
    for (i = 0; i < words; i++) {
        qtest_writel(qts, DMA_SOURCE + i * sizeof(uint32_t),
                     0x100000 + i);
    }

    qtest_writel(qts, PCM_DREQ_A, PCM_DREQ_TX(48) | PCM_DREQ_RX(32));
    qtest_writel(qts, PCM_CS_A, PCM_CS_STBY | PCM_CS_EN | PCM_CS_DMAEN);
    qtest_writel(qts, DMA_ADDR, DMA_CB);
    qtest_writel(qts, DMA_CS, DMA_ACTIVE);

    /* DREQ drops at 48 FIFO words, retaining the final 16 source words. */
    g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==, 16 * 4);
    dma_cs = qtest_readl(qts, DMA_CS);
    g_assert_true(dma_cs & DMA_ACTIVE);
    g_assert_true(dma_cs & DMA_ISHELD);

    qtest_writel(qts, PCM_CS_A,
                 PCM_CS_STBY | PCM_CS_EN | PCM_CS_DMAEN | PCM_CS_TXON);
    for (i = 0; i < 8; i++) {
        /* One stereo frame drains two words; DMA refills exactly two. */
        qtest_clock_step(qts, 22000);
        g_assert_cmphex(qtest_readl(qts, DMA_TXFR_LEN), ==,
                        (14 - 2 * i) * 4);
    }

    dma_cs = qtest_readl(qts, DMA_CS);
    g_assert_false(dma_cs & DMA_ACTIVE);
    g_assert_true(dma_cs & DMA_END);

    /* 48 queued words cover 24 frames; the following frame underflows. */
    qtest_clock_step(qts, 25 * 20834);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXERR);

    qtest_quit(qts);
}

static void bcm2835_i2s_test_packed_mode(void)
{
    QTestState *qts = bcm2835_i2s_start();
    uint32_t channels = PCM_CH1_EN | PCM_CH1_POS(1) |
                        PCM_CH2_EN | PCM_CH2_POS(17) | 8 | (8 << 16);

    bcm2835_i2s_program_48khz_clock(qts);
    qtest_writel(qts, PCM_MODE_A,
                 PCM_MODE_FTXP | PCM_MODE_FRXP | PCM_MODE_FLEN(31) | 16);
    qtest_writel(qts, PCM_TXC_A, channels);
    qtest_writel(qts, PCM_RXC_A, channels);
    qtest_writel(qts, PCM_FIFO_A, 0x1234abcd);
    qtest_writel(qts, PCM_CS_A, PCM_CS_STBY | PCM_CS_EN |
                                   PCM_CS_TXON | PCM_CS_RXON);

    /* One FIFO word carries both 16-bit channels in packed mode. */
    qtest_clock_step(qts, 10417);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_TXE);
    g_assert_true(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);
    g_assert_cmphex(qtest_readl(qts, PCM_FIFO_A), ==, 0);
    g_assert_false(qtest_readl(qts, PCM_CS_A) & PCM_CS_RXD);

    qtest_quit(qts);
}

#ifndef _WIN32
static void bcm2835_i2s_wait_for_migration(QTestState *qts)
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

    g_error("timed out waiting for BCM2835 I2S migration");
}

static void bcm2835_i2s_test_migration(void)
{
    const uint32_t channels = PCM_CH1_WEX | PCM_CH1_EN | PCM_CH1_POS(1) |
                              PCM_CH2_WEX | PCM_CH2_EN | PCM_CH2_POS(33);
    const uint32_t active_cs = PCM_CS_TXTHR(1) | PCM_CS_STBY |
                               PCM_CS_DMAEN | PCM_CS_EN |
                               PCM_CS_TXON | PCM_CS_RXON;
    const int64_t control_delay = 652;
    const int64_t control_deadline = PCM_FRAME_NS + control_delay;
    const int64_t second_frame = 2 * PCM_FRAME_NS;
    g_autoptr(GError) error = NULL;
    g_autofree char *tmpdir = NULL;
    g_autofree char *state_path = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *destination_args = NULL;
    QTestState *source = bcm2835_i2s_start();
    QTestState *destination;
    uint32_t cs;

    qtest_irq_intercept_in(source, "/machine/soc/peripherals");
    bcm2835_i2s_program_48khz_clock(source);
    bcm2835_i2s_configure_stereo(source);
    qtest_writel(source, PCM_DREQ_A,
                 PCM_DREQ_TX(48) | PCM_DREQ_RX(0));
    qtest_writel(source, PCM_INTEN_A, PCM_INT_TXW);
    qtest_writel(source, PCM_FIFO_A, 0x11111111);
    qtest_writel(source, PCM_FIFO_A, 0x22222222);
    qtest_writel(source, PCM_FIFO_A, 0x33333333);
    qtest_writel(source, PCM_FIFO_A, 0x44444444);
    qtest_writel(source, PCM_CS_A, active_cs);

    /* One frame consumes two TX words and produces two zero RX words. */
    g_assert_cmpint(qtest_clock_step(source, PCM_FRAME_NS), ==,
                    PCM_FRAME_NS);
    cs = qtest_readl(source, PCM_CS_A);
    g_assert_false(cs & PCM_CS_TXE);
    g_assert_true(cs & PCM_CS_RXD);
    g_assert_true(qtest_readl(source, PCM_INTSTC_A) & PCM_INT_TXW);
    g_assert_true(qtest_get_irq(source, PCM_GIC_IRQ));

    /* Leave both short control actions pending across migration. */
    qtest_writel(source, PCM_CS_A,
                 active_cs | PCM_CS_RXCLR | PCM_CS_SYNC);
    cs = qtest_readl(source, PCM_CS_A);
    g_assert_true(cs & PCM_CS_RXD);
    g_assert_false(cs & PCM_CS_SYNC);

    qtest_qmp_assert_success(source, "{ 'execute': 'stop' }");
    tmpdir = g_dir_make_tmp("bcm2835-i2s-migration-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);
    state_path = g_build_filename(tmpdir, "state", NULL);
    uri = g_strdup_printf("file:%s", state_path);

    qtest_qmp_assert_success(source,
        "{ 'execute': 'migrate', 'arguments': { 'uri': %s } }", uri);
    bcm2835_i2s_wait_for_migration(source);

    destination_args = g_strdup_printf(
        "-machine raspi4b -nic none -incoming %s", uri);
    destination = qtest_init(destination_args);
    bcm2835_i2s_wait_for_migration(destination);
    qtest_irq_intercept_in(destination, "/machine/soc/peripherals");

    g_assert_cmphex(qtest_readl(destination, PCM_MODE_A), ==,
                    PCM_MODE_FLEN(63) | 32);
    g_assert_cmphex(qtest_readl(destination, PCM_RXC_A), ==, channels);
    g_assert_cmphex(qtest_readl(destination, PCM_TXC_A), ==, channels);
    g_assert_cmphex(qtest_readl(destination, PCM_DREQ_A), ==,
                    PCM_DREQ_TX(48) | PCM_DREQ_RX(0));
    g_assert_cmphex(qtest_readl(destination, PCM_INTEN_A), ==,
                    PCM_INT_TXW);
    g_assert_true(qtest_readl(destination, PCM_INTSTC_A) & PCM_INT_TXW);
    cs = qtest_readl(destination, PCM_CS_A);
    g_assert_true(cs & PCM_CS_RXD);
    g_assert_false(cs & PCM_CS_TXE);
    g_assert_false(cs & PCM_CS_SYNC);

    qtest_qmp_assert_success(destination, "{ 'execute': 'cont' }");

    /* The two-clock FIFO-clear and SYNC deadlines retain their phase. */
    g_assert_cmpint(qtest_clock_step(destination, control_deadline - 1), ==,
                    control_deadline - 1);
    cs = qtest_readl(destination, PCM_CS_A);
    g_assert_true(cs & PCM_CS_RXD);
    g_assert_false(cs & PCM_CS_SYNC);
    g_assert_cmpint(qtest_clock_step_next(destination), ==,
                    control_deadline);
    cs = qtest_readl(destination, PCM_CS_A);
    g_assert_false(cs & PCM_CS_RXD);
    g_assert_true(cs & PCM_CS_SYNC);

    qtest_writel(destination, PCM_INTSTC_A, PCM_INT_TXW);
    g_assert_false(qtest_get_irq(destination, PCM_GIC_IRQ));
    qtest_writel(destination, PCM_CS_A,
                 (active_cs & ~PCM_CS_DMAEN) | PCM_CS_SYNC);
    g_assert_true(qtest_get_irq(destination, PCM_GIC_IRQ));
    qtest_writel(destination, PCM_CS_A, active_cs | PCM_CS_SYNC);

    /* The frame timer also resumes at its original absolute deadline. */
    g_assert_cmpint(qtest_clock_step(
                        destination,
                        second_frame - control_deadline - 1),
                    ==, second_frame - 1);
    cs = qtest_readl(destination, PCM_CS_A);
    g_assert_false(cs & PCM_CS_TXE);
    g_assert_false(cs & PCM_CS_RXD);
    g_assert_cmpint(qtest_clock_step_next(destination), ==, second_frame);
    cs = qtest_readl(destination, PCM_CS_A);
    g_assert_true(cs & PCM_CS_TXE);
    g_assert_true(cs & PCM_CS_RXD);

    /* Post-load output reconstruction must make DREQ 2 usable immediately. */
    bcm2835_i2s_write_dma_cb(destination, 1);
    qtest_writel(destination, DMA_SOURCE, 0x55555555);
    qtest_writel(destination, DMA_ADDR, DMA_CB);
    qtest_writel(destination, DMA_CS, DMA_ACTIVE);
    g_assert_cmphex(qtest_readl(destination, DMA_TXFR_LEN), ==, 0);
    g_assert_true(qtest_readl(destination, DMA_CS) & DMA_END);
    g_assert_false(qtest_readl(destination, PCM_CS_A) & PCM_CS_TXE);

    qtest_quit(destination);
    qtest_quit(source);
    g_assert_cmpint(g_unlink(state_path), ==, 0);
    g_assert_cmpint(g_rmdir(tmpdir), ==, 0);
}
#endif

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/bcm2835/i2s/reset_and_registers",
                   bcm2835_i2s_test_reset_and_registers);
    qtest_add_func("/bcm2835/i2s/bcm2711_clock_profile",
                   bcm2835_i2s_test_bcm2711_clock_profile);
    qtest_add_func("/bcm2835/i2s/fifo_errors_irq_and_control_timing",
                   bcm2835_i2s_test_fifo_errors_irq_and_control_timing);
    qtest_add_func("/bcm2835/i2s/frame_timing_and_receive",
                   bcm2835_i2s_test_frame_timing_and_receive);
    qtest_add_func("/bcm2835/i2s/clock_disable_holds_control_actions",
                   bcm2835_i2s_test_clock_disable_holds_control_actions);
    qtest_add_func("/bcm2835/i2s/direction_change_preserves_frame_phase",
                   bcm2835_i2s_test_direction_change_preserves_frame_phase);
    qtest_add_func("/bcm2835/i2s/configured_slave_clock",
                   bcm2835_i2s_test_configured_slave_clock);
    qtest_add_func("/bcm2835/i2s/dma_pacing",
                   bcm2835_i2s_test_dma_pacing);
    qtest_add_func("/bcm2835/i2s/packed_mode",
                   bcm2835_i2s_test_packed_mode);
#ifndef _WIN32
    qtest_add_func("/bcm2835/i2s/migration",
                   bcm2835_i2s_test_migration);
#endif
    return g_test_run();
}
