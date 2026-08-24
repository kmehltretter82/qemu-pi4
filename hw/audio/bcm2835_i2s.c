/*
 * BCM2835 PCM / I2S controller
 *
 * Copyright (c) 2026 qemu-pi4 contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/audio/bcm2835_i2s.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"

#define BCM2835_I2S_MMIO_SIZE       0x100

#define PCM_CS_A                    0x00
#define PCM_FIFO_A                  0x04
#define PCM_MODE_A                  0x08
#define PCM_RXC_A                   0x0c
#define PCM_TXC_A                   0x10
#define PCM_DREQ_A                  0x14
#define PCM_INTEN_A                 0x18
#define PCM_INTSTC_A                0x1c
#define PCM_GRAY                    0x20

#define PCM_CS_STBY                 BIT(25)
#define PCM_CS_SYNC                 BIT(24)
#define PCM_CS_RXSEX                BIT(23)
#define PCM_CS_RXF                  BIT(22)
#define PCM_CS_TXE                  BIT(21)
#define PCM_CS_RXD                  BIT(20)
#define PCM_CS_TXD                  BIT(19)
#define PCM_CS_RXR                  BIT(18)
#define PCM_CS_TXW                  BIT(17)
#define PCM_CS_RXERR                BIT(16)
#define PCM_CS_TXERR                BIT(15)
#define PCM_CS_RXSYNC               BIT(14)
#define PCM_CS_TXSYNC               BIT(13)
#define PCM_CS_DMAEN                BIT(9)
#define PCM_CS_RXTHR_SHIFT          7
#define PCM_CS_RXTHR_MASK           (3U << PCM_CS_RXTHR_SHIFT)
#define PCM_CS_TXTHR_SHIFT          5
#define PCM_CS_TXTHR_MASK           (3U << PCM_CS_TXTHR_SHIFT)
#define PCM_CS_RXCLR                BIT(4)
#define PCM_CS_TXCLR                BIT(3)
#define PCM_CS_TXON                 BIT(2)
#define PCM_CS_RXON                 BIT(1)
#define PCM_CS_EN                   BIT(0)

#define PCM_CS_RW_MASK              (PCM_CS_STBY | PCM_CS_RXSEX | \
                                     PCM_CS_DMAEN | PCM_CS_RXTHR_MASK | \
                                     PCM_CS_TXTHR_MASK | PCM_CS_TXON | \
                                     PCM_CS_RXON | PCM_CS_EN)

#define PCM_MODE_CLK_DIS            BIT(28)
#define PCM_MODE_PDMN               BIT(27)
#define PCM_MODE_PDME               BIT(26)
#define PCM_MODE_FRXP               BIT(25)
#define PCM_MODE_FTXP               BIT(24)
#define PCM_MODE_CLKM               BIT(23)
#define PCM_MODE_CLKI               BIT(22)
#define PCM_MODE_FSM                BIT(21)
#define PCM_MODE_FSI                BIT(20)
#define PCM_MODE_FLEN_SHIFT         10
#define PCM_MODE_FLEN_MASK          (0x3ffU << PCM_MODE_FLEN_SHIFT)
#define PCM_MODE_FSLEN_MASK         0x3ffU
#define PCM_MODE_RW_MASK            (PCM_MODE_CLK_DIS | PCM_MODE_PDMN | \
                                     PCM_MODE_PDME | PCM_MODE_FRXP | \
                                     PCM_MODE_FTXP | PCM_MODE_CLKM | \
                                     PCM_MODE_CLKI | PCM_MODE_FSM | \
                                     PCM_MODE_FSI | PCM_MODE_FLEN_MASK | \
                                     PCM_MODE_FSLEN_MASK)

#define PCM_CH1_WEX                 BIT(31)
#define PCM_CH1_EN                  BIT(30)
#define PCM_CH1_POS_SHIFT           20
#define PCM_CH1_POS_MASK            (0x3ffU << PCM_CH1_POS_SHIFT)
#define PCM_CH1_WID_SHIFT           16
#define PCM_CH1_WID_MASK            (0xfU << PCM_CH1_WID_SHIFT)
#define PCM_CH2_WEX                 BIT(15)
#define PCM_CH2_EN                  BIT(14)
#define PCM_CH2_POS_SHIFT           4
#define PCM_CH2_POS_MASK            (0x3ffU << PCM_CH2_POS_SHIFT)
#define PCM_CH2_WID_MASK            0xfU
#define PCM_CHANNEL_RW_MASK         (PCM_CH1_WEX | PCM_CH1_EN | \
                                     PCM_CH1_POS_MASK | PCM_CH1_WID_MASK | \
                                     PCM_CH2_WEX | PCM_CH2_EN | \
                                     PCM_CH2_POS_MASK | PCM_CH2_WID_MASK)

#define PCM_DREQ_TX_SHIFT           8
#define PCM_DREQ_TX_MASK            (0x7fU << PCM_DREQ_TX_SHIFT)
#define PCM_DREQ_RX_MASK            0x7fU
#define PCM_DREQ_RW_MASK            0x7f7f7f7fU
#define PCM_DREQ_RESET              0x10303020U

#define PCM_INT_RXERR               BIT(3)
#define PCM_INT_TXERR               BIT(2)
#define PCM_INT_RXR                 BIT(1)
#define PCM_INT_TXW                 BIT(0)
#define PCM_INT_MASK                0xfU

#define PCM_GRAY_RXFIFOLEVEL_SHIFT  16
#define PCM_GRAY_FLUSH              BIT(2)
#define PCM_GRAY_CLR                BIT(1)
#define PCM_GRAY_EN                 BIT(0)

#define PCM_DREQ_TX                 0
#define PCM_DREQ_RX                 1

/* Bound catch-up work while still allowing the DMA timer to refill a FIFO. */
#define BCM2835_I2S_FRAME_BUDGET    256
#define BCM2835_I2S_CATCHUP_NS      2000

