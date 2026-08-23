/*
 * BCM2838 RNG200 random number generator
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_BCM2838_RNG200_H
#define HW_MISC_BCM2838_RNG200_H

#include "hw/core/sysbus.h"
#include "qemu/fifo8.h"
#include "qemu/timer.h"
#include "qom/object.h"

#define TYPE_BCM2838_RNG200 "bcm2838-rng200"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2838Rng200State, BCM2838_RNG200)

struct BCM2838Rng200State {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    QEMUTimer refill_timer;
    Fifo8 fifo;

    uint32_t ctrl;
    uint32_t total_bit_count;
    uint32_t total_bit_count_threshold;
    uint32_t int_status;
    uint32_t int_enable;
    uint32_t last_fifo_data;
    uint8_t fifo_threshold;
};

#endif /* HW_MISC_BCM2838_RNG200_H */
