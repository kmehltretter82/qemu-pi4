/*
 * BCM2711 HDMI DVP clock and reset controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_BCM2711_DVP_H
#define HW_MISC_BCM2711_DVP_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2711_DVP "bcm2711-dvp"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711DVPState, BCM2711_DVP)

#define BCM2711_DVP_RESETS 6
#define BCM2711_DVP_CLOCKS 2

struct BCM2711DVPState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq reset[BCM2711_DVP_RESETS];
    qemu_irq clock_enable[BCM2711_DVP_CLOCKS];

    uint32_t sw_init;
    uint32_t misc_config;
};

#endif /* HW_MISC_BCM2711_DVP_H */