static const unsigned int pcm_fifo_threshold[4] = { 1, 16, 32, 64 };

static uint64_t bcm2835_i2s_bit_clock(BCM2835I2SState *s)
{
    if (s->mode & PCM_MODE_CLK_DIS) {
        return 0;
    }

    return s->mode & PCM_MODE_CLKM ? s->slave_clock_hz : s->pcm_clk_hz;
}

static unsigned int bcm2835_i2s_frame_bits(BCM2835I2SState *s)
{
    return ((s->mode & PCM_MODE_FLEN_MASK) >> PCM_MODE_FLEN_SHIFT) + 1;
}

static bool bcm2835_i2s_running(BCM2835I2SState *s)
{
    return (s->cs & PCM_CS_EN) && (s->cs & PCM_CS_STBY) &&
           (s->cs & (PCM_CS_TXON | PCM_CS_RXON)) &&
           bcm2835_i2s_bit_clock(s);
}

static unsigned int bcm2835_i2s_tx_level(BCM2835I2SState *s)
{
    return fifo32_num_used(&s->tx_fifo);
}

static unsigned int bcm2835_i2s_rx_level(BCM2835I2SState *s)
{
    return fifo32_num_used(&s->rx_fifo);
}

static bool bcm2835_i2s_txw(BCM2835I2SState *s)
{
    unsigned int select = (s->cs & PCM_CS_TXTHR_MASK) >>
                          PCM_CS_TXTHR_SHIFT;

    return bcm2835_i2s_tx_level(s) < pcm_fifo_threshold[select];
}

static bool bcm2835_i2s_rxr(BCM2835I2SState *s)
{
    unsigned int select = (s->cs & PCM_CS_RXTHR_MASK) >>
                          PCM_CS_RXTHR_SHIFT;

    return bcm2835_i2s_rx_level(s) >= pcm_fifo_threshold[select];
}

static unsigned int bcm2835_i2s_channel_count(uint32_t config)
{
    return !!(config & PCM_CH1_EN) + !!(config & PCM_CH2_EN);
}

static unsigned int bcm2835_i2s_words_per_frame(uint32_t config, bool packed)
{
    unsigned int channels = bcm2835_i2s_channel_count(config);

    return packed && channels == 2 ? 1 : channels;
}

static bool bcm2835_i2s_fifo_sync(Fifo32 *fifo, uint32_t config,
                                  bool packed)
{
    unsigned int words = bcm2835_i2s_words_per_frame(config, packed);

    return words && fifo32_num_used(fifo) % words == 0;
}

static uint32_t bcm2835_i2s_cs_read(BCM2835I2SState *s)
{
    uint32_t value = s->cs;

    if (fifo32_is_full(&s->rx_fifo)) {
        value |= PCM_CS_RXF;
    }
    if (fifo32_is_empty(&s->tx_fifo)) {
        value |= PCM_CS_TXE;
    }
    if (!fifo32_is_empty(&s->rx_fifo)) {
        value |= PCM_CS_RXD;
    }
    if (!fifo32_is_full(&s->tx_fifo)) {
        value |= PCM_CS_TXD;
    }
    if (bcm2835_i2s_rxr(s)) {
        value |= PCM_CS_RXR;
    }
    if (bcm2835_i2s_txw(s)) {
        value |= PCM_CS_TXW;
    }
    if (bcm2835_i2s_fifo_sync(&s->rx_fifo, s->rxc,
                              s->mode & PCM_MODE_FRXP)) {
        value |= PCM_CS_RXSYNC;
    }
    if (bcm2835_i2s_fifo_sync(&s->tx_fifo, s->txc,
                              s->mode & PCM_MODE_FTXP)) {
        value |= PCM_CS_TXSYNC;
    }

    return value;
}

