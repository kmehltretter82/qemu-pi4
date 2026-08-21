/*
 * Raspberry Pi 4 / BCM2711 platform definitions
 *
 * These definitions are derived from those in Raspbian Linux at
 * arch/arm/mach-{bcm2708,bcm2709}/include/mach/platform.h
 * where they carry the following notice:
 *
 * Copyright (C) 2010 Broadcom
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Various undocumented addresses and names come from Herman Hermitage's VC4
 * documentation:
 * https://github.com/hermanhermitage/videocoreiv/wiki/MMIO-Register-map
 */

#ifndef HW_ARM_RASPI4_PLATFORM_H
#define HW_ARM_RASPI4_PLATFORM_H

#include "hw/core/boards.h"
#include "hw/arm/boot.h"

#define TYPE_RASPI4_BASE_MACHINE MACHINE_TYPE_NAME("raspi4-base")
OBJECT_DECLARE_TYPE(Raspi4BaseMachineState, Raspi4BaseMachineClass,
                    RASPI4_BASE_MACHINE)

struct Raspi4BaseMachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    struct arm_boot_info binfo;
};

struct Raspi4BaseMachineClass {
    /*< private >*/
    MachineClass parent_obj;
    /*< public >*/
    uint32_t board_rev;
};

typedef struct BCM2838State BCM2838State;
void raspi4_common_machine_init(MachineState *machine, BCM2838State *soc);

void raspi4_common_machine_class_init(MachineClass *mc,
                                      uint32_t board_rev,
                                      const char *model);
uint64_t raspi4_board_ram_size(uint32_t board_rev);

#define ST_OFFSET               0x3000   /* System Timer */
#define TXP_OFFSET              0x4000   /* Transposer */
#define DMA_OFFSET              0x7000   /* DMA controller, channels 0-14 */
#define BRDG_OFFSET             0xa000   /* RPiVid ASB for BCM2838 (BCM2711) */
#define ARM_OFFSET              0xB000   /* ARM control block */
#define ARMCTRL_OFFSET          (ARM_OFFSET + 0x000)
#define ARMCTRL_IC_OFFSET       (ARM_OFFSET + 0x200) /* Interrupt controller */
#define ARMCTRL_TIMER0_1_OFFSET (ARM_OFFSET + 0x400) /* Timer 0 and 1 (SP804) */
/* User 0 (ARM) semaphores, doorbells, and mailboxes */
#define ARMCTRL_0_SBM_OFFSET    (ARM_OFFSET + 0x800)
#define PM_OFFSET               0x100000 /* Power Management */
#define CPRMAN_OFFSET           0x101000 /* Clock Management */
#define GPIO_OFFSET             0x200000
#define UART0_OFFSET            0x201000 /* PL011 */
#define MMCI0_OFFSET            0x202000 /* Legacy MMC */
#define I2S_OFFSET              0x203000 /* PCM */
#define SPI0_OFFSET             0x204000 /* SPI master */
#define BSC0_OFFSET             0x205000 /* BSC0 I2C/TWI */
#define OTP_OFFSET              0x20f000
#define BSC_SL_OFFSET           0x214000 /* SPI slave (bootrom) */
#define AUX_OFFSET              0x215000 /* AUX: UART1/SPI1/SPI2 */
#define EMMC1_OFFSET            0x300000
#define EMMC2_OFFSET            0x340000
#define SMI_OFFSET              0x600000
#define BSC1_OFFSET             0x804000 /* BSC1 I2C/TWI */
#define BSC2_OFFSET             0x805000 /* BSC2 I2C/TWI */
#define DBUS_OFFSET             0x900000
#define AVE0_OFFSET             0x910000
#define USB_OTG_OFFSET          0x980000 /* DTC_OTG USB controller */
#define V3D_OFFSET              0xc00000
#define SDRAMC_OFFSET           0xe00000
#define DMA15_OFFSET            0xE05000 /* DMA controller, channel 15 */

/* GPU interrupts */
#define INTERRUPT_TIMER0               0
#define INTERRUPT_TIMER1               1
#define INTERRUPT_TIMER2               2
#define INTERRUPT_TIMER3               3
#define INTERRUPT_USB                  9
#define INTERRUPT_AUX                  29
#define INTERRUPT_HOSTPORT             32
#define INTERRUPT_I2C                  53
#define INTERRUPT_SPI                  54
#define INTERRUPT_SDIO                 56
#define INTERRUPT_UART0                57
#define INTERRUPT_ARASANSDIO           62

/* ARM CPU IRQs use a private number space */
#define INTERRUPT_ARM_MAILBOX          1

/* Clock rates */
#define RPI_FIRMWARE_EMMC_CLK_RATE    50000000
#define RPI_FIRMWARE_UART_CLK_RATE    3000000
/*
 * TODO: this is really SoC-specific; we might want to
 * set it per-SoC if it turns out any guests care.
 */
#define RPI_FIRMWARE_CORE_CLK_RATE    350000000
#define RPI_FIRMWARE_DEFAULT_CLK_RATE 700000000

#endif /* HW_ARM_RASPI4_PLATFORM_H */
