/*
 * BCM2835 / BCM2711 PWM controller
 *
 * Copyright (c) 2026 qemu-pi4 contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/audio/bcm2835_pwm.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "trace.h"

#define BCM2835_PWM_MMIO_SIZE       0x100

#define PWM_CTL                     0x00
#define PWM_STA                     0x04
#define PWM_DMAC                    0x08
#define PWM_RNG1                    0x10
#define PWM_DAT1                    0x14
#define PWM_FIF1                    0x18
#define PWM_RNG2                    0x20
#define PWM_DAT2                    0x24

#define PWM_CTL_PWEN(_n)            BIT((_n) * 8)
#define PWM_CTL_MODE(_n)            BIT((_n) * 8 + 1)
#define PWM_CTL_RPTL(_n)            BIT((_n) * 8 + 2)
#define PWM_CTL_SBIT(_n)            BIT((_n) * 8 + 3)
#define PWM_CTL_POLA(_n)            BIT((_n) * 8 + 4)
#define PWM_CTL_USEF(_n)            BIT((_n) * 8 + 5)
#define PWM_CTL_CLRF                BIT(6)
#define PWM_CTL_MSEN(_n)            BIT((_n) * 8 + 7)
#define PWM_CTL_CHANNEL_MASK(_n)    (PWM_CTL_PWEN(_n) | PWM_CTL_MODE(_n) | \
                                     PWM_CTL_RPTL(_n) | PWM_CTL_SBIT(_n) | \
                                     PWM_CTL_POLA(_n) | PWM_CTL_USEF(_n) | \
                                     PWM_CTL_MSEN(_n))
#define PWM_CTL_RW_MASK             (PWM_CTL_CHANNEL_MASK(0) | \
                                     PWM_CTL_CHANNEL_MASK(1))

#define PWM_STA_FULL                BIT(0)
#define PWM_STA_EMPT                BIT(1)
#define PWM_STA_WERR                BIT(2)
#define PWM_STA_RERR                BIT(3)
#define PWM_STA_GAP(_n)             BIT(4 + (_n))
#define PWM_STA_BERR                BIT(8)
#define PWM_STA_ACTIVE(_n)          BIT(9 + (_n))
#define PWM_STA_W1C_MASK            (PWM_STA_WERR | PWM_STA_RERR | \
                                     PWM_STA_GAP(0) | PWM_STA_GAP(1) | \
                                     PWM_STA_BERR)

#define PWM_DMAC_DREQ_MASK          0xffU
#define PWM_DMAC_PANIC_SHIFT        8
#define PWM_DMAC_PANIC_MASK         (0xffU << PWM_DMAC_PANIC_SHIFT)
#define PWM_DMAC_ENAB               BIT(31)
#define PWM_DMAC_RW_MASK            (PWM_DMAC_ENAB | PWM_DMAC_PANIC_MASK | \
                                     PWM_DMAC_DREQ_MASK)
#define PWM_DMAC_RESET              0x00000707U

#define BCM2835_PWM_PERIOD_BUDGET   256
#define BCM2835_PWM_CATCHUP_NS      2000

static unsigned int bcm2835_pwm_fifo_level(BCM2835PWMState *s)
{
    return fifo32_num_used(&s->fifo);
}

static bool bcm2835_pwm_fifo_full(BCM2835PWMState *s)
{
    return bcm2835_pwm_fifo_level(s) >= s->fifo_depth;
}

static bool bcm2835_pwm_channel_enabled(BCM2835PWMState *s,
                                         unsigned int channel)
{
    return (s->ctl & PWM_CTL_PWEN(channel)) && s->range[channel] &&
           s->pwm_clk_hz;
}

static bool bcm2835_pwm_fifo_channel_running(BCM2835PWMState *s,
                                              unsigned int channel)
{
    return bcm2835_pwm_channel_enabled(s, channel) &&
           (s->ctl & PWM_CTL_USEF(channel));
}

static bool bcm2835_pwm_shared_fifo_running(BCM2835PWMState *s)
{
    return bcm2835_pwm_fifo_channel_running(s, 0) &&
           bcm2835_pwm_fifo_channel_running(s, 1);
}

static bool bcm2835_pwm_channel_is_timer_owner(BCM2835PWMState *s,
                                                unsigned int channel)
{
    if (!bcm2835_pwm_fifo_channel_running(s, channel)) {
        return false;
    }
    return !bcm2835_pwm_shared_fifo_running(s) || channel == 0;
}

static uint32_t bcm2835_pwm_timer_range(BCM2835PWMState *s,
                                        unsigned int channel)
{
    if (bcm2835_pwm_shared_fifo_running(s)) {
        return MAX(s->range[0], s->range[1]);
    }
    return s->range[channel];
}

static uint32_t bcm2835_pwm_serial_high_bits(BCM2835PWMState *s,
                                              unsigned int channel,
                                              uint32_t value)
{
    uint32_t range = s->range[channel];
    unsigned int data_bits = MIN(range, 32U);
    uint64_t high = 0;

    if (data_bits) {
        uint32_t selected = data_bits == 32 ? value :
                            value >> (32 - data_bits);

        high = ctpop32(selected);
    }
    if (range > 32 && (s->ctl & PWM_CTL_SBIT(channel))) {
        high += (uint64_t)range - 32;
    }
    return high;
}

static uint32_t bcm2835_pwm_calculate_duty(BCM2835PWMState *s,
                                            unsigned int channel,
                                            bool transmitting,
                                            uint32_t value)
{
    uint64_t high;
    uint32_t range = s->range[channel];

    if (!transmitting || !range) {
        high = !!(s->ctl & PWM_CTL_SBIT(channel));
        range = 1;
    } else if (s->ctl & PWM_CTL_MODE(channel)) {
        high = bcm2835_pwm_serial_high_bits(s, channel, value);
    } else {
        high = MIN(value, range);
    }

    if (s->ctl & PWM_CTL_POLA(channel)) {
        high = range - high;
    }
    return (uint64_t)BCM2835_PWM_MAX_DUTY * high / range;
}

static void bcm2835_pwm_update_channel_output(BCM2835PWMState *s,
                                               unsigned int channel)
{
    BCM2835PWMChannel *c = &s->channel[channel];
    bool transmitting;
    uint32_t value;
    uint32_t freq;
    uint32_t duty;

    if (!bcm2835_pwm_channel_enabled(s, channel)) {
        transmitting = false;
        value = 0;
    } else if (s->ctl & PWM_CTL_USEF(channel)) {
        transmitting = c->transmitting;
        value = c->current_data;
    } else {
        transmitting = true;
        value = s->data[channel];
    }

    c->transmitting = transmitting;
    freq = transmitting ? s->pwm_clk_hz / s->range[channel] : 0;
    duty = bcm2835_pwm_calculate_duty(s, channel, transmitting, value);

    if (c->freq != freq || c->duty != duty) {
        trace_bcm2835_pwm_output(s->instance_id, channel, c->freq, freq,
                                c->duty, duty);
    }
    c->freq = freq;
    if (c->duty != duty) {
        c->duty = duty;
        qemu_set_irq(s->duty_gpio_out[channel], duty);
    }
}

static bool bcm2835_pwm_audio_configured(BCM2835PWMState *s)
{
    uint32_t required = PWM_CTL_PWEN(0) | PWM_CTL_USEF(0) |
                        PWM_CTL_PWEN(1) | PWM_CTL_USEF(1);

    return s->audio_output && s->pwm_clk_hz && s->range[0] &&
           s->range[0] == s->range[1] &&
           !(s->ctl & (PWM_CTL_MODE(0) | PWM_CTL_MODE(1))) &&
           (s->ctl & required) == required;
}

static void bcm2835_pwm_audio_flush(BCM2835PWMState *s, int available)
{
    while (s->voice && s->audio_active && s->audio_used && available > 0) {
        unsigned int frames = MIN(s->audio_used,
                                  BCM2835_PWM_AUDIO_FRAMES - s->audio_read);
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
                        BCM2835_PWM_AUDIO_FRAMES;
        s->audio_used -= written / (2 * sizeof(int32_t));
        available -= written;
    }
}

static void bcm2835_pwm_audio_callback(void *opaque, int available)
{
    bcm2835_pwm_audio_flush(opaque, available);
}

static void bcm2835_pwm_update_audio(BCM2835PWMState *s)
{
    uint64_t rate64 = bcm2835_pwm_audio_configured(s) ?
                      (s->pwm_clk_hz + s->range[0] / 2) / s->range[0] : 0;
    bool active;

    if (rate64 >= 1000 && rate64 <= 768000 && rate64 != s->audio_rate) {
        struct audsettings settings = {
            .freq = rate64,
            .nchannels = 2,
            .fmt = AUDIO_FORMAT_S32,
            .big_endian = HOST_BIG_ENDIAN,
        };

        trace_bcm2835_pwm_audio_rate(s->instance_id, s->pwm_clk_hz,
                                     s->range[0], rate64);
        s->voice = audio_be_open_out(s->audio_be, s->voice,
                                     TYPE_BCM2835_PWM ".out", s,
                                     bcm2835_pwm_audio_callback, &settings);
        if (s->voice) {
            s->audio_rate = rate64;
        } else {
            s->audio_rate = 0;
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: could not open audio output at %" PRIu64
                          " Hz\n", TYPE_BCM2835_PWM, rate64);
        }
        s->audio_read = 0;
        s->audio_used = 0;
    }

    active = s->voice && bcm2835_pwm_audio_configured(s) &&
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

static void bcm2835_pwm_set_dreq(BCM2835PWMState *s, bool level)
{
    if (!level) {
        qemu_bh_cancel(s->dreq_bh);
    }
    if (level == s->dreq_level) {
        return;
    }

    /*
     * A rising edge can immediately make BCM2835 DMA write this device's
     * FIFO.  Defer that edge until the current PWM MMIO write has returned,
     * avoiding a nested access to the same MemoryRegion.  Timer and migration
     * updates are already outside PWM MMIO and may notify DMA synchronously.
     */
    if (level && s->in_mmio_write) {
        qemu_bh_schedule(s->dreq_bh);
        return;
    }

    s->dreq_level = level;
    qemu_set_irq(s->dreq, level);
}