static uint32_t bcm2835_i2s_level_interrupts(BCM2835I2SState *s)
{
    uint32_t value = 0;

    if (bcm2835_i2s_rxr(s)) {
        value |= PCM_INT_RXR;
    }
    if (bcm2835_i2s_txw(s)) {
        value |= PCM_INT_TXW;
    }
    return value;
}

static void bcm2835_i2s_update_outputs(BCM2835I2SState *s,
                                       bool latch_level_interrupts)
{
    unsigned int tx_request = (s->dreq_reg & PCM_DREQ_TX_MASK) >>
                              PCM_DREQ_TX_SHIFT;
    unsigned int rx_request = s->dreq_reg & PCM_DREQ_RX_MASK;
    bool dma_enabled = s->cs & PCM_CS_DMAEN;

    if (latch_level_interrupts) {
        s->int_status |= bcm2835_i2s_level_interrupts(s);
    }

    qemu_set_irq(s->irq, !!(s->int_status & s->inten));
    qemu_set_irq(s->dreq[PCM_DREQ_TX],
                 dma_enabled && bcm2835_i2s_tx_level(s) < tx_request);
    qemu_set_irq(s->dreq[PCM_DREQ_RX],
                 dma_enabled && bcm2835_i2s_rx_level(s) > rx_request);
}

static void bcm2835_i2s_set_error(BCM2835I2SState *s, bool tx)
{
    if (tx) {
        s->cs |= PCM_CS_TXERR;
        s->int_status |= PCM_INT_TXERR;
    } else {
        s->cs |= PCM_CS_RXERR;
        s->int_status |= PCM_INT_RXERR;
    }
}

static void bcm2835_i2s_audio_flush(BCM2835I2SState *s, int available)
{
    while (s->voice && s->audio_active && s->audio_used && available > 0) {
        unsigned int frames = MIN(s->audio_used,
                                  BCM2835_I2S_AUDIO_FRAMES - s->audio_read);
        size_t bytes = MIN((size_t)frames * 2 * sizeof(int32_t),
                           (size_t)available);
        size_t written;

        bytes -= bytes % (2 * sizeof(int32_t));
        if (!bytes) {
            return;
        }
        written = audio_be_write(s->audio_be, s->voice,
                                 &s->audio_buffer[s->audio_read * 2], bytes);
        written -= written % (2 * sizeof(int32_t));
        if (!written) {
            return;
        }
        s->audio_read = (s->audio_read +
                         written / (2 * sizeof(int32_t))) %
                        BCM2835_I2S_AUDIO_FRAMES;
        s->audio_used -= written / (2 * sizeof(int32_t));
        available -= written;
    }
}

static void bcm2835_i2s_audio_callback(void *opaque, int available)
{
    bcm2835_i2s_audio_flush(opaque, available);
}

static void bcm2835_i2s_audio_push(BCM2835I2SState *s,
                                   int32_t left, int32_t right)
{
    unsigned int write;

    if (!s->voice || !s->audio_active) {
        return;
    }

    if (s->audio_used == BCM2835_I2S_AUDIO_FRAMES) {
        s->audio_read = (s->audio_read + 1) % BCM2835_I2S_AUDIO_FRAMES;
        s->audio_used--;
    }
    write = (s->audio_read + s->audio_used) % BCM2835_I2S_AUDIO_FRAMES;
    s->audio_buffer[write * 2] = left;
    s->audio_buffer[write * 2 + 1] = right;
    s->audio_used++;
}

static void bcm2835_i2s_update_audio(BCM2835I2SState *s)
{
    uint64_t bit_clock = bcm2835_i2s_bit_clock(s);
    unsigned int frame_bits = bcm2835_i2s_frame_bits(s);
    uint64_t rate64 = frame_bits ?
        (bit_clock + frame_bits / 2) / frame_bits : 0;
    bool active;

    if (rate64 >= 1000 && rate64 <= 768000 && rate64 != s->audio_rate) {
        struct audsettings settings = {
            .freq = rate64,
            .nchannels = 2,
            .fmt = AUDIO_FORMAT_S32,
            .big_endian = HOST_BIG_ENDIAN,
        };

        trace_bcm2835_i2s_audio_rate(bit_clock, frame_bits, rate64);
        s->voice = audio_be_open_out(s->audio_be, s->voice,
                                     TYPE_BCM2835_I2S ".out", s,
                                     bcm2835_i2s_audio_callback, &settings);
        if (s->voice) {
            s->audio_rate = rate64;
        } else {
            s->audio_rate = 0;
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: could not open the audio output at %" PRIu64
                          " Hz\n", TYPE_BCM2835_I2S, rate64);
        }
        s->audio_read = 0;
        s->audio_used = 0;
    }

    active = s->voice && (s->cs & PCM_CS_EN) && (s->cs & PCM_CS_STBY) &&
             (s->cs & PCM_CS_TXON) && bit_clock &&
             rate64 == s->audio_rate;
    if (active != s->audio_active) {
        s->audio_active = active;
        audio_be_set_active_out(s->audio_be, s->voice, active);
        if (!active) {
            s->audio_read = 0;
            s->audio_used = 0;
        }
    }
}

