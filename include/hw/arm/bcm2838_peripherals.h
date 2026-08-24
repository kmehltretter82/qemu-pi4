/*
 * BCM2838 peripherals emulation
 *
 * Copyright (C) 2022 Ovchinnikov Vitalii <vitalii.ovchinnikov@auriga.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BCM2838_PERIPHERALS_H
#define BCM2838_PERIPHERALS_H

#include "hw/arm/bcm2835_peripherals.h"
#include "hw/sd/sdhci.h"
#include "hw/gpio/bcm2838_gpio.h"
#include "hw/i2c/bcm2711_hdmi_i2c.h"
#include "hw/intc/bcm2838_aon_intr.h"
#include "hw/display/i2c-ddc.h"
#include "hw/misc/bcm2711_dvp.h"
#include "hw/misc/bcm2838_rng200.h"
#include "hw/misc/bcm2838_thermal.h"
#include "hw/net/bcm2838_genet.h"
#include "hw/pci-host/bcm2711.h"

/* SPI */
#define GIC_SPI_INTERRUPT_MBOX         33
#define GIC_SPI_INTERRUPT_MPHI         40
#define GIC_SPI_INTERRUPT_SYSTIMER_0   64
#define GIC_SPI_INTERRUPT_SYSTIMER_3   67
#define GIC_SPI_INTERRUPT_DWC2         73
#define GIC_SPI_INTERRUPT_DMA_0        80
#define GIC_SPI_INTERRUPT_DMA_6        86
#define GIC_SPI_INTERRUPT_DMA_7_8      87
#define GIC_SPI_INTERRUPT_DMA_9_10     88
#define GIC_SPI_INTERRUPT_AUX_UART1    93
#define GIC_SPI_INTERRUPT_AON          96
#define GIC_SPI_INTERRUPT_GPIO_0       113
#define GIC_SPI_INTERRUPT_GPIO_ALL     116
#define GIC_SPI_INTERRUPT_I2C          117
#define GIC_SPI_INTERRUPT_SPI0         118
#define GIC_SPI_INTERRUPT_PCM          119
#define GIC_SPI_INTERRUPT_SDHOST       120
#define GIC_SPI_INTERRUPT_UART0        121
#define GIC_SPI_INTERRUPT_RNG200       125
#define GIC_SPI_INTERRUPT_EMMC_EMMC2   126
#define GIC_SPI_INTERRUPT_PCI_INT_A    143
#define GIC_SPI_INTERRUPT_PCI_INT_B    144
#define GIC_SPI_INTERRUPT_PCI_INT_C    145
#define GIC_SPI_INTERRUPT_PCI_INT_D    146
#define GIC_SPI_INTERRUPT_PCIE         147
#define GIC_SPI_INTERRUPT_PCIE_MSI     148
#define GIC_SPI_INTERRUPT_GENET_A      157
#define GIC_SPI_INTERRUPT_GENET_B      158


/* GPU (legacy) DMA interrupts */
#define GPU_INTERRUPT_DMA0      16
#define GPU_INTERRUPT_DMA1      17
#define GPU_INTERRUPT_DMA2      18
#define GPU_INTERRUPT_DMA3      19
#define GPU_INTERRUPT_DMA4      20
#define GPU_INTERRUPT_DMA5      21
#define GPU_INTERRUPT_DMA6      22
#define GPU_INTERRUPT_DMA7_8    23
#define GPU_INTERRUPT_DMA9_10   24
#define GPU_INTERRUPT_DMA11     25
#define GPU_INTERRUPT_DMA12     26
#define GPU_INTERRUPT_DMA13     27
#define GPU_INTERRUPT_DMA14     28
#define GPU_INTERRUPT_DMA15     31

#define BCM2838_MPHI_OFFSET     0xb200
#define BCM2838_MPHI_SIZE       0x200

#define TYPE_BCM2838_PERIPHERALS "bcm2838-peripherals"
OBJECT_DECLARE_TYPE(BCM2838PeripheralState, BCM2838PeripheralClass,
                    BCM2838_PERIPHERALS)

struct BCM2838PeripheralState {
    /*< private >*/
    BCMSocPeripheralBaseState parent_obj;

    /*< public >*/
    MemoryRegion peri_low_mr;
    MemoryRegion peri_low_mr_alias;
    MemoryRegion mphi_mr_alias;

    SDHCIState emmc2;
    BCM2838GpioState gpio;
    BCM2838Rng200State rng200;
    BCM2838ThermalState thermal;
    BCM2838GenetState genet;
    BCM2711PcieHostState pcie;
    BCM2835PWMState pwm1;
    BCM2711DVPState dvp;
    BCM2711HDMII2CState hdmi_i2c[2];
    I2CDDCState hdmi0_edid;
    BCM2838AonIntrState aon_intr;

    OrIRQState mmc_irq_orgate;
    OrIRQState dma_7_8_irq_orgate;
    OrIRQState dma_9_10_irq_orgate;

    MemoryRegion asb_mr;
    MemoryRegion rpivid_asb_mr;
};

struct BCM2838PeripheralClass {
    /*< private >*/
    BCMSocPeripheralBaseClass parent_class;
    /*< public >*/
    uint64_t peri_low_size; /* Peripheral lower range size */
};

#endif /* BCM2838_PERIPHERALS_H */
