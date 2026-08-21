/*
 * BCM2838 SoC emulation
 *
 * Copyright (C) 2022 Ovchinnikov Vitalii <vitalii.ovchinnikov@auriga.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BCM2838_H
#define BCM2838_H

#include "hw/core/sysbus.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/bcm2836_control.h"
#include "hw/arm/bcm2838_peripherals.h"
#include "target/arm/cpu.h"

#define BCM2838_PERI_LOW_BASE 0xfc000000
#define BCM2838_GIC_BASE 0x40000

#define TYPE_BCM2838 "bcm2838"
#define BCM2838_NCPUS 4

OBJECT_DECLARE_SIMPLE_TYPE(BCM2838State, BCM2838)

struct BCM2838State {
    /*< private >*/
    DeviceState parent_obj;
    /*< public >*/
    uint32_t enabled_cpus;
    struct {
        ARMCPU core;
    } cpu[BCM2838_NCPUS];
    BCM2836ControlState control;
    BCM2838PeripheralState peripherals;
    GICState gic;
};

#endif /* BCM2838_H */