static int64_t bcm2835_i2s_next_frame_delay(BCM2835I2SState *s)
{
    uint64_t bit_clock = bcm2835_i2s_bit_clock(s);
    uint64_t numerator = NANOSECONDS_PER_SECOND *
                         (uint64_t)bcm2835_i2s_frame_bits(s);
    uint64_t delay;

    if (!bit_clock) {
        return 0;
    }

    delay = numerator / bit_clock;
    s->frame_remainder += numerator % bit_clock;
    if (s->frame_remainder >= bit_clock) {
        delay += s->frame_remainder / bit_clock;
        s->frame_remainder %= bit_clock;
    }
    return MAX(delay, 1);
}

static void bcm2835_i2s_schedule_frame(BCM2835I2SState *s)
{
    int64_t now;
    int64_t expiry;

    if (!bcm2835_i2s_running(s)) {
        timer_del(&s->frame_timer);
        s->next_frame_ns = 0;
        return;
    }

    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (!s->next_frame_ns) {
        s->next_frame_ns = now + bcm2835_i2s_next_frame_delay(s);
    }

    /*
     * A realtime virtual-clock callback can arrive after several frames are
     * due.  Retry overdue work after a short yield so a DREQ-paced DMA timer
     * can refill the FIFO instead of permanently losing elapsed time.
     */
    expiry = s->next_frame_ns > now ? s->next_frame_ns :
             now + BCM2835_I2S_CATCHUP_NS;
    timer_mod(&s->frame_timer, expiry);
}

static int64_t bcm2835_i2s_control_delay(BCM2835I2SState *s)
{
    uint64_t bit_clock = bcm2835_i2s_bit_clock(s);

    if (!bit_clock) {
        return 0;
    }
    return MAX(DIV_ROUND_UP(2 * NANOSECONDS_PER_SECOND, bit_clock), 1);
}

static void bcm2835_i2s_schedule_control(BCM2835I2SState *s,
                                         QEMUTimer *timer)
{
    int64_t delay = bcm2835_i2s_control_delay(s);

    if (!delay) {
        timer_del(timer);
        return;
    }
    timer_mod(timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + delay);
}

static void bcm2835_i2s_reschedule(BCM2835I2SState *s, bool restart_frame)
{
    if (!bcm2835_i2s_bit_clock(s)) {
        timer_del(&s->frame_timer);
        timer_del(&s->fifo_clear_timer);
        timer_del(&s->sync_timer);
        s->next_frame_ns = 0;
        bcm2835_i2s_update_audio(s);
        return;
    }

    if (restart_frame) {
        timer_del(&s->frame_timer);
        s->next_frame_ns = 0;
        s->frame_remainder = 0;
    }
    if (!bcm2835_i2s_running(s)) {
        timer_del(&s->frame_timer);
        s->next_frame_ns = 0;
    } else if (!timer_pending(&s->frame_timer)) {
        bcm2835_i2s_schedule_frame(s);
    }
    if (!s->fifo_clear_pending) {
        timer_del(&s->fifo_clear_timer);
    } else if (!timer_pending(&s->fifo_clear_timer)) {
        bcm2835_i2s_schedule_control(s, &s->fifo_clear_timer);
    }
    if (!s->sync_pending) {
        timer_del(&s->sync_timer);
    } else if (!timer_pending(&s->sync_timer)) {
        bcm2835_i2s_schedule_control(s, &s->sync_timer);
    }
    bcm2835_i2s_update_audio(s);
}

static unsigned int bcm2835_i2s_channel_width(uint32_t config, bool channel1)
{
    unsigned int width;

    if (channel1) {
        width = ((config & PCM_CH1_WID_MASK) >> PCM_CH1_WID_SHIFT) + 8;
        if (config & PCM_CH1_WEX) {
            width += 16;
        }
    } else {
        width = (config & PCM_CH2_WID_MASK) + 8;
        if (config & PCM_CH2_WEX) {
            width += 16;
        }
    }
    return width;
}

