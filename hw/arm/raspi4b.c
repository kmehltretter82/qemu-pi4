/*
 * Raspberry Pi 4B emulation
 *
 * Copyright (C) 2022 Ovchinnikov Vitalii <vitalii.ovchinnikov@auriga.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "hw/arm/machines-qom.h"
#include "hw/arm/raspi4_platform.h"
#include "hw/display/bcm2835_fb.h"
#include "hw/core/registerfields.h"
#include "qemu/error-report.h"
#include "system/device_tree.h"
#include "hw/core/boards.h"
#include "hw/core/loader.h"
#include "hw/arm/boot.h"
#include "qom/object.h"
#include "hw/arm/bcm2838.h"
#include <libfdt.h>

#define TYPE_RASPI4B_MACHINE MACHINE_TYPE_NAME("raspi4b")
OBJECT_DECLARE_SIMPLE_TYPE(Raspi4bMachineState, RASPI4B_MACHINE)

struct Raspi4bMachineState {
    Raspi4BaseMachineState parent_obj;
    BCM2838State soc;
};

/*
 * Add second memory region if board RAM amount exceeds VC base address
 * (see https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf
 * 1.2 Address Map)
 */
static void raspi_add_memory_node(void *fdt, hwaddr mem_base, hwaddr mem_len)
{
    uint32_t acells, scells;
    char *nodename = g_strdup_printf("/memory@%" PRIx64, mem_base);

    acells = qemu_fdt_getprop_cell(fdt, "/", "#address-cells",
                                   NULL, &error_fatal);
    scells = qemu_fdt_getprop_cell(fdt, "/", "#size-cells",
                                   NULL, &error_fatal);
    /* validated by arm_load_dtb */
    g_assert(acells && scells);

    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "device_type", "memory");
    qemu_fdt_setprop_sized_cells(fdt, nodename, "reg",
                                        acells, mem_base,
                                        scells, mem_len);

    g_free(nodename);
}

static void raspi4_modify_dtb(const struct arm_boot_info *info, void *fdt)
{
    Raspi4BaseMachineState *s_base =
        container_of(info, Raspi4BaseMachineState, binfo);
    Raspi4bMachineState *s = RASPI4B_MACHINE(s_base);
    char **node_paths;
    uint64_t ram_size;

    /* Temporarily disable following devices until they are implemented */
    const char *nodes_to_remove[] = {
        "brcm,bcm2711-pcie",
        "brcm,bcm2711-rng200",
        "brcm,bcm2711-thermal",
        "brcm,bcm2711-l2-intc",
        "brcm,bcm2711-hdmi-i2c",
        "brcm,2711-v3d",
    };

    for (int i = 0; i < ARRAY_SIZE(nodes_to_remove); i++) {
        const char *dev_str = nodes_to_remove[i];
        int offset;

        while ((offset = fdt_node_offset_by_compatible(fdt, -1,
                                                       dev_str)) >= 0) {
            if (fdt_nop_node(fdt, offset) != 0) {
                break;
            }
            warn_report("bcm2711 dtb: %s has been disabled!", dev_str);
        }
    }

    node_paths = qemu_fdt_node_path(fdt, NULL, "brcm,bcm2711-genet-v5",
                                    &error_fatal);
    for (int i = 0; node_paths && node_paths[i]; i++) {
        qemu_fdt_setprop(fdt, node_paths[i], "local-mac-address",
                         s->soc.peripherals.genet.conf.macaddr.a, 6);
    }
    g_strfreev(node_paths);

    ram_size = raspi4_board_ram_size(info->board_id);

    if (info->ram_size > UPPER_RAM_BASE) {
        raspi_add_memory_node(fdt, UPPER_RAM_BASE, ram_size - UPPER_RAM_BASE);
    }
}

static void raspi4b_machine_init(MachineState *machine)
{
    Raspi4bMachineState *s = RASPI4B_MACHINE(machine);
    Raspi4BaseMachineState *s_base = RASPI4_BASE_MACHINE(machine);
    Raspi4BaseMachineClass *mc = RASPI4_BASE_MACHINE_GET_CLASS(machine);
    BCM2838State *soc = &s->soc;

    s_base->binfo.modify_dtb = raspi4_modify_dtb;
    s_base->binfo.board_id = mc->board_rev;

    object_initialize_child(OBJECT(machine), "soc", soc, TYPE_BCM2838);

    raspi4_common_machine_init(machine, soc);
}

static void raspi4b_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    Raspi4BaseMachineClass *rmc = RASPI4_BASE_MACHINE_CLASS(oc);

#if HOST_LONG_BITS == 32
    rmc->board_rev = 0xa03111; /* Revision 1.1, 1 Gb RAM */
#else
    rmc->board_rev = 0xb03115; /* Revision 1.5, 2 Gb RAM */
#endif
    raspi4_common_machine_class_init(mc, rmc->board_rev, "4B");
    mc->auto_create_sdcard = true;
    mc->init = raspi4b_machine_init;
}

static const TypeInfo raspi4b_machine_type = {
    .name           = TYPE_RASPI4B_MACHINE,
    .parent         = TYPE_RASPI4_BASE_MACHINE,
    .instance_size  = sizeof(Raspi4bMachineState),
    .class_init     = raspi4b_machine_class_init,
    .interfaces     = aarch64_machine_interfaces,
};

static void raspi4b_machine_register_type(void)
{
    type_register_static(&raspi4b_machine_type);
}

type_init(raspi4b_machine_register_type)
