/*
 * BCM2835 PCM / I2S controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AUDIO_BCM2835_I2S_H
#define HW_AUDIO_BCM2835_I2S_H

#include "hw/core/clock.h"
#include "hw/core/sysbus.h"
#include "qemu/audio.h"
#include "qemu/fifo32.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_BCM2835_I2S "bcm2835-i2s"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2835I2SState, BCM2835_I2S)

#define BCM2835_I2S_FIFO_WORDS 64
#define BCM2835_I2S_AUDIO_FRAMES 4096

struct BCM2835I2SState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq dreq[2];
    Clock *pcm_clk;
    QEMUTimer frame_timer;
    QEMUTimer fifo_clear_timer;
    QEMUTimer sync_timer;

    Fifo32 tx_fifo;
    Fifo32 rx_fifo;

    uint32_t cs;
    uint32_t mode;
    uint32_t rxc;
    uint32_t txc;
    uint32_t dreq_reg;
    uint32_t inten;
    uint32_t int_status;
    uint32_t gray;
    uint64_t pcm_clk_hz;
    uint64_t slave_clock_hz;
    uint64_t next_frame_ns;
    uint64_t frame_remainder;
    uint8_t fifo_clear_pending;
    bool sync_pending;
    bool sync_target;

    AudioBackend *audio_be;
    SWVoiceOut *voice;
    uint32_t audio_rate;
    bool audio_active;
    int32_t audio_buffer[BCM2835_I2S_AUDIO_FRAMES * 2];
    uint32_t audio_read;
    uint32_t audio_used;
    uint32_t trace_frame_count;
};

#endif /* HW_AUDIO_BCM2835_I2S_H */
