/*
 * BCM2711 V3D 4.2 register block
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_BCM2711_V3D_H
#define HW_DISPLAY_BCM2711_V3D_H

#include "hw/core/sysbus.h"

#define TYPE_BCM2711_V3D "bcm2711-v3d"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711V3DState, BCM2711_V3D)

/* BCM2711 exposes the V3D hub and core0 through separate windows. */
#define BCM2711_V3D_MMIO_SIZE      0x4000
#define BCM2711_V3D_REGS           (BCM2711_V3D_MMIO_SIZE / sizeof(uint32_t))

#define BCM2711_V3D_HUB            0
#define BCM2711_V3D_CORE0          1

struct BCM2711V3DState {
    SysBusDevice parent_obj;

    MemoryRegion hub_iomem;
    MemoryRegion core0_iomem;
    qemu_irq irq;

    /*
     * Deliberately opt-in: this only exposes the DT node for driver-probe
     * validation.  Command-list execution is not modeled.
     */
    bool enable_probe_dtb;

    uint32_t hub_regs[BCM2711_V3D_REGS];
    uint32_t core0_regs[BCM2711_V3D_REGS];
};

#endif /* HW_DISPLAY_BCM2711_V3D_H */