static void bcm2835_pwm_update_outputs(BCM2835PWMState *s)
{
    unsigned int level = bcm2835_pwm_fifo_level(s);
    unsigned int dreq_threshold = s->dmac & PWM_DMAC_DREQ_MASK;
    unsigned int panic_threshold = (s->dmac & PWM_DMAC_PANIC_MASK) >>
                                   PWM_DMAC_PANIC_SHIFT;
    bool dma_enabled = s->dmac & PWM_DMAC_ENAB;
    unsigned int channel;

    s->fifo_level = level;
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        bcm2835_pwm_update_channel_output(s, channel);
    }
    bcm2835_pwm_update_audio(s);
    bcm2835_pwm_set_dreq(s, dma_enabled && level <= dreq_threshold);
    qemu_set_irq(s->panic, dma_enabled && level <= panic_threshold);
}

static void bcm2835_pwm_dreq_bh(void *opaque)
{
    bcm2835_pwm_update_outputs(opaque);
}

static int32_t bcm2835_pwm_audio_sample(BCM2835PWMState *s,
                                        unsigned int channel)
{
    uint32_t duty = s->channel[channel].duty;
    int64_t centered = (int64_t)duty * 2 - BCM2835_PWM_MAX_DUTY;

    return centered * INT32_MAX / BCM2835_PWM_MAX_DUTY;
}

