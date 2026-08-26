/*
 * BCM2835 framebuffer unit tests
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/display/bcm2835_fb.h"
#include "qemu/atomic.h"
#include "qemu/mprotect.h"

static void test_rgb24_bounded_read(void)
{
    if (g_test_subprocess()) {
        size_t page_size = qemu_real_host_page_size();
        uint8_t *mapping = qemu_anon_ram_alloc(page_size * 2, NULL,
                                               false, false);
        volatile uint8_t *source;

        g_assert_nonnull(mapping);
        g_assert_cmpint(qemu_mprotect_none(mapping + page_size,
                                           page_size), ==, 0);

        source = mapping + page_size - 3;
        source[0] = 0xaa;
        source[1] = 0xbb;
        source[2] = 0xcc;
        barrier();
        g_assert_cmphex(bcm2835_fb_read_rgb24((const uint8_t *)source), ==,
                        0x00ccbbaa);
        qemu_anon_ram_free(mapping, page_size * 2);
        return;
    }

    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/bcm2835-fb/rgb24/bounded-read",
                    test_rgb24_bounded_read);
    return g_test_run();
}
