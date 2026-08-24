/*
 * BCM2711 Hardware Video Scaler
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_BCM2711_HVS_H
#define HW_DISPLAY_BCM2711_HVS_H

#include "hw/core/sysbus.h"
#include "hw/display/bcm2835_fb.h"
#include "qom/object.h"

#define TYPE_BCM2711_HVS "bcm2711-hvs"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711HVSState, BCM2711_HVS)

#define BCM2711_HVS_MMIO_SIZE 0x8000
#define BCM2711_HVS_REGS (BCM2711_HVS_MMIO_SIZE / sizeof(uint32_t))

struct BCM2711HVSState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    BCM2835FBState *fb;

    uint32_t regs[BCM2711_HVS_REGS];
};

#endif /* HW_DISPLAY_BCM2711_HVS_H */
