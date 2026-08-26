/*
 * BCM2711 HDMI transmitter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/display/bcm2711_hdmi.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define HDMI_FIFO_CTL                  0x074
#define HDMI_RAM_PACKET_CONFIG         0x0bc
#define HDMI_RAM_PACKET_STATUS         0x0c4
#define HDMI_SCHEDULER_CONTROL         0x0e0
#define HDMI_HOTPLUG                   0x1a8

#define HDMI_FIFO_CTL_RECENTER_DONE    BIT(14)
#define HDMI_FIFO_VALID_WRITE_MASK     0xefff
#define HDMI_RAM_PACKET_ENABLE         BIT(16)
#define HDMI_SCHEDULER_HDMI_ACTIVE     BIT(1)
#define HDMI_SCHEDULER_MODE_HDMI       BIT(0)
#define HDMI_HOTPLUG_CONNECTED         BIT(0)

#define HDMI_DVP_CLOCK_STOP            0x0bc

#define HDMI_MAI_CTL                   0x010
#define HDMI_MAI_THR                   0x014
#define HDMI_MAI_FMT                   0x018
#define HDMI_MAI_DATA                  0x01c

#define HDMI_MAI_CTL_DLATE             BIT(15)
#define HDMI_MAI_CTL_BUSY              BIT(14)
#define HDMI_MAI_CTL_CHALIGN           BIT(13)
#define HDMI_MAI_CTL_WHOLSMP           BIT(12)
#define HDMI_MAI_CTL_FULL              BIT(11)
#define HDMI_MAI_CTL_EMPTY             BIT(10)
#define HDMI_MAI_CTL_FLUSH             BIT(9)
#define HDMI_MAI_CTL_PAREN             BIT(8)
#define HDMI_MAI_CTL_CHNUM_SHIFT       4
#define HDMI_MAI_CTL_CHNUM_MASK        (0xfU << HDMI_MAI_CTL_CHNUM_SHIFT)
#define HDMI_MAI_CTL_ENABLE            BIT(3)
#define HDMI_MAI_CTL_ERRORE            BIT(2)
#define HDMI_MAI_CTL_ERRORF            BIT(1)
#define HDMI_MAI_CTL_RESET             BIT(0)
#define HDMI_MAI_CTL_STATUS_MASK       (HDMI_MAI_CTL_DLATE | \
                                        HDMI_MAI_CTL_ERRORE | \
                                        HDMI_MAI_CTL_ERRORF)
#define HDMI_MAI_CTL_RW_MASK           (HDMI_MAI_CTL_CHALIGN | \
                                        HDMI_MAI_CTL_WHOLSMP | \
                                        HDMI_MAI_CTL_PAREN | \
                                        HDMI_MAI_CTL_CHNUM_MASK | \
                                        HDMI_MAI_CTL_ENABLE)

#define HDMI_MAI_THR_DREQHIGH_SHIFT    8
#define HDMI_MAI_THR_DREQHIGH_MASK     (0x3fU << \
                                        HDMI_MAI_THR_DREQHIGH_SHIFT)
#define HDMI_MAI_THR_DREQLOW_MASK      0x3f

#define HDMI_MAI_FMT_AUDIO_SHIFT       16
#define HDMI_MAI_FMT_AUDIO_MASK        (0xffU << HDMI_MAI_FMT_AUDIO_SHIFT)
#define HDMI_MAI_FMT_RATE_SHIFT        8
#define HDMI_MAI_FMT_RATE_MASK         (0xffU << HDMI_MAI_FMT_RATE_SHIFT)
#define HDMI_MAI_FMT_PCM               2

#define IEC958_SUBFRAME_SAMPLE_MASK    0x0ffffff0
#define IEC958_SUBFRAME_VALIDITY       BIT(28)

#define BCM2711_HDMI_SAMPLE_BUDGET     256
#define BCM2711_HDMI_CATCHUP_NS        2000

static const uint16_t bcm2711_hdmi_bank_words[BCM2711_HDMI_BANKS] = {
    [BCM2711_HDMI_CORE] = 0x300 / 4,
    [BCM2711_HDMI_DVP] = 0x200 / 4,
    [BCM2711_HDMI_PHY] = 0x080 / 4,
    [BCM2711_HDMI_RM] = 0x080 / 4,
    [BCM2711_HDMI_PACKET] = 0x200 / 4,
    [BCM2711_HDMI_METADATA] = 0x400 / 4,
    [BCM2711_HDMI_CSC] = 0x080 / 4,
    [BCM2711_HDMI_CEC] = 0x100 / 4,
    [BCM2711_HDMI_HD] = 0x100 / 4,
};

static const char *const bcm2711_hdmi_bank_names[BCM2711_HDMI_BANKS] = {
    [BCM2711_HDMI_CORE] = TYPE_BCM2711_HDMI ".core",
    [BCM2711_HDMI_DVP] = TYPE_BCM2711_HDMI ".dvp",
    [BCM2711_HDMI_PHY] = TYPE_BCM2711_HDMI ".phy",
    [BCM2711_HDMI_RM] = TYPE_BCM2711_HDMI ".rm",
    [BCM2711_HDMI_PACKET] = TYPE_BCM2711_HDMI ".packet",
    [BCM2711_HDMI_METADATA] = TYPE_BCM2711_HDMI ".metadata",
    [BCM2711_HDMI_CSC] = TYPE_BCM2711_HDMI ".csc",
    [BCM2711_HDMI_CEC] = TYPE_BCM2711_HDMI ".cec",
    [BCM2711_HDMI_HD] = TYPE_BCM2711_HDMI ".hd",
};

static const uint32_t bcm2711_hdmi_mai_rates[] = {
    [1] = 8000,
    [2] = 11025,
    [3] = 12000,
    [4] = 16000,
    [5] = 22050,
    [6] = 24000,
    [7] = 32000,
    [8] = 44100,
    [9] = 48000,
    [10] = 64000,
    [11] = 88200,
    [12] = 96000,
    [13] = 128000,
    [14] = 176400,
    [15] = 192000,
};

static uint32_t *bcm2711_hdmi_reg(BCM2711HDMIRegBank *bank, hwaddr offset)
{
    return &bank->owner->regs[bank->first + (offset >> 2)];
}

static uint32_t *bcm2711_hdmi_bank_reg(BCM2711HDMIState *s,
                                       enum BCM2711HDMIBankID id,
                                       hwaddr offset)
{
    return &s->regs[s->banks[id].first + (offset >> 2)];
}

static uint32_t bcm2711_hdmi_mai_rate(BCM2711HDMIState *s)
{
    uint32_t format = *bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                             HDMI_MAI_FMT);
    unsigned int selector = (format & HDMI_MAI_FMT_RATE_MASK) >>
                            HDMI_MAI_FMT_RATE_SHIFT;

    return selector < ARRAY_SIZE(bcm2711_hdmi_mai_rates) ?
           bcm2711_hdmi_mai_rates[selector] : 0;
}

static unsigned int bcm2711_hdmi_mai_channels(BCM2711HDMIState *s)
{
    uint32_t control = *bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                              HDMI_MAI_CTL);

    return (control & HDMI_MAI_CTL_CHNUM_MASK) >>
           HDMI_MAI_CTL_CHNUM_SHIFT;
}

static bool bcm2711_hdmi_mai_running(BCM2711HDMIState *s)
{
    uint32_t control = *bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                              HDMI_MAI_CTL);
    unsigned int channels = bcm2711_hdmi_mai_channels(s);

    return s->clock_enabled && (control & HDMI_MAI_CTL_ENABLE) &&
           channels >= 1 && channels <= 8 && bcm2711_hdmi_mai_rate(s);
}

static bool bcm2711_hdmi_mai_pcm(BCM2711HDMIState *s)
{
    uint32_t format = *bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                             HDMI_MAI_FMT);

    return ((format & HDMI_MAI_FMT_AUDIO_MASK) >> HDMI_MAI_FMT_AUDIO_SHIFT) ==
           HDMI_MAI_FMT_PCM;
}

static void bcm2711_hdmi_audio_flush(BCM2711HDMIState *s, int available)
{
    while (s->voice && s->audio_active && s->audio_used && available > 0) {
        unsigned int frames = MIN(s->audio_used,
                                  BCM2711_HDMI_AUDIO_FRAMES - s->audio_read);
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
                        BCM2711_HDMI_AUDIO_FRAMES;
        s->audio_used -= written / (2 * sizeof(int32_t));
        available -= written;
    }
}

static void bcm2711_hdmi_audio_callback(void *opaque, int available)
{
    bcm2711_hdmi_audio_flush(opaque, available);
}

static void bcm2711_hdmi_audio_push(BCM2711HDMIState *s,
                                    int32_t left, int32_t right)
{
    unsigned int write;

    if (!s->voice || !s->audio_active) {
        return;
    }
    if (s->audio_used == BCM2711_HDMI_AUDIO_FRAMES) {
        s->audio_read = (s->audio_read + 1) % BCM2711_HDMI_AUDIO_FRAMES;
        s->audio_used--;
    }
    write = (s->audio_read + s->audio_used) % BCM2711_HDMI_AUDIO_FRAMES;
    s->audio_buffer[write * 2] = left;
    s->audio_buffer[write * 2 + 1] = right;
    s->audio_used++;
}

static void bcm2711_hdmi_update_audio(BCM2711HDMIState *s)
{
    uint32_t rate = bcm2711_hdmi_mai_rate(s);
    bool active;

    if (rate && rate != s->audio_rate) {
        struct audsettings settings = {
            .freq = rate,
            .nchannels = 2,
            .fmt = AUDIO_FORMAT_S32,
            .big_endian = HOST_BIG_ENDIAN,
        };

        s->voice = audio_be_open_out(s->audio_be, s->voice,
                                     TYPE_BCM2711_HDMI ".out", s,
                                     bcm2711_hdmi_audio_callback, &settings);
        if (s->voice) {
            s->audio_rate = rate;
        } else {
            s->audio_rate = 0;
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: could not open HDMI audio at %u Hz\n",
                          TYPE_BCM2711_HDMI, rate);
        }
        s->audio_read = 0;
        s->audio_used = 0;
    }

    active = s->voice && s->connected && bcm2711_hdmi_mai_running(s) &&
             bcm2711_hdmi_mai_pcm(s) && rate == s->audio_rate;
    if (active != s->audio_active) {
        s->audio_active = active;
        audio_be_set_active_out(s->audio_be, s->voice, active);
        if (!active) {
            s->audio_read = 0;
            s->audio_used = 0;
        }
    }
}

static void bcm2711_hdmi_update_dreq(BCM2711HDMIState *s)
{
    uint32_t threshold = *bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                                HDMI_MAI_THR);
    unsigned int low = threshold & HDMI_MAI_THR_DREQLOW_MASK;
    unsigned int high = (threshold & HDMI_MAI_THR_DREQHIGH_MASK) >>
                        HDMI_MAI_THR_DREQHIGH_SHIFT;
    unsigned int level = fifo32_num_used(&s->mai_fifo);

    high = MAX(high, low);
    if (!bcm2711_hdmi_mai_running(s)) {
        s->dreq_level = false;
    } else if (s->dreq_level) {
        s->dreq_level = level < high;
    } else {
        s->dreq_level = level < low;
    }
    qemu_set_irq(s->audio_dreq, s->dreq_level);
}

static int32_t bcm2711_hdmi_decode_subframe(uint32_t value)
{
    if (value & IEC958_SUBFRAME_VALIDITY) {
        return 0;
    }
    return (int32_t)((value & IEC958_SUBFRAME_SAMPLE_MASK) << 4);
}

static uint64_t bcm2711_hdmi_sample_delay(BCM2711HDMIState *s)
{
    uint32_t rate = bcm2711_hdmi_mai_rate(s);
    uint64_t delay;

    if (!rate) {
        return 0;
    }
    delay = NANOSECONDS_PER_SECOND / rate;
    s->sample_remainder += NANOSECONDS_PER_SECOND % rate;
    if (s->sample_remainder >= rate) {
        delay += s->sample_remainder / rate;
        s->sample_remainder %= rate;
    }
    return MAX(delay, 1);
}

static void bcm2711_hdmi_schedule_sample(BCM2711HDMIState *s)
{
    int64_t now;
    int64_t expiry;

    if (!bcm2711_hdmi_mai_running(s)) {
        timer_del(&s->mai_timer);
        s->next_sample_ns = 0;
        return;
    }
    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (!s->next_sample_ns) {
        s->next_sample_ns = now + bcm2711_hdmi_sample_delay(s);
    }
    expiry = s->next_sample_ns > now ? s->next_sample_ns :
             now + BCM2711_HDMI_CATCHUP_NS;
    timer_mod(&s->mai_timer, expiry);
}

static void bcm2711_hdmi_consume_sample(BCM2711HDMIState *s)
{
    uint32_t *control = bcm2711_hdmi_bank_reg(s, BCM2711_HDMI_HD,
                                              HDMI_MAI_CTL);
    unsigned int channels = bcm2711_hdmi_mai_channels(s);
    int32_t left = 0;
    int32_t right = 0;

    if (fifo32_num_used(&s->mai_fifo) >= channels) {
        for (unsigned int channel = 0; channel < channels; channel++) {
            int32_t sample = bcm2711_hdmi_decode_subframe(
                fifo32_pop(&s->mai_fifo));

            if (channel == 0) {
                left = sample;
            } else if (channel == 1) {
                right = sample;
            }
        }
        if (channels == 1) {
            right = left;
        }
    } else {
        fifo32_reset(&s->mai_fifo);
        *control |= HDMI_MAI_CTL_DLATE | HDMI_MAI_CTL_ERRORE;
    }

    bcm2711_hdmi_audio_push(s, left, right);
    bcm2711_hdmi_update_dreq(s);
}

static void bcm2711_hdmi_sample(void *opaque)
{
    BCM2711HDMIState *s = opaque;
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    unsigned int budget = BCM2711_HDMI_SAMPLE_BUDGET;

    if (!bcm2711_hdmi_mai_running(s)) {
        s->next_sample_ns = 0;
        return;
    }

    do {
        bcm2711_hdmi_consume_sample(s);
        s->next_sample_ns += bcm2711_hdmi_sample_delay(s);
        budget--;
    } while (budget && s->next_sample_ns <= now);

    bcm2711_hdmi_schedule_sample(s);
}

static uint64_t bcm2711_hdmi_read(void *opaque, hwaddr offset,
                                  unsigned int size)
{
    BCM2711HDMIRegBank *bank = opaque;
    BCM2711HDMIState *s = bank->owner;
    uint32_t value = *bcm2711_hdmi_reg(bank, offset);

    if (bank->id == BCM2711_HDMI_CORE) {
        switch (offset) {
        case HDMI_HOTPLUG:
            return s->connected ? HDMI_HOTPLUG_CONNECTED : 0;
        case HDMI_FIFO_CTL:
            return value | HDMI_FIFO_CTL_RECENTER_DONE;
        case HDMI_RAM_PACKET_STATUS:
            return s->regs[bank->first +
                           (HDMI_RAM_PACKET_CONFIG >> 2)] & 0xffff;
        default:
            break;
        }
    } else if (bank->id == BCM2711_HDMI_HD) {
        switch (offset) {
        case HDMI_MAI_CTL:
            value &= ~(HDMI_MAI_CTL_BUSY | HDMI_MAI_CTL_FULL |
                       HDMI_MAI_CTL_EMPTY);
            if (bcm2711_hdmi_mai_running(s)) {
                value |= HDMI_MAI_CTL_BUSY;
            }
            if (fifo32_is_full(&s->mai_fifo)) {
                value |= HDMI_MAI_CTL_FULL;
            }
            if (fifo32_is_empty(&s->mai_fifo)) {
                value |= HDMI_MAI_CTL_EMPTY;
            }
            return value;
        case HDMI_MAI_DATA:
            return 0;
        default:
            break;
        }
    }

    return value;
}

static void bcm2711_hdmi_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned int size)
{
    BCM2711HDMIRegBank *bank = opaque;
    BCM2711HDMIState *s = bank->owner;
    uint32_t *reg = bcm2711_hdmi_reg(bank, offset);

    if (bank->id == BCM2711_HDMI_CORE) {
        switch (offset) {
        case HDMI_HOTPLUG:
        case HDMI_RAM_PACKET_STATUS:
            return;
        case HDMI_FIFO_CTL:
            *reg = ((uint32_t)value & HDMI_FIFO_VALID_WRITE_MASK) |
                   HDMI_FIFO_CTL_RECENTER_DONE;
            return;
        case HDMI_SCHEDULER_CONTROL:
            *reg = (uint32_t)value & ~HDMI_SCHEDULER_HDMI_ACTIVE;
            if (*reg & HDMI_SCHEDULER_MODE_HDMI) {
                *reg |= HDMI_SCHEDULER_HDMI_ACTIVE;
            }
            return;
        default:
            break;
        }
    } else if (bank->id == BCM2711_HDMI_HD) {
        switch (offset) {
        case HDMI_MAI_CTL:
        {
            uint32_t status = *reg & HDMI_MAI_CTL_STATUS_MASK;

            status &= ~((uint32_t)value & HDMI_MAI_CTL_STATUS_MASK);
            if (value & HDMI_MAI_CTL_RESET) {
                status = 0;
                s->sample_remainder = 0;
                timer_del(&s->mai_timer);
                s->next_sample_ns = 0;
            }
            if (value & (HDMI_MAI_CTL_RESET | HDMI_MAI_CTL_FLUSH)) {
                fifo32_reset(&s->mai_fifo);
            }
            *reg = ((uint32_t)value & HDMI_MAI_CTL_RW_MASK) | status;
            bcm2711_hdmi_update_audio(s);
            bcm2711_hdmi_update_dreq(s);
            bcm2711_hdmi_schedule_sample(s);
            return;
        }
        case HDMI_MAI_THR:
            *reg = value;
            bcm2711_hdmi_update_dreq(s);
            return;
        case HDMI_MAI_FMT:
            *reg = value;
            s->sample_remainder = 0;
            timer_del(&s->mai_timer);
            s->next_sample_ns = 0;
            bcm2711_hdmi_update_audio(s);
            bcm2711_hdmi_update_dreq(s);
            bcm2711_hdmi_schedule_sample(s);
            return;
        case HDMI_MAI_DATA:
            if (fifo32_is_full(&s->mai_fifo)) {
                uint32_t *control = bcm2711_hdmi_bank_reg(
                    s, BCM2711_HDMI_HD, HDMI_MAI_CTL);

                *control |= HDMI_MAI_CTL_ERRORF;
            } else {
                fifo32_push(&s->mai_fifo, value);
            }
            bcm2711_hdmi_update_dreq(s);
            return;
        default:
            break;
        }
    }

    *reg = value;
}

static const MemoryRegionOps bcm2711_hdmi_ops = {
    .read = bcm2711_hdmi_read,
    .write = bcm2711_hdmi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void bcm2711_hdmi_reset_state(BCM2711HDMIState *s)
{
    BCM2711HDMIRegBank *core = &s->banks[BCM2711_HDMI_CORE];
    BCM2711HDMIRegBank *dvp = &s->banks[BCM2711_HDMI_DVP];

    memset(s->regs, 0, sizeof(s->regs));
    timer_del(&s->mai_timer);
    fifo32_reset(&s->mai_fifo);
    s->next_sample_ns = 0;
    s->sample_remainder = 0;
    s->dreq_level = false;
    qemu_set_irq(s->audio_dreq, false);
    s->audio_read = 0;
    s->audio_used = 0;
    if (s->voice && s->audio_active) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
    }
    s->audio_active = false;
    s->regs[core->first + (HDMI_FIFO_CTL >> 2)] =
        HDMI_FIFO_CTL_RECENTER_DONE;
    s->regs[dvp->first + (HDMI_DVP_CLOCK_STOP >> 2)] = 3;
}

static void bcm2711_hdmi_reset(DeviceState *dev)
{
    bcm2711_hdmi_reset_state(BCM2711_HDMI(dev));
}

static void bcm2711_hdmi_reset_input(void *opaque, int irq, int level)
{
    BCM2711HDMIState *s = opaque;

    if (level) {
        bcm2711_hdmi_reset_state(s);
    }
}

static void bcm2711_hdmi_clock_input(void *opaque, int irq, int level)
{
    BCM2711HDMIState *s = opaque;

    s->clock_enabled = level;
    bcm2711_hdmi_update_audio(s);
    bcm2711_hdmi_update_dreq(s);
    bcm2711_hdmi_schedule_sample(s);
}

static int bcm2711_hdmi_post_load(void *opaque, int version_id)
{
    BCM2711HDMIState *s = opaque;
    BCM2711HDMIRegBank *core = &s->banks[BCM2711_HDMI_CORE];
    uint32_t *fifo = &s->regs[core->first + (HDMI_FIFO_CTL >> 2)];
    uint32_t *scheduler =
        &s->regs[core->first + (HDMI_SCHEDULER_CONTROL >> 2)];

    if (version_id < 2) {
        timer_del(&s->mai_timer);
        fifo32_reset(&s->mai_fifo);
        s->next_sample_ns = 0;
        s->sample_remainder = 0;
    }
    if (fifo32_num_used(&s->mai_fifo) > BCM2711_HDMI_MAI_FIFO_WORDS) {
        return -EINVAL;
    }

    *fifo = (*fifo & HDMI_FIFO_VALID_WRITE_MASK) |
            HDMI_FIFO_CTL_RECENTER_DONE;
    *scheduler &= ~HDMI_SCHEDULER_HDMI_ACTIVE;
    if (*scheduler & HDMI_SCHEDULER_MODE_HDMI) {
        *scheduler |= HDMI_SCHEDULER_HDMI_ACTIVE;
    }
    s->audio_read = 0;
    s->audio_used = 0;
    if (s->voice && s->audio_active) {
        audio_be_set_active_out(s->audio_be, s->voice, false);
    }
    s->audio_active = false;
    s->dreq_level = false;
    bcm2711_hdmi_update_audio(s);
    bcm2711_hdmi_update_dreq(s);
    if (!timer_pending(&s->mai_timer)) {
        bcm2711_hdmi_schedule_sample(s);
    }
    return 0;
}

static bool bcm2711_hdmi_audio_state_needed(void *opaque, int version_id)
{
    return version_id >= 2;
}

static const VMStateDescription vmstate_bcm2711_hdmi = {
    .name = TYPE_BCM2711_HDMI,
    .version_id = 3,
    .minimum_version_id = 1,
    .post_load = bcm2711_hdmi_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, BCM2711HDMIState, BCM2711_HDMI_REGS),
        VMSTATE_BOOL(clock_enabled, BCM2711HDMIState),
        /*
         * Hot-plug state is guest-visible through HDMI_HOTPLUG, so it has to
         * survive migration.  Version 3: a stream from an older version leaves
         * the destination's default in place.
         */
        VMSTATE_BOOL_V(connected, BCM2711HDMIState, 3),
        VMSTATE_UINT64_V(next_sample_ns, BCM2711HDMIState, 2),
        VMSTATE_UINT64_V(sample_remainder, BCM2711HDMIState, 2),
        VMSTATE_FIFO8_TEST(mai_fifo.fifo, BCM2711HDMIState,
                           bcm2711_hdmi_audio_state_needed),
        VMSTATE_TIMER_V(mai_timer, BCM2711HDMIState, 2),
        VMSTATE_END_OF_LIST()
    },
};

