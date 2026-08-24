/*
 * BCM2835 / BCM2711 PWM controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AUDIO_BCM2835_PWM_H
#define HW_AUDIO_BCM2835_PWM_H

#include "hw/core/clock.h"
#include "hw/core/sysbus.h"
#include "qemu/audio.h"
#include "qemu/fifo32.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_BCM2835_PWM "bcm2835-pwm"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2835PWMState, BCM2835_PWM)

#define BCM2835_PWM_CHANNELS 2
#define BCM2835_PWM_MAX_FIFO_WORDS 64
#define BCM2835_PWM_AUDIO_FRAMES 4096
#define BCM2835_PWM_MAX_DUTY 1000000

typedef struct BCM2835PWMChannel {
    BCM2835PWMState *pwm;
    QEMUTimer timer;

    uint64_t next_period_ns;
    uint64_t period_remainder;
    uint32_t current_data;
    uint32_t last_data;
    uint32_t freq;
    uint32_t duty;
    uint8_t index;
    bool have_current;
    bool have_last;
    bool transmitting;
} BCM2835PWMChannel;

struct BCM2835PWMState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    Clock *pwm_clk;
    QEMUBH *dreq_bh;
    qemu_irq dreq;
    qemu_irq panic;
    qemu_irq duty_gpio_out[BCM2835_PWM_CHANNELS];
    Fifo32 fifo;
    BCM2835PWMChannel channel[BCM2835_PWM_CHANNELS];

    uint32_t ctl;
    uint32_t status;
    uint32_t dmac;
    uint32_t range[BCM2835_PWM_CHANNELS];
    uint32_t data[BCM2835_PWM_CHANNELS];
    uint32_t fifo_depth;
    uint32_t fifo_level;
    uint64_t pwm_clk_hz;
    uint8_t instance_id;
    bool audio_output;
    bool dreq_level;
    bool in_mmio_write;

    AudioBackend *audio_be;
    SWVoiceOut *voice;
    uint32_t audio_rate;
    bool audio_active;
    int32_t audio_buffer[BCM2835_PWM_AUDIO_FRAMES * 2];
    uint32_t audio_read;
    uint32_t audio_used;
    uint32_t trace_period_count;
};

#endif /* HW_AUDIO_BCM2835_PWM_H */