static void bcm2835_pwm_audio_push(BCM2835PWMState *s)
{
    unsigned int write;

    if (!s->voice || !s->audio_active) {
        return;
    }
    if (s->audio_used == BCM2835_PWM_AUDIO_FRAMES) {
        s->audio_read = (s->audio_read + 1) % BCM2835_PWM_AUDIO_FRAMES;
        s->audio_used--;
    }
    write = (s->audio_read + s->audio_used) % BCM2835_PWM_AUDIO_FRAMES;
    s->audio_buffer[write * 2] = bcm2835_pwm_audio_sample(s, 0);
    s->audio_buffer[write * 2 + 1] = bcm2835_pwm_audio_sample(s, 1);
    s->audio_used++;
}

static bool bcm2835_pwm_claim_channel(BCM2835PWMState *s,
                                      unsigned int channel)
{
    BCM2835PWMChannel *c = &s->channel[channel];

    if (fifo32_is_empty(&s->fifo)) {
        return false;
    }

    c->current_data = fifo32_pop(&s->fifo);
    c->last_data = c->current_data;
    c->have_current = true;
    c->have_last = true;
    c->transmitting = true;
    return true;
}

static void bcm2835_pwm_consume_channel(BCM2835PWMState *s,
                                        unsigned int channel,
                                        bool shared)
{
    BCM2835PWMChannel *c = &s->channel[channel];

    if (bcm2835_pwm_claim_channel(s, channel)) {
        return;
    }
    if (!shared && (s->ctl & PWM_CTL_RPTL(channel)) &&
               c->have_last) {
        c->current_data = c->last_data;
        c->have_current = true;
        c->transmitting = true;
    } else {
        s->status |= PWM_STA_GAP(channel);
        c->have_current = false;
        c->transmitting = false;
    }
}