/*
 * "connected" models the HDMI hot-plug detect line.  It is a runtime QOM
 * property rather than a static qdev one so a display can be attached or
 * removed while the guest runs.  The VC4 DRM connector is polled
 * (DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT) and its
 * BCM2711 detect path reads HDMI_HOTPLUG, so no separate HPD interrupt is
 * required for the guest to notice a change.
 */
static bool bcm2711_hdmi_get_connected(Object *obj, Error **errp)
{
    return BCM2711_HDMI(obj)->connected;
}

static void bcm2711_hdmi_set_connected(Object *obj, bool value, Error **errp)
{
    BCM2711HDMIState *s = BCM2711_HDMI(obj);

    if (s->connected == value) {
        return;
    }

    s->connected = value;

    /*
     * Audio is gated on the hot-plug state, so a disconnect must stop an
     * in-flight stream rather than leave it running against an absent sink.
     */
    bcm2711_hdmi_update_audio(s);
}

static const Property bcm2711_hdmi_properties[] = {
    DEFINE_AUDIO_PROPERTIES(BCM2711HDMIState, audio_be),
};

static void bcm2711_hdmi_realize(DeviceState *dev, Error **errp)
{
    BCM2711HDMIState *s = BCM2711_HDMI(dev);

    if (!audio_be_check(&s->audio_be, errp)) {
        return;
    }
}

