/*
 * BCM2711 Pixel Valve
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_BCM2711_PIXELVALVE_H
#define HW_DISPLAY_BCM2711_PIXELVALVE_H

#include "hw/core/sysbus.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_BCM2711_PIXELVALVE "bcm2711-pixelvalve"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711PixelValveState, BCM2711_PIXELVALVE)

#define BCM2711_PIXELVALVE_MMIO_SIZE 0x100
#define BCM2711_PIXELVALVE_REGS \
    (BCM2711_PIXELVALVE_MMIO_SIZE / sizeof(uint32_t))

struct BCM2711PixelValveState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    QEMUTimer *vblank_timer;

    uint32_t regs[BCM2711_PIXELVALVE_REGS];
};

#endif /* HW_DISPLAY_BCM2711_PIXELVALVE_H */