static void bcm2835_pwm_prime_fifo(BCM2835PWMState *s)
{
    unsigned int channel;

    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        BCM2835PWMChannel *c = &s->channel[channel];

        if (bcm2835_pwm_fifo_channel_running(s, channel) &&
            !c->have_current) {
            bcm2835_pwm_claim_channel(s, channel);
        }
    }
}

static int64_t bcm2835_pwm_next_period_delay(BCM2835PWMChannel *c)
{
    BCM2835PWMState *s = c->pwm;
    uint64_t numerator = NANOSECONDS_PER_SECOND *
                         (uint64_t)bcm2835_pwm_timer_range(s, c->index);
    uint64_t delay;

    if (!s->pwm_clk_hz) {
        return 0;
    }
    delay = numerator / s->pwm_clk_hz;
    c->period_remainder += numerator % s->pwm_clk_hz;
    if (c->period_remainder >= s->pwm_clk_hz) {
        delay += c->period_remainder / s->pwm_clk_hz;
        c->period_remainder %= s->pwm_clk_hz;
    }
    return MAX(delay, 1);
}

static void bcm2835_pwm_schedule_channel(BCM2835PWMChannel *c)
{
    BCM2835PWMState *s = c->pwm;
    int64_t now;
    int64_t expiry;

    if (!bcm2835_pwm_channel_is_timer_owner(s, c->index)) {
        timer_del(&c->timer);
        c->next_period_ns = 0;
        return;
    }

    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (!c->next_period_ns) {
        c->next_period_ns = now + bcm2835_pwm_next_period_delay(c);
    }
    expiry = c->next_period_ns > now ? c->next_period_ns :
             now + BCM2835_PWM_CATCHUP_NS;
    timer_mod(&c->timer, expiry);
}

static void bcm2835_pwm_reschedule(BCM2835PWMState *s, bool restart)
{
    unsigned int channel;

    bcm2835_pwm_prime_fifo(s);
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        BCM2835PWMChannel *c = &s->channel[channel];

        if (restart) {
            timer_del(&c->timer);
            c->next_period_ns = 0;
            c->period_remainder = 0;
        }
        if (!bcm2835_pwm_channel_is_timer_owner(s, channel)) {
            timer_del(&c->timer);
            c->next_period_ns = 0;
            if (!bcm2835_pwm_fifo_channel_running(s, channel)) {
                c->transmitting = false;
            }
        } else if (!timer_pending(&c->timer)) {
            bcm2835_pwm_schedule_channel(c);
        }
    }
    bcm2835_pwm_update_outputs(s);
}

static void bcm2835_pwm_period_tick(void *opaque)
{
    BCM2835PWMChannel *c = opaque;
    BCM2835PWMState *s = c->pwm;
    bool shared = bcm2835_pwm_shared_fifo_running(s);
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    unsigned int budget = BCM2835_PWM_PERIOD_BUDGET;

    if (!bcm2835_pwm_channel_is_timer_owner(s, c->index)) {
        c->next_period_ns = 0;
        return;
    }

    do {
        unsigned int before = bcm2835_pwm_fifo_level(s);

        if (shared) {
            /* The current pair has occupied the period that just ended. */
            bcm2835_pwm_audio_push(s);
        }
        bcm2835_pwm_consume_channel(s, c->index, shared);
        if (shared) {
            bcm2835_pwm_consume_channel(s, 1, true);
        }
        bcm2835_pwm_update_outputs(s);

        s->trace_period_count++;
        if (!(s->trace_period_count & 0x3ff)) {
            trace_bcm2835_pwm_period(s->instance_id, now,
                                     s->trace_period_count, before,
                                     bcm2835_pwm_fifo_level(s));
        }

        c->next_period_ns += bcm2835_pwm_next_period_delay(c);
        budget--;
    } while (budget && c->next_period_ns <= now);

    bcm2835_pwm_schedule_channel(c);
}