static unsigned int bcm2835_i2s_channel_position(uint32_t config,
                                                  bool channel1)
{
    return channel1 ?
        (config & PCM_CH1_POS_MASK) >> PCM_CH1_POS_SHIFT :
        (config & PCM_CH2_POS_MASK) >> PCM_CH2_POS_SHIFT;
}

static int32_t bcm2835_i2s_expand_sample(uint32_t sample, unsigned int width)
{
    if (width >= 32) {
        return sample;
    }

    return (int32_t)(sample << (32 - width));
}

static uint32_t bcm2835_i2s_tx_pop(BCM2835I2SState *s)
{
    if (fifo32_is_empty(&s->tx_fifo)) {
        bcm2835_i2s_set_error(s, true);
        return 0;
    }
    return fifo32_pop(&s->tx_fifo);
}

static void bcm2835_i2s_transmit_frame(BCM2835I2SState *s)
{
    bool ch1 = s->txc & PCM_CH1_EN;
    bool ch2 = s->txc & PCM_CH2_EN;
    int32_t sample[2] = { 0, 0 };

    if ((s->mode & PCM_MODE_FTXP) && ch1 && ch2) {
        uint32_t packed = bcm2835_i2s_tx_pop(s);
        bool ch1_first = bcm2835_i2s_channel_position(s->txc, true) <=
                         bcm2835_i2s_channel_position(s->txc, false);
        uint16_t first = packed;
        uint16_t second = packed >> 16;
        unsigned int first_width = MIN(
            bcm2835_i2s_channel_width(s->txc, ch1_first), 16);
        unsigned int second_width = MIN(
            bcm2835_i2s_channel_width(s->txc, !ch1_first), 16);

        sample[ch1_first ? 0 : 1] =
            bcm2835_i2s_expand_sample(first, first_width);
        sample[ch1_first ? 1 : 0] =
            bcm2835_i2s_expand_sample(second, second_width);
    } else if (ch1 && ch2) {
        bool ch1_first = bcm2835_i2s_channel_position(s->txc, true) <=
                         bcm2835_i2s_channel_position(s->txc, false);
        uint32_t first = bcm2835_i2s_tx_pop(s);
        uint32_t second = bcm2835_i2s_tx_pop(s);

        sample[ch1_first ? 0 : 1] = bcm2835_i2s_expand_sample(
            first, bcm2835_i2s_channel_width(s->txc, ch1_first));
        sample[ch1_first ? 1 : 0] = bcm2835_i2s_expand_sample(
            second, bcm2835_i2s_channel_width(s->txc, !ch1_first));
    } else if (ch1 || ch2) {
        bool channel1 = ch1;

        sample[channel1 ? 0 : 1] = bcm2835_i2s_expand_sample(
            bcm2835_i2s_tx_pop(s),
            bcm2835_i2s_channel_width(s->txc, channel1));
    }

    bcm2835_i2s_audio_push(s, sample[0], sample[1]);
}

static void bcm2835_i2s_rx_push(BCM2835I2SState *s, uint32_t sample)
{
    if (fifo32_is_full(&s->rx_fifo)) {
        bcm2835_i2s_set_error(s, false);
        return;
    }
    fifo32_push(&s->rx_fifo, sample);
}

static void bcm2835_i2s_receive_frame(BCM2835I2SState *s)
{
    unsigned int channels = bcm2835_i2s_channel_count(s->rxc);

    if (!channels) {
        return;
    }
    if ((s->mode & PCM_MODE_FRXP) && channels == 2) {
        bcm2835_i2s_rx_push(s, 0);
    } else {
        while (channels--) {
            bcm2835_i2s_rx_push(s, 0);
        }
    }
}

