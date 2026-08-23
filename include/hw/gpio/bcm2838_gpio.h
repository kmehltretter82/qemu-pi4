/*
 * Raspberry Pi (BCM2838) GPIO Controller
 * This implementation is based on the BCM2835 GPIO programming model.
 *
 * Copyright (c) 2022 Auriga LLC
 *
 * Authors:
 *  Lotosh, Aleksey <aleksey.lotosh@auriga.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef BCM2838_GPIO_H
#define BCM2838_GPIO_H

#include "hw/sd/sd.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2838_GPIO "bcm2838-gpio"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2838GpioState, BCM2838_GPIO)

#define BCM2838_GPIO_REGS_SIZE 0x1000
#define BCM2838_GPIO_NUM       58
#define BCM2838_GPIO_REG_BANKS 2
#define BCM2838_GPIO_IRQS      4
#define GPIO_PUP_PDN_CNTRL_NUM 4

struct BCM2838GpioState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* SDBus selector */
    SDBus sdbus;
    SDBus *sdbus_sdhci;
    SDBus *sdbus_sdhost;

    uint8_t fsel[BCM2838_GPIO_NUM];
    /* Output latch state, retained while a pin is configured as an input. */
    uint32_t lev0, lev1;
    uint32_t input_level[BCM2838_GPIO_REG_BANKS];
    uint32_t event_status[BCM2838_GPIO_REG_BANKS];
    uint32_t rising_detect[BCM2838_GPIO_REG_BANKS];
    uint32_t falling_detect[BCM2838_GPIO_REG_BANKS];
    uint32_t high_detect[BCM2838_GPIO_REG_BANKS];
    uint32_t low_detect[BCM2838_GPIO_REG_BANKS];
    uint32_t async_rising_detect[BCM2838_GPIO_REG_BANKS];
    uint32_t async_falling_detect[BCM2838_GPIO_REG_BANKS];
    uint8_t sd_fsel;
    qemu_irq out[BCM2838_GPIO_NUM];
    qemu_irq irq[BCM2838_GPIO_IRQS];
    uint32_t pup_cntrl_reg[GPIO_PUP_PDN_CNTRL_NUM];
};

#endif