static void bcm2835_pwm_clock_update(void *opaque, ClockEvent event)
{
    BCM2835PWMState *s = opaque;
    uint64_t old_rate = s->pwm_clk_hz;

    s->pwm_clk_hz = clock_get_hz(s->pwm_clk);
    bcm2835_pwm_reschedule(s, old_rate != s->pwm_clk_hz);
}

static uint32_t bcm2835_pwm_status_read(BCM2835PWMState *s)
{
    uint32_t value = s->status;
    unsigned int channel;

    if (bcm2835_pwm_fifo_full(s)) {
        value |= PWM_STA_FULL;
    }
    if (fifo32_is_empty(&s->fifo)) {
        value |= PWM_STA_EMPT;
    }
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        if (s->channel[channel].transmitting) {
            value |= PWM_STA_ACTIVE(channel);
        }
    }
    return value;
}

static uint64_t bcm2835_pwm_read(void *opaque, hwaddr offset,
                                 unsigned int size)
{
    BCM2835PWMState *s = opaque;

    switch (offset) {
    case PWM_CTL:
        return s->ctl;
    case PWM_STA:
        return bcm2835_pwm_status_read(s);
    case PWM_DMAC:
        return s->dmac;
    case PWM_RNG1:
        return s->range[0];
    case PWM_DAT1:
        return s->data[0];
    case PWM_FIF1:
        return 0x70776d30U + s->instance_id;
    case PWM_RNG2:
        return s->range[1];
    case PWM_DAT2:
        return s->data[1];
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: read from invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2835_PWM, offset);
        return 0;
    }
}

static void bcm2835_pwm_fifo_write(BCM2835PWMState *s, uint32_t value)
{
    if (bcm2835_pwm_fifo_full(s)) {
        s->status |= PWM_STA_WERR;
    } else {
        fifo32_push(&s->fifo, value);
    }
    bcm2835_pwm_reschedule(s, false);
}

static void bcm2835_pwm_ctl_write(BCM2835PWMState *s, uint32_t value)
{
    uint32_t old_ctl = s->ctl;

    s->ctl = value & PWM_CTL_RW_MASK;
    if (value & PWM_CTL_CLRF) {
        fifo32_reset(&s->fifo);
        s->channel[0].have_current = false;
        s->channel[0].have_last = false;
        s->channel[0].transmitting = false;
        s->channel[1].have_current = false;
        s->channel[1].have_last = false;
        s->channel[1].transmitting = false;
    }
    bcm2835_pwm_reschedule(s, old_ctl != s->ctl);
}

static void bcm2835_pwm_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned int size)
{
    BCM2835PWMState *s = opaque;

    assert(!s->in_mmio_write);
    s->in_mmio_write = true;
    switch (offset) {
    case PWM_CTL:
        bcm2835_pwm_ctl_write(s, value);
        break;
    case PWM_STA:
        s->status &= ~(value & PWM_STA_W1C_MASK);
        break;
    case PWM_DMAC:
        s->dmac = value & PWM_DMAC_RW_MASK;
        bcm2835_pwm_update_outputs(s);
        break;
    case PWM_RNG1:
        s->range[0] = value;
        bcm2835_pwm_reschedule(s, true);
        break;
    case PWM_DAT1:
        s->data[0] = value;
        bcm2835_pwm_update_outputs(s);
        break;
    case PWM_FIF1:
        bcm2835_pwm_fifo_write(s, value);
        break;
    case PWM_RNG2:
        s->range[1] = value;
        bcm2835_pwm_reschedule(s, true);
        break;
    case PWM_DAT2:
        s->data[1] = value;
        bcm2835_pwm_update_outputs(s);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write to invalid offset 0x%" HWADDR_PRIx "\n",
                      TYPE_BCM2835_PWM, offset);
        break;
    }
    s->in_mmio_write = false;
}

