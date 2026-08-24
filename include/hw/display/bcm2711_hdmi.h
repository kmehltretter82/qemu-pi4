/*
 * BCM2711 HDMI transmitter
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_DISPLAY_BCM2711_HDMI_H
#define HW_DISPLAY_BCM2711_HDMI_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_BCM2711_HDMI "bcm2711-hdmi"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2711HDMIState, BCM2711_HDMI)

enum BCM2711HDMIBankID {
    BCM2711_HDMI_CORE,
    BCM2711_HDMI_DVP,
    BCM2711_HDMI_PHY,
    BCM2711_HDMI_RM,
    BCM2711_HDMI_PACKET,
    BCM2711_HDMI_METADATA,
    BCM2711_HDMI_CSC,
    BCM2711_HDMI_CEC,
    BCM2711_HDMI_HD,
    BCM2711_HDMI_BANKS,
};

#define BCM2711_HDMI_REGS 928

typedef struct BCM2711HDMIRegBank {
    MemoryRegion iomem;
    BCM2711HDMIState *owner;
    uint16_t first;
    uint16_t words;
    uint8_t id;
} BCM2711HDMIRegBank;

struct BCM2711HDMIState {
    SysBusDevice parent_obj;

    BCM2711HDMIRegBank banks[BCM2711_HDMI_BANKS];
    uint32_t regs[BCM2711_HDMI_REGS];

    bool connected;
    bool clock_enabled;
};

#endif /* HW_DISPLAY_BCM2711_HDMI_H */
