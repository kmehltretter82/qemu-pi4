/*
 * libqos driver framework
 *
 * Copyright (c) 2018 Emanuele Giuseppe Esposito <e.emanuelegiuseppe@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "qemu/osdep.h"
#include "../libqtest.h"
#include "qemu/module.h"
#include "libqos-malloc.h"
#include "qgraph.h"
#include "sdhci.h"

#define AARCH64_PAGE_SIZE        4096
#define RASPI4_RAM_ADDR          0
#define RASPI4_RAM_SIZE          0x80000000ULL
#define RASPI4_SDHCI_ADDR        0xfe300000

typedef struct QRaspi4bMachine QRaspi4bMachine;

struct QRaspi4bMachine {
    QOSGraphObject obj;
    QGuestAllocator alloc;
    QSDHCI_MemoryMapped sdhci;
};

static void *raspi4b_get_driver(void *object, const char *interface)
{
    QRaspi4bMachine *machine = object;

    if (!g_strcmp0(interface, "memory")) {
        return &machine->alloc;
    }

    fprintf(stderr, "%s not present in aarch64/raspi4b\n", interface);
    g_assert_not_reached();
}

static QOSGraphObject *raspi4b_get_device(void *obj, const char *device)
{
    QRaspi4bMachine *machine = obj;

    if (!g_strcmp0(device, "generic-sdhci")) {
        return &machine->sdhci.obj;
    }

    fprintf(stderr, "%s not present in aarch64/raspi4b\n", device);
    g_assert_not_reached();
}

static void raspi4b_destructor(QOSGraphObject *obj)
{
    QRaspi4bMachine *machine = (QRaspi4bMachine *)obj;

    alloc_destroy(&machine->alloc);
}

static void *qos_create_machine_aarch64_raspi4b(QTestState *qts)
{
    QRaspi4bMachine *machine = g_new0(QRaspi4bMachine, 1);

    alloc_init(&machine->alloc, 0,
               RASPI4_RAM_ADDR + (1 << 20),
               RASPI4_RAM_ADDR + RASPI4_RAM_SIZE,
               AARCH64_PAGE_SIZE);
    machine->obj.get_device = raspi4b_get_device;
    machine->obj.get_driver = raspi4b_get_driver;
    machine->obj.destructor = raspi4b_destructor;
    qos_init_sdhci_mm(&machine->sdhci, qts, RASPI4_SDHCI_ADDR,
                      &(QSDHCIProperties) {
        .version = 3,
        .baseclock = 52,
        .capab.sdma = false,
        .capab.reg = 0x052134b4
    });
    return &machine->obj;
}

static void raspi4b_register_nodes(void)
{
#if HOST_LONG_BITS == 64
    qos_node_create_machine("aarch64/raspi400",
                            qos_create_machine_aarch64_raspi4b);
    qos_node_contains("aarch64/raspi400", "generic-sdhci", NULL);
#endif
    qos_node_create_machine("aarch64/raspi4b",
                            qos_create_machine_aarch64_raspi4b);
    qos_node_contains("aarch64/raspi4b", "generic-sdhci", NULL);
}

libqos_init(raspi4b_register_nodes);