static const MemoryRegionOps bcm2835_pwm_ops = {
    .read = bcm2835_pwm_read,
    .write = bcm2835_pwm_write,
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

static int bcm2835_pwm_post_load(void *opaque, int version_id)
{
    BCM2835PWMState *s = opaque;
    unsigned int channel;

    if (s->ctl & ~PWM_CTL_RW_MASK || s->status & ~PWM_STA_W1C_MASK ||
        s->dmac & ~PWM_DMAC_RW_MASK ||
        bcm2835_pwm_fifo_level(s) > s->fifo_depth) {
        return -EINVAL;
    }
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        if (s->channel[channel].index != channel) {
            return -EINVAL;
        }
        s->channel[channel].pwm = s;
    }
    s->audio_read = 0;
    s->audio_used = 0;
    s->audio_active = false;
    s->trace_period_count = 0;
    qemu_bh_cancel(s->dreq_bh);
    s->in_mmio_write = false;
    s->dreq_level = false;
    qemu_set_irq(s->dreq, 0);
    bcm2835_pwm_update_outputs(s);
    bcm2835_pwm_reschedule(s, false);
    return 0;
}

static const VMStateDescription vmstate_bcm2835_pwm_channel = {
    .name = TYPE_BCM2835_PWM "/channel",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(next_period_ns, BCM2835PWMChannel),
        VMSTATE_UINT64(period_remainder, BCM2835PWMChannel),
        VMSTATE_UINT32(current_data, BCM2835PWMChannel),
        VMSTATE_UINT32(last_data, BCM2835PWMChannel),
        VMSTATE_UINT8(index, BCM2835PWMChannel),
        VMSTATE_BOOL(have_current, BCM2835PWMChannel),
        VMSTATE_BOOL(have_last, BCM2835PWMChannel),
        VMSTATE_BOOL(transmitting, BCM2835PWMChannel),
        VMSTATE_TIMER(timer, BCM2835PWMChannel),
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription vmstate_bcm2835_pwm = {
    .name = TYPE_BCM2835_PWM,
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = bcm2835_pwm_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(ctl, BCM2835PWMState),
        VMSTATE_UINT32(status, BCM2835PWMState),
        VMSTATE_UINT32(dmac, BCM2835PWMState),
        VMSTATE_UINT32_ARRAY(range, BCM2835PWMState, BCM2835_PWM_CHANNELS),
        VMSTATE_UINT32_ARRAY(data, BCM2835PWMState, BCM2835_PWM_CHANNELS),
        VMSTATE_UINT64(pwm_clk_hz, BCM2835PWMState),
        VMSTATE_FIFO32(fifo, BCM2835PWMState),
        VMSTATE_STRUCT_ARRAY(channel, BCM2835PWMState,
                             BCM2835_PWM_CHANNELS, 1,
                             vmstate_bcm2835_pwm_channel,
                             BCM2835PWMChannel),
        VMSTATE_END_OF_LIST()
    },
};

static void bcm2835_pwm_reset_hold(Object *obj, ResetType type)
{
    BCM2835PWMState *s = BCM2835_PWM(obj);
    unsigned int channel;

    fifo32_reset(&s->fifo);
    s->ctl = 0;
    s->status = 0;
    s->dmac = PWM_DMAC_RESET;
    s->range[0] = 32;
    s->range[1] = 32;
    s->data[0] = 0;
    s->data[1] = 0;
    s->fifo_level = 0;
    s->pwm_clk_hz = clock_get_hz(s->pwm_clk);
    qemu_bh_cancel(s->dreq_bh);
    s->in_mmio_write = false;
    s->dreq_level = false;
    qemu_set_irq(s->dreq, 0);
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        BCM2835PWMChannel *c = &s->channel[channel];

        timer_del(&c->timer);
        c->next_period_ns = 0;
        c->period_remainder = 0;
        c->current_data = 0;
        c->last_data = 0;
        c->freq = 0;
        c->duty = 0;
        c->have_current = false;
        c->have_last = false;
        c->transmitting = false;
        qemu_set_irq(s->duty_gpio_out[channel], 0);
    }
    s->audio_read = 0;
    s->audio_used = 0;
    if (s->voice && s->audio_active) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
    }
    s->audio_active = false;
    s->trace_period_count = 0;
    bcm2835_pwm_update_outputs(s);
}