static void bcm2835_i2s_frame_tick(void *opaque)
{
    BCM2835I2SState *s = opaque;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    unsigned int budget = BCM2835_I2S_FRAME_BUDGET;

    if (!bcm2835_i2s_running(s)) {
        s->next_frame_ns = 0;
        return;
    }

    do {
        unsigned int tx_before = bcm2835_i2s_tx_level(s);
        unsigned int rx_before = bcm2835_i2s_rx_level(s);

        if (s->cs & PCM_CS_TXON) {
            bcm2835_i2s_transmit_frame(s);
        }
        if (s->cs & PCM_CS_RXON) {
            bcm2835_i2s_receive_frame(s);
        }
        s->trace_frame_count++;
        if (!(s->trace_frame_count & 0x3ff)) {
            trace_bcm2835_i2s_frame_period(
                now, s->trace_frame_count,
                tx_before, bcm2835_i2s_tx_level(s),
                rx_before, bcm2835_i2s_rx_level(s));
        }

        s->next_frame_ns += bcm2835_i2s_next_frame_delay(s);
        budget--;

        if (s->next_frame_ns <= now &&
            (((s->cs & PCM_CS_TXON) && fifo32_is_empty(&s->tx_fifo) &&
              !(s->cs & PCM_CS_TXERR)) ||
             ((s->cs & PCM_CS_RXON) && fifo32_is_full(&s->rx_fifo) &&
              !(s->cs & PCM_CS_RXERR)))) {
            /*
             * Raising DREQ can synchronously service a bounded DMA slice.
             * Continue the catch-up batch if that resolved both boundaries;
             * otherwise yield before declaring a real under/overrun.
             */
            bcm2835_i2s_update_outputs(s, true);
            if (((s->cs & PCM_CS_TXON) && fifo32_is_empty(&s->tx_fifo) &&
                 !(s->cs & PCM_CS_TXERR)) ||
                ((s->cs & PCM_CS_RXON) && fifo32_is_full(&s->rx_fifo) &&
                 !(s->cs & PCM_CS_RXERR))) {
                break;
            }
        }
    } while (budget && s->next_frame_ns <= now);

    bcm2835_i2s_update_outputs(s, true);
    bcm2835_i2s_schedule_frame(s);
}

static void bcm2835_i2s_fifo_clear(void *opaque)
{
    BCM2835I2SState *s = opaque;

    if (s->fifo_clear_pending & PCM_CS_TXCLR) {
        fifo32_reset(&s->tx_fifo);
    }
    if (s->fifo_clear_pending & PCM_CS_RXCLR) {
        fifo32_reset(&s->rx_fifo);
    }
    s->fifo_clear_pending = 0;
    bcm2835_i2s_update_outputs(s, true);
}

static void bcm2835_i2s_sync_complete(void *opaque)
{
    BCM2835I2SState *s = opaque;

    if (s->sync_target) {
        s->cs |= PCM_CS_SYNC;
    } else {
        s->cs &= ~PCM_CS_SYNC;
    }
    s->sync_pending = false;
}

static void bcm2835_i2s_clock_update(void *opaque, ClockEvent event)
{
    BCM2835I2SState *s = opaque;
    uint64_t old_rate = s->pcm_clk_hz;

    s->pcm_clk_hz = clock_get_hz(s->pcm_clk);

    if (!s->pcm_clk_hz) {
        timer_del(&s->frame_timer);
        timer_del(&s->fifo_clear_timer);
        timer_del(&s->sync_timer);
        s->next_frame_ns = 0;
        bcm2835_i2s_update_audio(s);
        return;
    }
    bcm2835_i2s_reschedule(s, old_rate != s->pcm_clk_hz);
}

static uint32_t bcm2835_i2s_fifo_read(BCM2835I2SState *s)
{
    uint32_t value;

    if (fifo32_is_empty(&s->rx_fifo)) {
        bcm2835_i2s_set_error(s, false);
        value = 0;
    } else {
        value = fifo32_pop(&s->rx_fifo);
    }
    bcm2835_i2s_update_outputs(s, true);
    return value;
}

static void bcm2835_i2s_fifo_write(BCM2835I2SState *s, uint32_t value)
{
    if (fifo32_is_full(&s->tx_fifo)) {
        bcm2835_i2s_set_error(s, true);
    } else {
        fifo32_push(&s->tx_fifo, value);
    }
    bcm2835_i2s_update_outputs(s, true);
}

static uint64_t bcm2835_i2s_read(void *opaque, hwaddr offset,
                                 unsigned int size)
{
    BCM2835I2SState *s = opaque;

    switch (offset) {
    case PCM_CS_A:
        return bcm2835_i2s_cs_read(s);
    case PCM_FIFO_A:
        return bcm2835_i2s_fifo_read(s);
    case PCM_MODE_A:
        return s->mode;
    case PCM_RXC_A:
        return s->rxc;
    case PCM_TXC_A:
        return s->txc;
    case PCM_DREQ_A:
        return s->dreq_reg;
    case PCM_INTEN_A:
        return s->inten;
    case PCM_INTSTC_A:
        return s->int_status;
    case PCM_GRAY:
        return s->gray |
               (bcm2835_i2s_rx_level(s) << PCM_GRAY_RXFIFOLEVEL_SHIFT);
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2835_I2S, offset);
        return 0;
    }
}

