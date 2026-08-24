/*
 * BCM2711 HDMI DDC I2C controller
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_I2C_BCM2711_HDMI_I2C_H
#define HW_I2C_BCM2711_HDMI_I2C_H

#include "hw/core/sysbus.h"
#include "hw/i2c/i2c.h"
#include "qom/object.h"

#define TYPE_BCM2711_HDMI_I2C "bcm2711-hdmi-i2c"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711HDMII2CState, BCM2711_HDMI_I2C)

#define BCM2711_HDMI_I2C_DATA_REGS 8

struct BCM2711HDMII2CState {
    SysBusDevice parent_obj;

    MemoryRegion bsc_iomem;
    MemoryRegion auto_i2c_iomem;
    I2CBus *bus;

    uint32_t chip_address;
    uint32_t data_in[BCM2711_HDMI_I2C_DATA_REGS];
    uint32_t count;
    uint32_t control;
    uint32_t iic_enable;
    uint32_t data_out[BCM2711_HDMI_I2C_DATA_REGS];
    uint32_t control_high;
    uint32_t scl_param;
    bool released;
};

#endif /* HW_I2C_BCM2711_HDMI_I2C_H */