static void bcm2835_pwm_realize(DeviceState *dev, Error **errp)
{
    BCM2835PWMState *s = BCM2835_PWM(dev);

    if (s->fifo_depth < 1 ||
        s->fifo_depth > BCM2835_PWM_MAX_FIFO_WORDS) {
        error_setg(errp, "fifo-depth must be between 1 and %u",
                   BCM2835_PWM_MAX_FIFO_WORDS);
        return;
    }
    if (s->instance_id > 1) {
        error_setg(errp, "instance-id must be 0 or 1");
        return;
    }
    if (s->audio_output && !audio_be_check(&s->audio_be, errp)) {
        return;
    }
}

static void bcm2835_pwm_unrealize(DeviceState *dev)
{
    BCM2835PWMState *s = BCM2835_PWM(dev);

    if (s->voice) {
        audio_be_close_out(s->audio_be, s->voice);
        s->voice = NULL;
    }
}

static void bcm2835_pwm_init(Object *obj)
{
    BCM2835PWMState *s = BCM2835_PWM(obj);
    unsigned int channel;

    fifo32_create(&s->fifo, BCM2835_PWM_MAX_FIFO_WORDS);
    s->dreq_bh = qemu_bh_new(bcm2835_pwm_dreq_bh, s);
    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        BCM2835PWMChannel *c = &s->channel[channel];

        c->pwm = s;
        c->index = channel;
        timer_init_ns(&c->timer, QEMU_CLOCK_VIRTUAL,
                      bcm2835_pwm_period_tick, c);
        object_property_add_uint32_ptr(obj, "freq[*]", &c->freq,
                                       OBJ_PROP_FLAG_READ);
        object_property_add_uint32_ptr(obj, "duty[*]", &c->duty,
                                       OBJ_PROP_FLAG_READ);
    }
    object_property_add_uint32_ptr(obj, "fifo-level", &s->fifo_level,
                                   OBJ_PROP_FLAG_READ);
    s->pwm_clk = qdev_init_clock_in(DEVICE(obj), "pwm-in",
                                    bcm2835_pwm_clock_update, s,
                                    ClockUpdate);
    memory_region_init_io(&s->iomem, obj, &bcm2835_pwm_ops, s,
                          TYPE_BCM2835_PWM, BCM2835_PWM_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
    qdev_init_gpio_out_named(DEVICE(obj), &s->dreq, "dreq", 1);
    qdev_init_gpio_out_named(DEVICE(obj), &s->panic, "panic", 1);
    qdev_init_gpio_out_named(DEVICE(obj), s->duty_gpio_out,
                             "duty-gpio-out", BCM2835_PWM_CHANNELS);
}

static void bcm2835_pwm_finalize(Object *obj)
{
    BCM2835PWMState *s = BCM2835_PWM(obj);
    unsigned int channel;

    for (channel = 0; channel < BCM2835_PWM_CHANNELS; channel++) {
        timer_deinit(&s->channel[channel].timer);
    }
    qemu_bh_delete(s->dreq_bh);
    fifo32_destroy(&s->fifo);
}

static const Property bcm2835_pwm_properties[] = {
    DEFINE_AUDIO_PROPERTIES(BCM2835PWMState, audio_be),
    DEFINE_PROP_UINT32("fifo-depth", BCM2835PWMState, fifo_depth, 8),
    DEFINE_PROP_UINT8("instance-id", BCM2835PWMState, instance_id, 0),
    DEFINE_PROP_BOOL("audio-output", BCM2835PWMState, audio_output, false),
};

static void bcm2835_pwm_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = bcm2835_pwm_realize;
    dc->unrealize = bcm2835_pwm_unrealize;
    dc->vmsd = &vmstate_bcm2835_pwm;
    device_class_set_props(dc, bcm2835_pwm_properties);
    set_bit(DEVICE_CATEGORY_SOUND, dc->categories);
    rc->phases.hold = bcm2835_pwm_reset_hold;
}

static const TypeInfo bcm2835_pwm_info = {
    .name = TYPE_BCM2835_PWM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2835PWMState),
    .instance_init = bcm2835_pwm_init,
    .instance_finalize = bcm2835_pwm_finalize,
    .class_init = bcm2835_pwm_class_init,
};

static void bcm2835_pwm_register_types(void)
{
    type_register_static(&bcm2835_pwm_info);
}

type_init(bcm2835_pwm_register_types)
