/*
 * Raspberry Pi 4 machine integration tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/arm/raspberrypi-fw-defs.h"
#include "hw/misc/bcm2835_mbox_defs.h"
#include "libqtest-single.h"
#include "qobject/qdict.h"
#include "qobject/qlist.h"

#define RASPI4_MBOX_BASE       0xfe00b800
#define RASPI4_MBOX_READ       (RASPI4_MBOX_BASE + 0x80)
#define RASPI4_MBOX_WRITE      (RASPI4_MBOX_BASE + 0xa0)
#define RASPI4_PROPERTY_BUFFER 0x1000

static bool qom_bus_has_sd_card(const char *path)
{
    QDict *response;
    QList *properties;
    QListEntry *entry;
    bool found = false;

    response = qtest_qmp(global_qtest,
                         "{ 'execute': 'qom-list',"
                         "  'arguments': { 'path': %s } }", path);
    g_assert(qdict_haskey(response, "return"));
    properties = qdict_get_qlist(response, "return");

    QLIST_FOREACH_ENTRY(properties, entry) {
        QDict *property = qobject_to(QDict, qlist_entry_obj(entry));

        if (g_str_equal(qdict_get_str(property, "type"), "link<sd-card>")) {
            found = true;
            break;
        }
    }

    qobject_unref(response);
    return found;
}

static void test_sd_card_on_emmc2(void)
{
    g_assert_true(qom_bus_has_sd_card(
        "/machine/soc/peripherals/emmc2/sd-bus"));
    g_assert_false(qom_bus_has_sd_card(
        "/machine/soc/peripherals/sdhci/sd-bus"));
}

static void property_request(uint32_t tag, const uint32_t *payload,
                             size_t words, size_t response_bytes)
{
    uint32_t payload_bytes = words * sizeof(*payload);
    uint32_t total_bytes = 24 + payload_bytes;
    uint32_t response;
    size_t i;

    writel(RASPI4_PROPERTY_BUFFER, total_bytes);
    writel(RASPI4_PROPERTY_BUFFER + 4, 0);
    writel(RASPI4_PROPERTY_BUFFER + 8, tag);
    writel(RASPI4_PROPERTY_BUFFER + 12, payload_bytes);
    writel(RASPI4_PROPERTY_BUFFER + 16, 0);
    for (i = 0; i < words; i++) {
        writel(RASPI4_PROPERTY_BUFFER + 20 + i * 4, payload[i]);
    }
    writel(RASPI4_PROPERTY_BUFFER + 20 + payload_bytes, 0);

    writel(RASPI4_MBOX_WRITE,
           RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    response = readl(RASPI4_MBOX_READ);

    g_assert_cmphex(response, ==,
                    RASPI4_PROPERTY_BUFFER | MBOX_CHAN_PROPERTY);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 4), ==, 0x80000000);
    g_assert_cmphex(readl(RASPI4_PROPERTY_BUFFER + 16), ==,
                    0x80000000 | response_bytes);
}

static uint32_t property_payload(size_t word)
{
    return readl(RASPI4_PROPERTY_BUFFER + 20 + word * 4);
}

static void test_firmware_gpio(void)
{
    const uint32_t gpio = 132;
    const uint32_t get_config[] = { gpio, 0, 0, 0, 0 };
    const uint32_t set_config[] = { gpio, 1, 1, 1, 1, 1 };
    const uint32_t get_state[] = { gpio, 0 };
    const uint32_t clear_state[] = { gpio, 0 };

    property_request(RPI_FWREQ_GET_GPIO_CONFIG, get_config,
                     G_N_ELEMENTS(get_config), 20);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 0);
    g_assert_cmphex(property_payload(2), ==, 0);
    g_assert_cmphex(property_payload(3), ==, 0);
    g_assert_cmphex(property_payload(4), ==, 0);

    property_request(RPI_FWREQ_SET_GPIO_CONFIG, set_config,
                     G_N_ELEMENTS(set_config), 24);
    g_assert_cmphex(property_payload(0), ==, 0);

    property_request(RPI_FWREQ_GET_GPIO_CONFIG, get_config,
                     G_N_ELEMENTS(get_config), 20);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 1);
    g_assert_cmphex(property_payload(2), ==, 1);
    g_assert_cmphex(property_payload(3), ==, 1);
    g_assert_cmphex(property_payload(4), ==, 1);

    property_request(RPI_FWREQ_GET_GPIO_STATE, get_state,
                     G_N_ELEMENTS(get_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 1);

    property_request(RPI_FWREQ_SET_GPIO_STATE, clear_state,
                     G_N_ELEMENTS(clear_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);

    property_request(RPI_FWREQ_GET_GPIO_STATE, get_state,
                     G_N_ELEMENTS(get_state), 8);
    g_assert_cmphex(property_payload(0), ==, 0);
    g_assert_cmphex(property_payload(1), ==, 0);
}

int main(int argc, char **argv)
{
    int ret;

    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/raspi4b/sd/card_on_emmc2", test_sd_card_on_emmc2);
    qtest_add_func("/raspi4b/firmware_gpio", test_firmware_gpio);

    qtest_start("-machine raspi4b");
    ret = g_test_run();
    qtest_end();

    return ret;
}