static void bcm2711_hdmi_unrealize(DeviceState *dev)
{
    BCM2711HDMIState *s = BCM2711_HDMI(dev);

    if (s->voice) {
        audio_be_close_out(s->audio_be, s->voice);
        s->voice = NULL;
    }
}

static void bcm2711_hdmi_init(Object *obj)
{
    BCM2711HDMIState *s = BCM2711_HDMI(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    uint16_t first = 0;

    fifo32_create(&s->mai_fifo, BCM2711_HDMI_MAI_FIFO_WORDS);
    timer_init_ns(&s->mai_timer, QEMU_CLOCK_VIRTUAL,
                  bcm2711_hdmi_sample, s);

    s->connected = true;
    object_property_add_bool(obj, "connected",
                             bcm2711_hdmi_get_connected,
                             bcm2711_hdmi_set_connected);

    for (unsigned int i = 0; i < BCM2711_HDMI_BANKS; i++) {
        BCM2711HDMIRegBank *bank = &s->banks[i];

        bank->owner = s;
        bank->first = first;
        bank->words = bcm2711_hdmi_bank_words[i];
        bank->id = i;
        memory_region_init_io(&bank->iomem, obj, &bcm2711_hdmi_ops, bank,
                              bcm2711_hdmi_bank_names[i], bank->words * 4);
        sysbus_init_mmio(sbd, &bank->iomem);
        first += bank->words;
    }
    g_assert(first == BCM2711_HDMI_REGS);

    qdev_init_gpio_in_named(DEVICE(obj), bcm2711_hdmi_reset_input,
                            "reset", 1);
    qdev_init_gpio_in_named(DEVICE(obj), bcm2711_hdmi_clock_input,
                            "clock-enable", 1);
    qdev_init_gpio_out_named(DEVICE(obj), &s->audio_dreq,
                             "audio-dreq", 1);
}

static void bcm2711_hdmi_finalize(Object *obj)
{
    BCM2711HDMIState *s = BCM2711_HDMI(obj);

    timer_deinit(&s->mai_timer);
    fifo32_destroy(&s->mai_fifo);
}

static void bcm2711_hdmi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = bcm2711_hdmi_realize;
    dc->unrealize = bcm2711_hdmi_unrealize;
    device_class_set_legacy_reset(dc, bcm2711_hdmi_reset);
    device_class_set_props(dc, bcm2711_hdmi_properties);
    dc->vmsd = &vmstate_bcm2711_hdmi;
    dc->desc = "BCM2711 HDMI transmitter";
}

static const TypeInfo bcm2711_hdmi_info = {
    .name = TYPE_BCM2711_HDMI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2711HDMIState),
    .instance_init = bcm2711_hdmi_init,
    .instance_finalize = bcm2711_hdmi_finalize,
    .class_init = bcm2711_hdmi_class_init,
};

static void bcm2711_hdmi_register_types(void)
{
    type_register_static(&bcm2711_hdmi_info);
}

type_init(bcm2711_hdmi_register_types)
