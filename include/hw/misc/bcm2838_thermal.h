/*
 * BCM2838 AVS thermal monitor
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_BCM2838_THERMAL_H
#define HW_MISC_BCM2838_THERMAL_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2838_THERMAL "bcm2838-thermal"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2838ThermalState, BCM2838_THERMAL)

struct BCM2838ThermalState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint16_t raw_temperature;
};

#endif /* HW_MISC_BCM2838_THERMAL_H */