static void bcm2835_i2s_cs_write(BCM2835I2SState *s, uint32_t value)
{
    uint32_t old_cs = s->cs;
    bool old_running = bcm2835_i2s_running(s);
    bool sync_target = value & PCM_CS_SYNC;

    if (value & PCM_CS_RXERR) {
        s->cs &= ~PCM_CS_RXERR;
    }
    if (value & PCM_CS_TXERR) {
        s->cs &= ~PCM_CS_TXERR;
    }
    s->cs = (s->cs & (PCM_CS_RXERR | PCM_CS_TXERR | PCM_CS_SYNC)) |
            (value & PCM_CS_RW_MASK);

    if (sync_target != !!(s->cs & PCM_CS_SYNC)) {
        s->sync_target = sync_target;
        s->sync_pending = true;
        bcm2835_i2s_schedule_control(s, &s->sync_timer);
    } else if (s->sync_pending && sync_target != s->sync_target) {
        s->sync_target = sync_target;
        s->sync_pending = false;
        timer_del(&s->sync_timer);
    }
    if (value & (PCM_CS_RXCLR | PCM_CS_TXCLR)) {
        s->fifo_clear_pending |= value & (PCM_CS_RXCLR | PCM_CS_TXCLR);
        bcm2835_i2s_schedule_control(s, &s->fifo_clear_timer);
    }

    bcm2835_i2s_update_outputs(s, old_cs != s->cs);
    bcm2835_i2s_reschedule(s,
        !old_running && bcm2835_i2s_running(s));
}

static void bcm2835_i2s_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned int size)
{
    BCM2835I2SState *s = opaque;

    switch (offset) {
    case PCM_CS_A:
        bcm2835_i2s_cs_write(s, value);
        break;
    case PCM_FIFO_A:
        bcm2835_i2s_fifo_write(s, value);
        break;
    case PCM_MODE_A:
        s->mode = value & PCM_MODE_RW_MASK;
        bcm2835_i2s_reschedule(s, true);
        break;
    case PCM_RXC_A:
        s->rxc = value & PCM_CHANNEL_RW_MASK;
        bcm2835_i2s_update_outputs(s, true);
        break;
    case PCM_TXC_A:
        s->txc = value & PCM_CHANNEL_RW_MASK;
        bcm2835_i2s_update_outputs(s, true);
        break;
    case PCM_DREQ_A:
        s->dreq_reg = value & PCM_DREQ_RW_MASK;
        bcm2835_i2s_update_outputs(s, false);
        break;
    case PCM_INTEN_A:
        s->inten = value & PCM_INT_MASK;
        bcm2835_i2s_update_outputs(s, true);
        break;
    case PCM_INTSTC_A:
        s->int_status &= ~(value & PCM_INT_MASK);
        bcm2835_i2s_update_outputs(s, false);
        break;
    case PCM_GRAY:
        s->gray = value & (PCM_GRAY_CLR | PCM_GRAY_EN);
        if (value & PCM_GRAY_FLUSH) {
            /* There is no external gray-code input buffer in this model. */
        }
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2835_I2S, offset);
        break;
    }
}

