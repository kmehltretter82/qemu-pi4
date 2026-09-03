/*
 * Rasperry Pi 2 emulation and refactoring Copyright (c) 2015, Microsoft
 * Written by Andrew Baumann
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef BCM2835_AUX_H
#define BCM2835_AUX_H

#include "hw/core/sysbus.h"
#include "hw/ssi/ssi.h"
#include "chardev/char-fe.h"
#include "qemu/fifo8.h"
#include "qom/object.h"

#define TYPE_BCM2835_AUX "bcm2835-aux"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2835AuxState, BCM2835_AUX)

#define BCM2835_AUX_RX_FIFO_LEN 8
#define BCM2835_AUX_SPI_COUNT 2
/* The Linux driver keeps at most four three-byte words in flight. */
#define BCM2835_AUX_SPI_RX_FIFO_LEN 12

typedef struct BCM2835AuxSPIState {
    SSIBus *bus;
    Fifo8 rx_fifo;
    uint32_t cntl0;
    uint32_t cntl1;
} BCM2835AuxSPIState;

struct BCM2835AuxState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion iomem;
    CharFrontend chr;
    qemu_irq irq;

    uint8_t read_fifo[BCM2835_AUX_RX_FIFO_LEN];
    uint8_t read_pos, read_count;
    uint8_t ier, iir, mcr, enables, scratch;

    BCM2835AuxSPIState spi[BCM2835_AUX_SPI_COUNT];
};

#endif
