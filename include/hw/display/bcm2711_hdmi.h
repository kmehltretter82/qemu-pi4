/*
 * BCM2711 HDMI transmitter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_BCM2711_HDMI_H
#define HW_DISPLAY_BCM2711_HDMI_H

#include "hw/core/sysbus.h"
#include "qemu/audio.h"
#include "qemu/fifo32.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_BCM2711_HDMI "bcm2711-hdmi"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711HDMIState, BCM2711_HDMI)

enum BCM2711HDMIBankID {
    BCM2711_HDMI_CORE,
    BCM2711_HDMI_DVP,
    BCM2711_HDMI_PHY,
    BCM2711_HDMI_RM,
    BCM2711_HDMI_PACKET,
    BCM2711_HDMI_METADATA,
    BCM2711_HDMI_CSC,
    BCM2711_HDMI_CEC,
    BCM2711_HDMI_HD,
    BCM2711_HDMI_BANKS,
};

#define BCM2711_HDMI_REGS 928
#define BCM2711_HDMI_MAI_FIFO_WORDS 64
#define BCM2711_HDMI_AUDIO_FRAMES 4096

typedef struct BCM2711HDMIRegBank {
    MemoryRegion iomem;
    BCM2711HDMIState *owner;
    uint16_t first;
    uint16_t words;
    uint8_t id;
} BCM2711HDMIRegBank;

struct BCM2711HDMIState {
    SysBusDevice parent_obj;

    BCM2711HDMIRegBank banks[BCM2711_HDMI_BANKS];
    uint32_t regs[BCM2711_HDMI_REGS];

    qemu_irq audio_dreq;
    QEMUTimer mai_timer;
    Fifo32 mai_fifo;

    uint64_t next_sample_ns;
    uint64_t sample_remainder;
    bool dreq_level;

    bool connected;
    bool clock_enabled;

    AudioBackend *audio_be;
    SWVoiceOut *voice;
    uint32_t audio_rate;
    bool audio_active;
    int32_t audio_buffer[BCM2711_HDMI_AUDIO_FRAMES * 2];
    uint32_t audio_read;
    uint32_t audio_used;
};

#endif /* HW_DISPLAY_BCM2711_HDMI_H */