static const MemoryRegionOps bcm2835_i2s_ops = {
    .read = bcm2835_i2s_read,
    .write = bcm2835_i2s_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static int bcm2835_i2s_post_load(void *opaque, int version_id)
{
    BCM2835I2SState *s = opaque;

    if (fifo32_num_used(&s->tx_fifo) > BCM2835_I2S_FIFO_WORDS ||
        fifo32_num_used(&s->rx_fifo) > BCM2835_I2S_FIFO_WORDS ||
        s->fifo_clear_pending & ~(PCM_CS_RXCLR | PCM_CS_TXCLR)) {
        return -EINVAL;
    }
    s->audio_read = 0;
    s->audio_used = 0;
    s->trace_frame_count = 0;
    s->audio_active = false;
    bcm2835_i2s_update_outputs(s, false);
    bcm2835_i2s_reschedule(s, false);
    return 0;
}

static const VMStateDescription vmstate_bcm2835_i2s = {
    .name = TYPE_BCM2835_I2S,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2835_i2s_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(cs, BCM2835I2SState),
        VMSTATE_UINT32(mode, BCM2835I2SState),
        VMSTATE_UINT32(rxc, BCM2835I2SState),
        VMSTATE_UINT32(txc, BCM2835I2SState),
        VMSTATE_UINT32(dreq_reg, BCM2835I2SState),
        VMSTATE_UINT32(inten, BCM2835I2SState),
        VMSTATE_UINT32(int_status, BCM2835I2SState),
        VMSTATE_UINT32(gray, BCM2835I2SState),
        VMSTATE_UINT64(pcm_clk_hz, BCM2835I2SState),
        VMSTATE_UINT64(next_frame_ns, BCM2835I2SState),
        VMSTATE_UINT64(frame_remainder, BCM2835I2SState),
        VMSTATE_UINT8(fifo_clear_pending, BCM2835I2SState),
        VMSTATE_BOOL(sync_pending, BCM2835I2SState),
        VMSTATE_BOOL(sync_target, BCM2835I2SState),
        VMSTATE_FIFO32(tx_fifo, BCM2835I2SState),
        VMSTATE_FIFO32(rx_fifo, BCM2835I2SState),
        VMSTATE_TIMER(frame_timer, BCM2835I2SState),
        VMSTATE_TIMER(fifo_clear_timer, BCM2835I2SState),
        VMSTATE_TIMER(sync_timer, BCM2835I2SState),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2835_i2s_reset_hold(Object *obj, ResetType type)
{
    BCM2835I2SState *s = BCM2835_I2S(obj);

    timer_del(&s->frame_timer);
    timer_del(&s->fifo_clear_timer);
    timer_del(&s->sync_timer);
    fifo32_reset(&s->tx_fifo);
    fifo32_reset(&s->rx_fifo);
    s->cs = 0;
    s->mode = 0;
    s->rxc = 0;
    s->txc = 0;
    s->dreq_reg = PCM_DREQ_RESET;
    s->inten = 0;
    s->int_status = 0;
    s->gray = 0;
    s->pcm_clk_hz = clock_get_hz(s->pcm_clk);
    s->next_frame_ns = 0;
    s->frame_remainder = 0;
    s->fifo_clear_pending = 0;
    s->sync_pending = false;
    s->sync_target = false;
    s->audio_read = 0;
    s->audio_used = 0;
    if (s->voice && s->audio_active) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
    }
    s->audio_active = false;
    bcm2835_i2s_update_outputs(s, false);
}

static void bcm2835_i2s_realize(DeviceState *dev, Error **errp)
{
    BCM2835I2SState *s = BCM2835_I2S(dev);

    if (!audio_be_check(&s->audio_be, errp)) {
        return;
    }
}

static void bcm2835_i2s_unrealize(DeviceState *dev)
{
    BCM2835I2SState *s = BCM2835_I2S(dev);

    if (s->voice) {
        audio_be_close_out(s->audio_be, s->voice);
        s->voice = NULL;
    }
}

static void bcm2835_i2s_init(Object *obj)
{
    BCM2835I2SState *s = BCM2835_I2S(obj);

    fifo32_create(&s->tx_fifo, BCM2835_I2S_FIFO_WORDS);
    fifo32_create(&s->rx_fifo, BCM2835_I2S_FIFO_WORDS);
    timer_init_ns(&s->frame_timer, QEMU_CLOCK_VIRTUAL,
                  bcm2835_i2s_frame_tick, s);
    timer_init_ns(&s->fifo_clear_timer, QEMU_CLOCK_VIRTUAL,
                  bcm2835_i2s_fifo_clear, s);
    timer_init_ns(&s->sync_timer, QEMU_CLOCK_VIRTUAL,
                  bcm2835_i2s_sync_complete, s);
    s->pcm_clk = qdev_init_clock_in(DEVICE(obj), "pcm-in",
                                    bcm2835_i2s_clock_update, s,
                                    ClockUpdate);
    memory_region_init_io(&s->iomem, obj, &bcm2835_i2s_ops, s,
                          TYPE_BCM2835_I2S, BCM2835_I2S_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    qdev_init_gpio_out_named(DEVICE(obj), s->dreq, "dreq", 2);
}

static void bcm2835_i2s_finalize(Object *obj)
{
    BCM2835I2SState *s = BCM2835_I2S(obj);

    timer_deinit(&s->frame_timer);
    timer_deinit(&s->fifo_clear_timer);
    timer_deinit(&s->sync_timer);
    fifo32_destroy(&s->tx_fifo);
    fifo32_destroy(&s->rx_fifo);
}

static const Property bcm2835_i2s_properties[] = {
    DEFINE_AUDIO_PROPERTIES(BCM2835I2SState, audio_be),
    DEFINE_PROP_UINT64("slave-clock-frequency", BCM2835I2SState,
                       slave_clock_hz, 0),
};

static void bcm2835_i2s_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = bcm2835_i2s_realize;
    dc->unrealize = bcm2835_i2s_unrealize;
    dc->vmsd = &vmstate_bcm2835_i2s;
    device_class_set_props(dc, bcm2835_i2s_properties);
    set_bit(DEVICE_CATEGORY_SOUND, dc->categories);
    rc->phases.hold = bcm2835_i2s_reset_hold;
}

static const TypeInfo bcm2835_i2s_info = {
    .name = TYPE_BCM2835_I2S,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835I2SState),
    .instance_init = bcm2835_i2s_init,
    .instance_finalize = bcm2835_i2s_finalize,
    .class_init = bcm2835_i2s_class_init,
};

static void bcm2835_i2s_register_types(void)
{
    type_register_static(&bcm2835_i2s_info);
}

type_init(bcm2835_i2s_register_types)
