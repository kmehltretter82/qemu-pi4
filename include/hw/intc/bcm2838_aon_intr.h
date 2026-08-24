/*
 * BCM2711 always-on L2 interrupt controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INTC_BCM2838_AON_INTR_H
#define HW_INTC_BCM2838_AON_INTR_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2838_AON_INTR "bcm2838-aon-intr"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2838AonIntrState, BCM2838_AON_INTR)

#define BCM2838_AON_INTR_LINES 12
#define BCM2838_AON_INTR_OUTPUTS 2

struct BCM2838AonIntrState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq[BCM2838_AON_INTR_OUTPUTS];

    uint32_t status[BCM2838_AON_INTR_OUTPUTS];
    uint32_t mask[BCM2838_AON_INTR_OUTPUTS];
    uint32_t input_level;
};

#endif /* HW_INTC_BCM2838_AON_INTR_H */
