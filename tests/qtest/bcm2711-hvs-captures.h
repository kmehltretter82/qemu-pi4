/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Pi 400 writeback captures from harnesses/hvs-diff-20261009. */
/* Expected pixels are hardware data; no reference-model output is used. */
/* Regenerate with harnesses/hvs-diff-20261009/gen-fixtures.py. */
typedef struct HVSReferenceRegion {
    uint32_t base;
    const char *data;
} HVSReferenceRegion;

typedef struct HVSReferenceSegment {
    uint32_t offset, length;
    const uint32_t *words;
} HVSReferenceSegment;

typedef struct HVSReferencePixel {
    uint32_t x, y, rgb;
} HVSReferencePixel;

typedef struct HVSReference {
    const char *name;
    uint32_t width, height, channel, start;
    uint32_t regions_count, segments_count, pixels_count;
    const HVSReferenceRegion *regions;
    const HVSReferenceSegment *segments;
    const HVSReferencePixel *pixels;
} HVSReference;

/*
 * cap1/fmt_AB24_unity.json
 * SHA256 58877e2330e5718cb79c245f111cb330e78b26e946977a8f146c73255712b597
 */
static const HVSReferenceRegion hvs_reference_0_regions[] = {
    { 0x2e87f000,
      "5zXnY+43pE/nEyZ0YYIQBl56jpHzyBWkX4OPtTDXtX3k+57/m5YJNUhZRf8uI88+FUDogMp0"
      "YWXn9QzfUCWICwePecAgbEjTHmgYURIIO2hhI+Rve4k3bg/SvEXt5Cd3p39lseEeZlxkF/Oh"
      "d1qDoZaQW2P/vAVvAkPxYysvEjHquVtEjkZzetDmi0MqqbEtgkdRhKERycZ1CXIxk/Ms3g+3"
      "0tcjn8ZANxFCZs0K5s43JugJlPZkMcUiA2Yin8C3CKOvuABF7TttJaevayZoYBrnMiTwvD/2"
      "wBZtNstCfAzWrscTJbQs9mUrnkuK8qRhrCeGXizVCDqfD7Joqv8qWAcHE1DRw+GQPMKvMEQG"
      "1x9+JIxEMyn57AUuDiceO28x7oMgp/nV28haqRHvYMZY2uXqq7Vh4UfcQ+zwa9YqTL/ED+EG"
      "O578FMosLGbX9nFcbAvg0AB1lOiKOVS4m/6DvQr4n89rgsNjXwVqVMm8mzAzmkgHFUmLpUpn"
      "/ittVmbP4q/SYYTiOq4E/wKEqHvBj5enDjvFzxZRJYDNzyZlPkQu3PuoamYvvXxtVd0HxCFc"
      "vHDqz76i02noa85eQUWJJPdEfq5F/vKaxbP88F0UQTNOIK0Fn43etnRW8xl/OaJ2YeY3i1Hx"
      "YplkjNasxm0PbvfThc8ow0Jd1/wLJxihEsc="
    },
};
static const uint32_t hvs_reference_0_words_72[] = {
    0x4800f807, 0x00020004, 0x2000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_0_segments[] = {
    { 72, 9, hvs_reference_0_words_72 },
};
static const HVSReferencePixel hvs_reference_0_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_AB30_unity.json
 * SHA256 79993a984b4b6d8df23d62543cf3acda4a0d4844035396af0cf1cd57d67f0ea1
 */
static const HVSReferenceRegion hvs_reference_1_regions[] = {
    { 0x2e87f000,
      "n1PzebtzI2mfM4FJhSkIBHmlp6PPj0yFfTnoo8B8bW2Tv6/nbmpJAiGVVdG4MPIzVAQ0uitH"
      "V1ifXw/DQVEiIhz4WN6AxBbSeIQGRkiAwE6FMTJ57ZnITTwsLW+3T85JnvZXmYfjkVmRcfG8"
      "3aXloFoK2Vb/y0tBCDR0fKzwggSrm9tWOmbUXENv7mKomGosCnZUlIYScfLVkZAcTj4Pyzx4"
      "u/SM+Llx3BCRUDejsPncYDI6Um4fGRcjwkCI+DmwIDjqqwBUdDu1UeKprWESWmh8jgzDy8vP"
      "A2NRGy8nFB9b6/oxlEgLy5WxomcqLi9psnKiYbBcDQJ+8qBsqv6PShxwwERHP3y48CzsKxFh"
      "8DX5QSJjzJBy/hTgggN4sNMbuzsIiOdf/fZpmUrEgW0c1pev7qqFHd7RDc0+fFujEpMT83A4"
      "7Og5PyvDAktfb19csbEw+ABUJ+UqkhOVbu7voCiM7+etKfhwfVGQWifL6ybMqBkSVJTkoil1"
      "tj+1ZZXZi/u6dBIujo4Q/I+AorZ3sF56igMX/4xFlAh485hUhk+4zP2+qWXGi/HVVtUcTExI"
      "8ga3+voq+nSjt7ZzBVVkIt9HlJ8V7b+8Fzs7/3VBURA5AWIrftq4t9Fl9Tz9kaNohW3OjUUd"
      "n5iRybi1G9fGQ98/beGgPJxQX8/PAmAYisQ="
    },
};
static const uint32_t hvs_reference_1_words_72[] = {
    0x4800f810, 0x00020004, 0x2000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_1_segments[] = {
    { 72, 9, hvs_reference_1_words_72 },
};
static const HVSReferencePixel hvs_reference_1_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_AR15_unity.json
 * SHA256 1847689989a1988fc1975f9fe8496a7c121bfaca5e94a006edff0750fdf120b2
 */
static const HVSReferenceRegion hvs_reference_2_regions[] = {
    { 0x2e87e000,
      "3HDUdERwAjLxrSL7Ea5WG/PzQU5opZkUHYnMZcHzkSgvgqmRow0nCJwwJj5XB4R37NFscF6w"
      "cLlLSuB+HgGiFOt2DkWRa7YWCsFZ0C44xcvahngSSBg85J0YzEuAYHiSlYYdAZS0jTSGD+f6"
      "TWAPZbhqxZKzMNRHkFRBFzZM5VcCABzrFR8aIJE8v5ihAO0MBPZb/6KuC7O184izviOp6Dxg"
      "fx6lZM5rPLTSgerE8M/zhxg2DSzzZmkaMYmfJUy1unKHw+CD+NWBSiJjGZKHEX+XhbWqvQQD"
      "3d2aXrl1ESEP+f6j3+JILJUkO85eOfQ8hrPMqzqyoWFQ+wgX4WuCjg=="
    },
};
static const uint32_t hvs_reference_2_words_99[] = {
    0x4800d803, 0x00020004, 0x2000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e0e0, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_2_segments[] = {
    { 99, 9, hvs_reference_2_words_99 },
};
static const HVSReferencePixel hvs_reference_2_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe731e7 },
    { 4, 4, 0xefbd5a },
    { 4, 6, 0x000010 },
    { 4, 7, 0xc608e7 },
    { 4, 9, 0xf7427b },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0042ef },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x844252 },
    { 8, 8, 0x94a508 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf7ce10 },
    { 9, 4, 0xa510ce },
    { 9, 7, 0x007394 },
    { 9, 9, 0x9c8cde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4a5a42 },
    { 14, 4, 0x311042 },
    { 14, 7, 0x5a006b },
    { 14, 9, 0x638cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3121ef },
    { 16, 8, 0xbd73ef },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f763 },
    { 17, 7, 0x104a8c },
    { 19, 2, 0x52218c },
    { 19, 4, 0x219cc6 },
    { 19, 7, 0x6b5263 },
    { 19, 9, 0x18a510 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_AR24_unity.json
 * SHA256 b256a14226726c6ddf197ff676dd462c4c8862a8cd41adab96418ef7a7aa8585
 */
static const HVSReferenceRegion hvs_reference_3_regions[] = {
    { 0x2e87f000,
      "5zXnY6Q37k8mE+d0EIJhBo56XpEVyPOkj4NftbXXMH2e++T/CZabNUVZSP/PIy4+6EAVgGF0"
      "ymUM9effiCVQC3mPB8BIbCDTGGgeUTsIEmjkI2FvN4l7brzSD0Un5O13ZX+nsWYe4VzzF2Sh"
      "g1p3oVuQlmMFvP9v8UMCYxIvKzFbuepEc0aOeovm0EOxqSotUUeChMkRocZyCXUxLPOT3tK3"
      "D9fGnyNAQhE3ZuYKzc7oJjcJZPaUMQMixWbAnyK3r6MIuO1FADunJW2vaCZrYDLnGiQ/vPD2"
      "bRbANnxCywzHrtYTLLQl9p4rZUuk8ophhiesXgjVLDqyD59oKv+qWBMHB1Dhw9GQr8I8MNcG"
      "RB+MJH5E+Skz7A4uBSdvOx4xIIPup9vV+cgRqVrvWMZg2qvq5bVH4WHc8OxDa0wq1r/hD8QG"
      "/J47FCwsymZx9tdc4Ats0JR1AOhUOYq4g/6bvZ/4Cs/DgmtjagVfVJu8yTBImjMHi0kVpf5n"
      "SitmVm3P0q/iYTrihK4C/wSEwXuojw6nlzsWz8VRzYAlzz5lJkT73C6oL2ZqvVVtfN0hxAdc"
      "6nC8z9OivmnOa+heiUVBJH5E967y/kWa/LPF8EEUXTOtIE4F3o2ftvNWdBmiOX92N+Zhi2Lx"
      "UZnWjGSsD23GboXT989CwyhdC/zXJxKhGMc="
    },
};
static const uint32_t hvs_reference_3_words_72[] = {
    0x4800d807, 0x00020004, 0x2000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_3_segments[] = {
    { 72, 9, hvs_reference_3_words_72 },
};
static const HVSReferencePixel hvs_reference_3_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_AR30_unity.json
 * SHA256 6e8d1efe1d2e98d2be438209451daaca89b39ce5bd4098c24e5ba85f96f246a7
 */
static const HVSReferenceRegion hvs_reference_4_regions[] = {
    { 0x2e87f000,
      "n1PzeZJys3uYMPF5QChYGDqml5dUjPy8PjrYl9Z+DUx6vj/5JGjpJhWVFdI/M4ILowdEhYVF"
      "t3IwXP/5IlISFOX5yMEhxQbIYISGR+yAgESTM1JY3JjYXvIuzUOcTH57lfXnqZnhcXjPcxGZ"
      "DqbVnW0JqWUUyPt/xzeEQEjwwgptmbt6zWWkYy5uPnTGmooKRXWkoCcTYejJkVAdsDzv5Et7"
      "y8Mb+8lICRHBTZujcPOjY8INkW0vJQwgcnED+4mIvjoKgrdXBACeUlKboWHSWsh8jgb8yDv8"
      "tWExMPEl9DIf67o1sEhLyXqyUlmSLq9iGnIiayBcDQvK8uBnqPyvakxwwEGHP3y0vi4MD19j"
      "EBEyQpJf55PCzDjgQgG9sYMHgDi4u29fff5EmJrWYW0c2K6ufrkdHV7Yw8/eUDGhsrWH8zAx"
      "8+vJDrDAsnLFbf91g7MQ21JWB8BRkaOiDu7vpn6Oj8IPK9haqVHQV27KezIhqckMLpZEhft3"
      "lhKZZVXbS/u6eOgsLqEI/A+BB7cnqjh46iVY/HxxNwtIyfhUhknvz42LvGSWmlXVFt+ETMxB"
      "qwcn708rqm87tzZ6JlZUEPlF9L3L71+R8zt78QVBURe2ApITe9vop89nFR2KktNf3GxemIkd"
      "X5RbyxiZPNS2cRY+/f0JPQxKLMz/NUgYCsY="
    },
};
static const uint32_t hvs_reference_4_words_72[] = {
    0x4800d810, 0x00020004, 0x2000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_4_segments[] = {
    { 72, 9, hvs_reference_4_words_72 },
};
static const HVSReferencePixel hvs_reference_4_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_BG16_unity.json
 * SHA256 9678a337b5d184c6413f4c0d8342494cd81317abd476f333d42d9f4443e85424
 */
static const HVSReferenceRegion hvs_reference_5_regions[] = {
    { 0x2e87f000,
      "vOG9oZwgDBTLi14WC4ymttyfswzJQgXJAuq5Y7wPKolgfGRLQxtCOAzhTzSBvj0n9GP8YKzw"
      "zoKSXP8FAPJlEd1dMXI6j0W1MFKUyE5wki+h1eTEhkBZ4CbpsmcYAeTEAa0g6i2hLWkjN/49"
      "uGgZenrFpC1MmZGnNYGlDnOw9S8gEBrmB64o0C+JRvlgCcNpHSS/3ksVLF5crwxHaPdaSXjg"
      "5/x5Kbp3TeCgk9FR84fBnw3EK2j5ncZMQoop+61ifNUQP+AH1cMyDXgWBMwkO+X+LStvUyAm"
      "l+sX1V3LKIo+euj3mP2rQAmpc9yu8s+hLDeKZ2zUeAuehgVG+g8DFQ=="
    },
};
static const uint32_t hvs_reference_5_words_72[] = {
    0x4800f804, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f0e0, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_5_segments[] = {
    { 72, 9, hvs_reference_5_words_72 },
};
static const HVSReferencePixel hvs_reference_5_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe734e7 },
    { 4, 4, 0xefba5a },
    { 4, 6, 0x000410 },
    { 4, 7, 0xc60ce7 },
    { 4, 9, 0xf7457b },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ef },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x844552 },
    { 8, 8, 0x94a608 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf7cb10 },
    { 9, 4, 0xa510ce },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9c8ede },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4a5942 },
    { 14, 4, 0x311042 },
    { 14, 7, 0x5a046b },
    { 14, 9, 0x638ed6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3124ef },
    { 16, 8, 0xbd71ef },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f763 },
    { 17, 7, 0x10498c },
    { 19, 2, 0x52248c },
    { 19, 4, 0x219ec6 },
    { 19, 7, 0x6b5563 },
    { 19, 9, 0x18a210 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_BG24_unity.json
 * SHA256 7e58093ae6704147b9366f83453b5a32b3883f567fac02666d4ea50319b29fbe
 */
static const HVSReferenceRegion hvs_reference_6_regions[] = {
    { 0x2e87f000,
      "5zXn7jek5xMmYYIQXnqO88gVX4OPMNe15Puem5YJSFlFLiPPFUDoynRh5/UMUCWIB495IGxI"
      "HmgYEgg7YSPke4k3D9K87eQnp39l4R5mZBfzd1qDlpBb/7wFAkPxKy8S6rlbjkZz0OaLKqmx"
      "gkdRoRHJdQlyk/MsD7fSI5/GNxFCzQrmNybolPZkxSIDIp/ACKOvAEXtbSWnayZoGucy8Lw/"
      "wBZty0J81q7HJbQsZSueivKkrCeGLNUInw+yqv8qBwcT0cPhPMKvRAbXfiSMMyn5BS4OHjtv"
      "7oMg+dXbWqkRYMZY5eqrYeFHQ+zw1ipMxA/hO578yiws1/ZxbAvgAHWUijlUm/6DCvifa4LD"
      "XwVqybybM5pIFUmLSmf+bVZm4q/ShOI6BP8CqHvBl6cOxc8WJYDNJmU+Ltz7amYvfG1VB8Qh"
      "vHDqvqLT6GvOQUWJ90R+Rf7yxbP8XRRBTiCtn43edFbzfzmiYeY3UfFiZIzWxm0P99OFKMNC"
      "1/wLGKES"
    },
};
static const uint32_t hvs_reference_6_words_72[] = {
    0x4800f805, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f150, 0x00000030, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_6_segments[] = {
    { 72, 9, hvs_reference_6_words_72 },
};
static const HVSReferencePixel hvs_reference_6_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_BGR8_unity.json
 * SHA256 f6abc96e75411a683cf39aef7af7173888372d0641a4775de5c5de7e74d2e982
 */
static const HVSReferenceRegion hvs_reference_7_regions[] = {
    { 0x2e87f000,
      "z48HI5o3orG/JFLJ0F4/imBZGADLI7A/XUfDk2Qv0AlvVL6pVMVDPOjhQcbJfA7hqNCLSzgv"
      "RlbuKYu8jTGEPQD2scKLyQhIJ/cqc797+k7G4Q5+w5hMvLjjQq5hkNpT7zw43Sw24RnxG1sw"
      "3e3fklf67kKK5NOLO3rjHrdxPig="
    },
};
static const uint32_t hvs_reference_7_words_72[] = {
    0x4800f800, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f070, 0x00000010, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_7_segments[] = {
    { 72, 9, hvs_reference_7_words_72 },
};
static const HVSReferencePixel hvs_reference_7_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xff24ff },
    { 4, 4, 0xffb655 },
    { 4, 6, 0x000000 },
    { 4, 7, 0xdb00ff },
    { 4, 9, 0xff4955 },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0049ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x924955 },
    { 8, 8, 0x92b600 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xffdb00 },
    { 9, 4, 0xb600ff },
    { 9, 7, 0x006daa },
    { 9, 9, 0x9292ff },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x494955 },
    { 14, 4, 0x240055 },
    { 14, 7, 0x490055 },
    { 14, 9, 0x6d92ff },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x2424ff },
    { 16, 8, 0xb66dff },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x92ff55 },
    { 17, 7, 0x0049aa },
    { 19, 2, 0x4924aa },
    { 19, 4, 0x2492ff },
    { 19, 7, 0x6d4955 },
    { 19, 9, 0x00b600 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_NV12_unity.json
 * SHA256 6578c5fd3809c08ba70f4fa42e379e861e6ecb0a12f47c393af398d073c8c327
 */
static const HVSReferenceRegion hvs_reference_8_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ1F2qAWU8YaaFc0KnDNW9Yn8Iz0EsTdD40q/2KwhRq267m8"
      "xWqerTqQgel486v2unq+VVvtDrrDXdxgEHANA3GJ"
    },
};
static const uint32_t hvs_reference_8_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_8_words_107[] = {
    0x56009809, 0x00020004, 0x4000fff0, 0x00080010, 0x00070003, 0xee87e000,
    0xee87e080, 0xee87e070, 0xee87e0b0, 0x00000010, 0x00000010, 0x00f00000,
    0xf27784a8, 0x00072e1d, 0x00000000, 0x40800070, 0x40800070, 0x4000c000,
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_8_segments[] = {
    { 32, 11, hvs_reference_8_words_32 },
    { 107, 23, hvs_reference_8_words_107 },
};
static const HVSReferencePixel hvs_reference_8_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xffe385 },
    { 4, 4, 0x701de9 },
    { 4, 6, 0xffc0ff },
    { 4, 7, 0xffabff },
    { 4, 9, 0x6bccff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xc0c2ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb12ad1 },
    { 8, 8, 0x760000 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x580000 },
    { 9, 4, 0x9b001a },
    { 9, 7, 0xfffbdd },
    { 9, 9, 0xcf9a12 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x429400 },
    { 14, 4, 0xffc36c },
    { 14, 7, 0xffa392 },
    { 14, 9, 0x468800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x00422e },
    { 16, 8, 0x006700 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x0a72a5 },
    { 17, 7, 0xaf5b86 },
    { 19, 2, 0x006cff },
    { 19, 4, 0xff8bff },
    { 19, 7, 0x8188de },
    { 19, 9, 0x190000 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_NV16_unity.json
 * SHA256 091347ae56f98ff62781fe7e7e8d823442d3f1ff384f03d44526058470607ce8
 */
static const HVSReferenceRegion hvs_reference_9_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ3aQxaIxtdoRjR7cGZb3ifxjB4ShN0UjdX/3LAvGs/rZbyx"
      "arytxZBS6QjzT/YPehNVJe3UumZdwmAycLQDYImfropb+3s/BGwUyX59OyS40E+2DKWlKo+q"
      "PEh1LvSf+p+1XIbf47tTWuUPlWsX+Y7jqPjjW20dka6C6Q56AacNMQ=="
    },
};
static const uint32_t hvs_reference_9_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_9_words_104[] = {
    0x5600980b, 0x00020004, 0x4000fff0, 0x00080010, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e070, 0xee87e0f0, 0x00000010, 0x00000010, 0x00f00000,
    0xf27784a8, 0x00072e1d, 0x00000000, 0x40800070, 0x41000070, 0x0001c000,
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_9_segments[] = {
    { 32, 11, hvs_reference_9_words_32 },
    { 104, 23, hvs_reference_9_words_104 },
};
static const HVSReferencePixel hvs_reference_9_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x9affff },
    { 4, 4, 0x6726ae },
    { 4, 6, 0xd7ebff },
    { 4, 7, 0xffb688 },
    { 4, 9, 0xff86ff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xb0dbe8 },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x9936a0 },
    { 8, 8, 0x55009a },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x540067 },
    { 9, 4, 0x420a7b },
    { 9, 7, 0xabffff },
    { 9, 9, 0x51b19e },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4c8639 },
    { 14, 4, 0x65dbff },
    { 14, 7, 0x57ec94 },
    { 14, 9, 0x757800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x0033d2 },
    { 16, 8, 0xc51200 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x007fdf },
    { 17, 7, 0x9251ff },
    { 19, 2, 0xff2400 },
    { 19, 4, 0x14f2e2 },
    { 19, 7, 0xd75fff },
    { 19, 9, 0x002c00 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_NV21_unity.json
 * SHA256 71cd1d3bc0f90ea53f6626bd30addc5de82f1dd3c13a0568289d9ff93d586611
 */
static const HVSReferenceRegion hvs_reference_10_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ3aRRagxlNoGjRXcCpbzSfWjPAS9N3EjQ//KrBiGoXrtry5"
      "asWtnpA66YHzeParerpVvu1bug5dw2DccBADDYlx"
    },
};
static const uint32_t hvs_reference_10_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_10_words_104[] = {
    0x5600b809, 0x00020004, 0x4000fff0, 0x00080010, 0x00070003, 0xee87e000,
    0xee87e080, 0xee87e070, 0xee87e0b0, 0x00000010, 0x00000010, 0x00f00000,
    0xf27784a8, 0x00072e1d, 0x00000000, 0x40800070, 0x40800070, 0x4000c000,
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_10_segments[] = {
    { 32, 11, hvs_reference_10_words_32 },
    { 104, 23, hvs_reference_10_words_104 },
};
static const HVSReferencePixel hvs_reference_10_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xffe385 },
    { 4, 4, 0x701de9 },
    { 4, 6, 0xffc0ff },
    { 4, 7, 0xffabff },
    { 4, 9, 0x6bccff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xc0c2ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb12ad1 },
    { 8, 8, 0x760000 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x580000 },
    { 9, 4, 0x9b001a },
    { 9, 7, 0xfffbdd },
    { 9, 9, 0xcf9a12 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x429400 },
    { 14, 4, 0xffc36c },
    { 14, 7, 0xffa392 },
    { 14, 9, 0x468800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x00422e },
    { 16, 8, 0x006700 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x0a72a5 },
    { 17, 7, 0xaf5b86 },
    { 19, 2, 0x006cff },
    { 19, 4, 0xff8bff },
    { 19, 7, 0x8188de },
    { 19, 9, 0x190000 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_NV61_unity.json
 * SHA256 e25c4c90b02f3177f50f849f656ad75d3d984123ed5c1b7b2f4fff3e1ff2d91c
 */
static const HVSReferenceRegion hvs_reference_11_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ1D2ogW18ZGaHs0ZnDeW/EnHoyEEhTd1Y3c/y+wzxpl67G8"
      "vGrFrVKQCOlP8w/2E3olVdTtZrrCXTJgtHBgA5+Jiq77Wz97bATJFH1+JDvQuLZPpQwqpaqP"
      "SDwudZ/0n/pctd+Gu+NaUw/la5X5F+OO+Khb4x1trpHpgnoOpwExDQ=="
    },
};
static const uint32_t hvs_reference_11_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_11_words_104[] = {
    0x5600b80b, 0x00020004, 0x4000fff0, 0x00080010, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e070, 0xee87e0f0, 0x00000010, 0x00000010, 0x00f00000,
    0xf27784a8, 0x00072e1d, 0x00000000, 0x40800070, 0x41000070, 0x0001c000,
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_11_segments[] = {
    { 32, 11, hvs_reference_11_words_32 },
    { 104, 23, hvs_reference_11_words_104 },
};
static const HVSReferencePixel hvs_reference_11_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x9affff },
    { 4, 4, 0x6726ae },
    { 4, 6, 0xd7ebff },
    { 4, 7, 0xffb688 },
    { 4, 9, 0xff86ff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xb0dbe8 },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x9936a0 },
    { 8, 8, 0x55009a },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x540067 },
    { 9, 4, 0x420a7b },
    { 9, 7, 0xabffff },
    { 9, 9, 0x51b19e },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4c8639 },
    { 14, 4, 0x65dbff },
    { 14, 7, 0x57ec94 },
    { 14, 9, 0x757800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x0033d2 },
    { 16, 8, 0xc51200 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x007fdf },
    { 17, 7, 0x9251ff },
    { 19, 2, 0xff2400 },
    { 19, 4, 0x14f2e2 },
    { 19, 7, 0xd75fff },
    { 19, 9, 0x002c00 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_RG16_unity.json
 * SHA256 faa898d8d90f2ecdf721b03fe84f2f2401943fc8ffc85851db6b194e34faeede
 */
static const HVSReferenceRegion hvs_reference_12_regions[] = {
    { 0x2e87e000,
      "vOG06YTgAmTRW0L2EVy2NtPnoZzIShkpHRKsy6HnMVFvBGkjQxtHEBxhRnyXDiTv7KPs4L5g"
      "0HKLlOD9HgJiKcvtLoox11YtKoKZoE5whZe6DfgkiDBcyD0xrJcAwfgkFQ09AjRpLWkmH+f1"
      "rcAPynjVpSVTYZSPMKmhLnaY5a8iABzWFT46QDF5XzFhAc0ZBOy7/kJdK2ZV5whnfkdJ0XzA"
      "/zxlya7XXGiyA8qJ8J/TDxhsLVjzzck0URI/S6xqeuUHh+AH2KshlWLGGSQnI/8uJWtqeyQG"
      "nbsavVnrMUIv8v5Hn8WoWBVJe5y+ctR5JmeMV3pkYcOQ9ggu4dcCHQ=="
    },
};
static const uint32_t hvs_reference_12_words_99[] = {
    0x4800d804, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e0e0, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_12_segments[] = {
    { 99, 9, hvs_reference_12_words_99 },
};
static const HVSReferencePixel hvs_reference_12_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe734e7 },
    { 4, 4, 0xefba5a },
    { 4, 6, 0x000410 },
    { 4, 7, 0xc60ce7 },
    { 4, 9, 0xf7457b },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ef },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x844552 },
    { 8, 8, 0x94a608 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf7cb10 },
    { 9, 4, 0xa510ce },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9c8ede },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4a5942 },
    { 14, 4, 0x311042 },
    { 14, 7, 0x5a046b },
    { 14, 9, 0x638ed6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3124ef },
    { 16, 8, 0xbd71ef },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f763 },
    { 17, 7, 0x10498c },
    { 19, 2, 0x52248c },
    { 19, 4, 0x219ec6 },
    { 19, 7, 0x6b5563 },
    { 19, 9, 0x18a210 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_RG24_unity.json
 * SHA256 a6ba7724dff03b38dfcefdb43b43f5d9a829a36b468e049614686ab980ccc457
 */
static const HVSReferenceRegion hvs_reference_13_regions[] = {
    { 0x2e87e000,
      "5zXnpDfuJhPnEIJhjnpeFcjzj4NftdcwnvvkCZabRVlIzyMu6EAVYXTKDPXniCVQeY8HSGwg"
      "GGgeOwgS5CNhN4l7vNIPJ+TtZX+nZh7h8xdkg1p3W5CWBbz/8UMCEi8rW7nqc0aOi+bQsakq"
      "UUeCyRGhcgl1LPOT0rcPxp8jQhE35grN6CY3ZPaUAyLFwJ8ir6MI7UUApyVtaCZrMucaP7zw"
      "bRbAfELLx67WLLQlnitlpPKKhiesCNUssg+fKv+qEwcH4cPRr8I81wZEjCR++SkzDi4Fbzse"
      "IIPu29X5EalaWMZgq+rlR+Fh8OxDTCrW4Q/E/J47LCzKcfbX4AtslHUAVDmKg/6bn/gKw4Jr"
      "agVfm7zJSJozi0kV/mdKZlZt0q/iOuKEAv8EwXuoDqeXFs/FzYAlPmUm+9wuL2ZqVW18IcQH"
      "6nC806K+zmvoiUVBfkT38v5F/LPFQRRdrSBO3o2f81Z0ojl/N+ZhYvFR1oxkD23GhdP3QsMo"
      "C/zXEqEY"
    },
};
static const uint32_t hvs_reference_13_words_99[] = {
    0x4800d805, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e150, 0x00000030, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_13_segments[] = {
    { 99, 9, hvs_reference_13_words_99 },
};
static const HVSReferencePixel hvs_reference_13_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_RGB8_unity.json
 * SHA256 cae4982333fc43a1f9e51e04fd07e3acb7239e4568b66a7520b09574ed6b8662
 */
static const HVSReferenceRegion hvs_reference_14_regions[] = {
    { 0x2e87e000,
      "5+bgcE74Ujr+kEknC838RhEtDABncBr8reFjapH0CyT1id42iaNhnBczIcMnncQzFgtmZRz0"
      "wcnXNGaepjiCvADbOkNmJwQF8PtUef59X8XDM8TdYw6Fnh5zQdYxCk9p95wcr5TYMyw7bG0Y"
      "r7fvSulf10FGk2tmfF1zzPo53BQ="
    },
};
static const uint32_t hvs_reference_14_words_99[] = {
    0x4800d800, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e070, 0x00000010, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_14_segments[] = {
    { 99, 9, hvs_reference_14_words_99 },
};
static const HVSReferencePixel hvs_reference_14_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xff24ff },
    { 4, 4, 0xffb655 },
    { 4, 6, 0x000000 },
    { 4, 7, 0xdb00ff },
    { 4, 9, 0xff4955 },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0049ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x924955 },
    { 8, 8, 0x92b600 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xffdb00 },
    { 9, 4, 0xb600ff },
    { 9, 7, 0x006daa },
    { 9, 9, 0x9292ff },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x494955 },
    { 14, 4, 0x240055 },
    { 14, 7, 0x490055 },
    { 14, 9, 0x6d92ff },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x2424ff },
    { 16, 8, 0xb66dff },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x92ff55 },
    { 17, 7, 0x0049aa },
    { 19, 2, 0x4924aa },
    { 19, 4, 0x2492ff },
    { 19, 7, 0x6d4955 },
    { 19, 9, 0x00b600 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_XB24_unity.json
 * SHA256 e964aacd7ea68e391588d15337e31e0c321e2aa5ef2e813dc22bd0ad8bc84a14
 */
static const HVSReferenceRegion hvs_reference_15_regions[] = {
    { 0x2e87e000,
      "5zXnY+43pE/nEyZ0YYIQBl56jpHzyBWkX4OPtTDXtX3k+57/m5YJNUhZRf8uI88+FUDogMp0"
      "YWXn9QzfUCWICwePecAgbEjTHmgYURIIO2hhI+Rve4k3bg/SvEXt5Cd3p39lseEeZlxkF/Oh"
      "d1qDoZaQW2P/vAVvAkPxYysvEjHquVtEjkZzetDmi0MqqbEtgkdRhKERycZ1CXIxk/Ms3g+3"
      "0tcjn8ZANxFCZs0K5s43JugJlPZkMcUiA2Yin8C3CKOvuABF7TttJaevayZoYBrnMiTwvD/2"
      "wBZtNstCfAzWrscTJbQs9mUrnkuK8qRhrCeGXizVCDqfD7Joqv8qWAcHE1DRw+GQPMKvMEQG"
      "1x9+JIxEMyn57AUuDiceO28x7oMgp/nV28haqRHvYMZY2uXqq7Vh4UfcQ+zwa9YqTL/ED+EG"
      "O578FMosLGbX9nFcbAvg0AB1lOiKOVS4m/6DvQr4n89rgsNjXwVqVMm8mzAzmkgHFUmLpUpn"
      "/ittVmbP4q/SYYTiOq4E/wKEqHvBj5enDjvFzxZRJYDNzyZlPkQu3PuoamYvvXxtVd0HxCFc"
      "vHDqz76i02noa85eQUWJJPdEfq5F/vKaxbP88F0UQTNOIK0Fn43etnRW8xl/OaJ2YeY3i1Hx"
      "YplkjNasxm0PbvfThc8ow0Jd1/wLJxihEsc="
    },
};
static const uint32_t hvs_reference_15_words_99[] = {
    0x4800f807, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_15_segments[] = {
    { 99, 9, hvs_reference_15_words_99 },
};
static const HVSReferencePixel hvs_reference_15_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_XB30_unity.json
 * SHA256 d58fbd37acf6f705f204ba7414e4f2d56afe16991ac57b62887796465c7777f5
 */
static const HVSReferenceRegion hvs_reference_16_regions[] = {
    { 0x2e87e000,
      "n1PzebtzI2mfM4FJhSkIBHmlp6PPj0yFfTnoo8B8bW2Tv6/nbmpJAiGVVdG4MPIzVAQ0uitH"
      "V1ifXw/DQVEiIhz4WN6AxBbSeIQGRkiAwE6FMTJ57ZnITTwsLW+3T85JnvZXmYfjkVmRcfG8"
      "3aXloFoK2Vb/y0tBCDR0fKzwggSrm9tWOmbUXENv7mKomGosCnZUlIYScfLVkZAcTj4Pyzx4"
      "u/SM+Llx3BCRUDejsPncYDI6Um4fGRcjwkCI+DmwIDjqqwBUdDu1UeKprWESWmh8jgzDy8vP"
      "A2NRGy8nFB9b6/oxlEgLy5WxomcqLi9psnKiYbBcDQJ+8qBsqv6PShxwwERHP3y48CzsKxFh"
      "8DX5QSJjzJBy/hTgggN4sNMbuzsIiOdf/fZpmUrEgW0c1pev7qqFHd7RDc0+fFujEpMT83A4"
      "7Og5PyvDAktfb19csbEw+ABUJ+UqkhOVbu7voCiM7+etKfhwfVGQWifL6ybMqBkSVJTkoil1"
      "tj+1ZZXZi/u6dBIujo4Q/I+AorZ3sF56igMX/4xFlAh485hUhk+4zP2+qWXGi/HVVtUcTExI"
      "8ga3+voq+nSjt7ZzBVVkIt9HlJ8V7b+8Fzs7/3VBURA5AWIrftq4t9Fl9Tz9kaNohW3OjUUd"
      "n5iRybi1G9fGQ98/beGgPJxQX8/PAmAYisQ="
    },
};
static const uint32_t hvs_reference_16_words_99[] = {
    0x4800f810, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_16_segments[] = {
    { 99, 9, hvs_reference_16_words_99 },
};
static const HVSReferencePixel hvs_reference_16_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_XR15_unity.json
 * SHA256 75d9c01c90e0caf063a09da403796f3ce330b9cd30889117b21c7caa7a2167f2
 */
static const HVSReferenceRegion hvs_reference_17_regions[] = {
    { 0x2e87f000,
      "3HDUdERwAjLxrSL7Ea5WG/PzQU5opZkUHYnMZcHzkSgvgqmRow0nCJwwJj5XB4R37NFscF6w"
      "cLlLSuB+HgGiFOt2DkWRa7YWCsFZ0C44xcvahngSSBg85J0YzEuAYHiSlYYdAZS0jTSGD+f6"
      "TWAPZbhqxZKzMNRHkFRBFzZM5VcCABzrFR8aIJE8v5ihAO0MBPZb/6KuC7O184izviOp6Dxg"
      "fx6lZM5rPLTSgerE8M/zhxg2DSzzZmkaMYmfJUy1unKHw+CD+NWBSiJjGZKHEX+XhbWqvQQD"
      "3d2aXrl1ESEP+f6j3+JILJUkO85eOfQ8hrPMqzqyoWFQ+wgX4WuCjg=="
    },
};
static const uint32_t hvs_reference_17_words_72[] = {
    0x4800d803, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87f000,
    0xee87f0e0, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_17_segments[] = {
    { 72, 9, hvs_reference_17_words_72 },
};
static const HVSReferencePixel hvs_reference_17_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe731e7 },
    { 4, 4, 0xefbd5a },
    { 4, 6, 0x000010 },
    { 4, 7, 0xc608e7 },
    { 4, 9, 0xf7427b },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0042ef },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x844252 },
    { 8, 8, 0x94a508 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf7ce10 },
    { 9, 4, 0xa510ce },
    { 9, 7, 0x007394 },
    { 9, 9, 0x9c8cde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4a5a42 },
    { 14, 4, 0x311042 },
    { 14, 7, 0x5a006b },
    { 14, 9, 0x638cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3121ef },
    { 16, 8, 0xbd73ef },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f763 },
    { 17, 7, 0x104a8c },
    { 19, 2, 0x52218c },
    { 19, 4, 0x219cc6 },
    { 19, 7, 0x6b5263 },
    { 19, 9, 0x18a510 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_XR24_unity.json
 * SHA256 d2f4d0034daee11918249c59866a4bc1a6e54e79dbb602ee6b7090d5003b87db
 */
static const HVSReferenceRegion hvs_reference_18_regions[] = {
    { 0x2e87e000,
      "5zXnY6Q37k8mE+d0EIJhBo56XpEVyPOkj4NftbXXMH2e++T/CZabNUVZSP/PIy4+6EAVgGF0"
      "ymUM9effiCVQC3mPB8BIbCDTGGgeUTsIEmjkI2FvN4l7brzSD0Un5O13ZX+nsWYe4VzzF2Sh"
      "g1p3oVuQlmMFvP9v8UMCYxIvKzFbuepEc0aOeovm0EOxqSotUUeChMkRocZyCXUxLPOT3tK3"
      "D9fGnyNAQhE3ZuYKzc7oJjcJZPaUMQMixWbAnyK3r6MIuO1FADunJW2vaCZrYDLnGiQ/vPD2"
      "bRbANnxCywzHrtYTLLQl9p4rZUuk8ophhiesXgjVLDqyD59oKv+qWBMHB1Dhw9GQr8I8MNcG"
      "RB+MJH5E+Skz7A4uBSdvOx4xIIPup9vV+cgRqVrvWMZg2qvq5bVH4WHc8OxDa0wq1r/hD8QG"
      "/J47FCwsymZx9tdc4Ats0JR1AOhUOYq4g/6bvZ/4Cs/DgmtjagVfVJu8yTBImjMHi0kVpf5n"
      "SitmVm3P0q/iYTrihK4C/wSEwXuojw6nlzsWz8VRzYAlzz5lJkT73C6oL2ZqvVVtfN0hxAdc"
      "6nC8z9OivmnOa+heiUVBJH5E967y/kWa/LPF8EEUXTOtIE4F3o2ftvNWdBmiOX92N+Zhi2Lx"
      "UZnWjGSsD23GboXT989CwyhdC/zXJxKhGMc="
    },
};
static const uint32_t hvs_reference_18_words_99[] = {
    0x4800d807, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_18_segments[] = {
    { 99, 9, hvs_reference_18_words_99 },
};
static const HVSReferencePixel hvs_reference_18_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_XR30_unity.json
 * SHA256 818962bf44e438e198149f88ad73f2918fd174d24ecfb7b0f7ccc0c3b9a7de17
 */
static const HVSReferenceRegion hvs_reference_19_regions[] = {
    { 0x2e87e000,
      "n1PzeZJys3uYMPF5QChYGDqml5dUjPy8PjrYl9Z+DUx6vj/5JGjpJhWVFdI/M4ILowdEhYVF"
      "t3IwXP/5IlISFOX5yMEhxQbIYISGR+yAgESTM1JY3JjYXvIuzUOcTH57lfXnqZnhcXjPcxGZ"
      "DqbVnW0JqWUUyPt/xzeEQEjwwgptmbt6zWWkYy5uPnTGmooKRXWkoCcTYejJkVAdsDzv5Et7"
      "y8Mb+8lICRHBTZujcPOjY8INkW0vJQwgcnED+4mIvjoKgrdXBACeUlKboWHSWsh8jgb8yDv8"
      "tWExMPEl9DIf67o1sEhLyXqyUlmSLq9iGnIiayBcDQvK8uBnqPyvakxwwEGHP3y0vi4MD19j"
      "EBEyQpJf55PCzDjgQgG9sYMHgDi4u29fff5EmJrWYW0c2K6ufrkdHV7Yw8/eUDGhsrWH8zAx"
      "8+vJDrDAsnLFbf91g7MQ21JWB8BRkaOiDu7vpn6Oj8IPK9haqVHQV27KezIhqckMLpZEhft3"
      "lhKZZVXbS/u6eOgsLqEI/A+BB7cnqjh46iVY/HxxNwtIyfhUhknvz42LvGSWmlXVFt+ETMxB"
      "qwcn708rqm87tzZ6JlZUEPlF9L3L71+R8zt78QVBURe2ApITe9vop89nFR2KktNf3GxemIkd"
      "X5RbyxiZPNS2cRY+/f0JPQxKLMz/NUgYCsY="
    },
};
static const uint32_t hvs_reference_19_words_99[] = {
    0x4800d810, 0x00020004, 0x4000fff0, 0x00080010, 0x00070000, 0xee87e000,
    0xee87e1c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_19_segments[] = {
    { 99, 9, hvs_reference_19_words_99 },
};
static const HVSReferencePixel hvs_reference_19_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xe735e7 },
    { 4, 4, 0xeab95b },
    { 4, 6, 0x070713 },
    { 4, 7, 0xc40fe1 },
    { 4, 9, 0xf7447e },
    { 4, 14, 0x000000 },
    { 5, 5, 0x0045ed },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x824751 },
    { 8, 8, 0x97a70e },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0xf3c815 },
    { 9, 4, 0xa111c9 },
    { 9, 7, 0x007594 },
    { 9, 9, 0x9f8dde },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x485945 },
    { 14, 4, 0x371142 },
    { 14, 7, 0x5f056a },
    { 14, 9, 0x648cd6 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x3726e8 },
    { 16, 8, 0xbc70ea },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x94f664 },
    { 17, 7, 0x15498b },
    { 19, 2, 0x502588 },
    { 19, 4, 0x229fc0 },
    { 19, 7, 0x6d5666 },
    { 19, 9, 0x18a112 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YU12_unity.json
 * SHA256 9ed470ec7821c6b34c751393beae85a54461029e8e07d241f8de3629fae18f15
 */
static const HVSReferenceRegion hvs_reference_20_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ1FoFMaVyrN1vD0xA8qYoW2ucWeOoF4q7q+Ww7D3BANcdoW"
      "xmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJ"
    },
};
static const uint32_t hvs_reference_20_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_20_words_111[] = {
    0x59009808, 0x00020004, 0x4000fff0, 0x00080010, 0x00070003, 0xee87e000,
    0xee87e080, 0xee87e0a0, 0xee87e070, 0xee87e098, 0xee87e0b8, 0x00000010,
    0x00000008, 0x00000008, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x40800070, 0x40800070, 0x4000c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_20_segments[] = {
    { 32, 11, hvs_reference_20_words_32 },
    { 111, 26, hvs_reference_20_words_111 },
};
static const HVSReferencePixel hvs_reference_20_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xffe385 },
    { 4, 4, 0x701de9 },
    { 4, 6, 0xffc0ff },
    { 4, 7, 0xffabff },
    { 4, 9, 0x6bccff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xc0c2ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb12ad1 },
    { 8, 8, 0x760000 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x580000 },
    { 9, 4, 0x9b001a },
    { 9, 7, 0xfffbdd },
    { 9, 9, 0xcf9a12 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x429400 },
    { 14, 4, 0xffc36c },
    { 14, 7, 0xffa392 },
    { 14, 9, 0x468800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x00422e },
    { 16, 8, 0x006700 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x0a72a5 },
    { 17, 7, 0xaf5b86 },
    { 19, 2, 0x006cff },
    { 19, 4, 0xff8bff },
    { 19, 7, 0x8188de },
    { 19, 9, 0x190000 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YU16_unity.json
 * SHA256 74015b1efae144e6593ccd474d976897b078283da6219d5d799572cc3a099cc8
 */
static const HVSReferenceRegion hvs_reference_21_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ3aFsZoNHBbJ4wS3Y3/sBrrvGqtkOnz9npV7bpdYHADia5b"
      "ewQUfju4Twyljzx19Pq1huNT5ZUXjqjjbZGCDgENQ4jXRntm3vEehBTV3C/PZbG8xVIITw8T"
      "JdRmwjK0YJ+K+z9syX0k0LalKqpILp+fXN+7Wg9r+eP4Wx2u6XqnMQ=="
    },
};
static const uint32_t hvs_reference_21_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_21_words_110[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080010, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e0c0, 0xee87e070, 0xee87e0b8, 0xee87e0f8, 0x00000010,
    0x00000008, 0x00000008, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x40800070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_21_segments[] = {
    { 32, 11, hvs_reference_21_words_32 },
    { 110, 26, hvs_reference_21_words_110 },
};
static const HVSReferencePixel hvs_reference_21_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x9affff },
    { 4, 4, 0x6726ae },
    { 4, 6, 0xd7ebff },
    { 4, 7, 0xffb688 },
    { 4, 9, 0xff86ff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xb0dbe8 },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x9936a0 },
    { 8, 8, 0x55009a },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x540067 },
    { 9, 4, 0x420a7b },
    { 9, 7, 0xabffff },
    { 9, 9, 0x51b19e },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4c8639 },
    { 14, 4, 0x65dbff },
    { 14, 7, 0x57ec94 },
    { 14, 9, 0x757800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x0033d2 },
    { 16, 8, 0xc51200 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x007fdf },
    { 17, 7, 0x9251ff },
    { 19, 2, 0xff2400 },
    { 19, 4, 0x14f2e2 },
    { 19, 7, 0xd75fff },
    { 19, 9, 0x002c00 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YU24_unity.json
 * SHA256 78732c486ec6b4c411996104bb382979f034730d928535d1accd046d6db50960
 */
static const HVSReferenceRegion hvs_reference_22_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ1DiNdGe2be8R6EFNXcL89lsbzFUghPDxMl1GbCMrRgn4r7"
      "P2zJfSTQtqUqqkgun59c37taD2v54/hbHa7peqcxcgO4u49n3ZdwS6pT7cd/6+sZdNDK4Ro2"
      "n7MIpP+4nVdF9HNEgP0X27qS0ZDHpEtAhBDB09mGrdDC/MTMXVZpgjiJG7ecRp9sDJ0VBldk"
      "U2SXYV6fCzS6/2blMgNAyfmW77bG1A6OCk/7uzTaz78kQrIfdjfYKxwEfw85DnTzaeus2KF7"
      "yCysHrlImD74RY4GZXU/99+uz6Ktw/ivY7kgdq7FiEDhjoGv1jQ9/Fi5T43Bq56/tanrJFjM"
      "aKW7s0iX"
    },
};
static const uint32_t hvs_reference_22_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_22_words_110[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e100, 0xee87e070, 0xee87e0f0, 0xee87e170, 0x00000010,
    0x00000010, 0x00000010, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_22_segments[] = {
    { 32, 11, hvs_reference_22_words_32 },
    { 110, 26, hvs_reference_22_words_110 },
};
static const HVSReferencePixel hvs_reference_22_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x83ff83 },
    { 4, 4, 0xec0664 },
    { 4, 6, 0xffd7c4 },
    { 4, 7, 0xafbdff },
    { 4, 9, 0xffa1b0 },
    { 4, 14, 0x000000 },
    { 5, 5, 0x81daff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x00798d },
    { 8, 8, 0x8b000b },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x001b00 },
    { 9, 4, 0x002b1d },
    { 9, 7, 0xffe6ff },
    { 9, 9, 0xe47dea },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x00b200 },
    { 14, 4, 0x78f864 },
    { 14, 7, 0xffa235 },
    { 14, 9, 0x4b65ff },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x800a00 },
    { 16, 8, 0x1e2acb },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xd44a00 },
    { 17, 7, 0xa450ff },
    { 19, 2, 0x156042 },
    { 19, 4, 0x69d5ff },
    { 19, 7, 0x42a483 },
    { 19, 9, 0x0c0000 },
    { 22, 3, 0x674279 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x2e96ca },
    { 24, 5, 0xb60d00 },
    { 24, 8, 0xffff77 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x1a1000 },
    { 28, 3, 0xfff9ff },
    { 29, 1, 0x000000 },
    { 29, 4, 0x02699e },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x7be3ff },
    { 31, 5, 0xff7512 },
    { 31, 7, 0x59a87c },
    { 31, 8, 0xa89e15 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YV12_unity.json
 * SHA256 c33ac4292127119d12df8f7821cb4a69db6c6c63165144faa5df947fc6f91319
 */
static const HVSReferenceRegion hvs_reference_23_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ3aFsZoNHBbJ4wS3Y3/sBrrvGqtkOnz9npV7bpdYHADiUWg"
      "UxpXKs3W8PTEDypihba5xZ46gXirur5bDsPcEA1x"
    },
};
static const uint32_t hvs_reference_23_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_23_words_110[] = {
    0x5900b808, 0x00020004, 0x4000fff0, 0x00080010, 0x00070003, 0xee87e000,
    0xee87e080, 0xee87e0a0, 0xee87e070, 0xee87e098, 0xee87e0b8, 0x00000010,
    0x00000008, 0x00000008, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x40800070, 0x40800070, 0x4000c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_23_segments[] = {
    { 32, 11, hvs_reference_23_words_32 },
    { 110, 26, hvs_reference_23_words_110 },
};
static const HVSReferencePixel hvs_reference_23_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0xffe385 },
    { 4, 4, 0x701de9 },
    { 4, 6, 0xffc0ff },
    { 4, 7, 0xffabff },
    { 4, 9, 0x6bccff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xc0c2ff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb12ad1 },
    { 8, 8, 0x760000 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x580000 },
    { 9, 4, 0x9b001a },
    { 9, 7, 0xfffbdd },
    { 9, 9, 0xcf9a12 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x429400 },
    { 14, 4, 0xffc36c },
    { 14, 7, 0xffa392 },
    { 14, 9, 0x468800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x00422e },
    { 16, 8, 0x006700 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x0a72a5 },
    { 17, 7, 0xaf5b86 },
    { 19, 2, 0x006cff },
    { 19, 4, 0xff8bff },
    { 19, 7, 0x8188de },
    { 19, 9, 0x190000 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YV16_unity.json
 * SHA256 cf68888e2d61bd64cdea9dbcef0dc54adf806e8eb33051bfde76804d93b8c201
 */
static const HVSReferenceRegion hvs_reference_24_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ1DiNdGe2be8R6EFNXcL89lsbzFUghPDxMl1GbCMrRgn4r7"
      "P2zJfSTQtqUqqkgun59c37taD2v54/hbHa7peqcx2hbGaDRwWyeMEt2N/7Aa67xqrZDp8/Z6"
      "Ve26XWBwA4muW3sEFH47uE8MpY88dfT6tYbjU+WVF46o422Rgg4BDQ=="
    },
};
static const uint32_t hvs_reference_24_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_24_words_110[] = {
    0x5900b80a, 0x00020004, 0x4000fff0, 0x00080010, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e0c0, 0xee87e070, 0xee87e0b8, 0xee87e0f8, 0x00000010,
    0x00000008, 0x00000008, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x40800070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_24_segments[] = {
    { 32, 11, hvs_reference_24_words_32 },
    { 110, 26, hvs_reference_24_words_110 },
};
static const HVSReferencePixel hvs_reference_24_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x9affff },
    { 4, 4, 0x6726ae },
    { 4, 6, 0xd7ebff },
    { 4, 7, 0xffb688 },
    { 4, 9, 0xff86ff },
    { 4, 14, 0x000000 },
    { 5, 5, 0xb0dbe8 },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x9936a0 },
    { 8, 8, 0x55009a },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x540067 },
    { 9, 4, 0x420a7b },
    { 9, 7, 0xabffff },
    { 9, 9, 0x51b19e },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x4c8639 },
    { 14, 4, 0x65dbff },
    { 14, 7, 0x57ec94 },
    { 14, 9, 0x757800 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x0033d2 },
    { 16, 8, 0xc51200 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0x007fdf },
    { 17, 7, 0x9251ff },
    { 19, 2, 0xff2400 },
    { 19, 4, 0x14f2e2 },
    { 19, 7, 0xd75fff },
    { 19, 9, 0x002c00 },
    { 22, 3, 0x000000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x000000 },
    { 24, 5, 0x000000 },
    { 24, 8, 0x000000 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x000000 },
    { 28, 3, 0x000000 },
    { 29, 1, 0x000000 },
    { 29, 4, 0x000000 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x000000 },
    { 31, 5, 0x000000 },
    { 31, 7, 0x000000 },
    { 31, 8, 0x000000 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/fmt_YV24_unity.json
 * SHA256 a432951071baa305272f2024af21606121b80d57186157047898e7dc58963953
 */
static const HVSReferenceRegion hvs_reference_25_regions[] = {
    { 0x2e87e000,
      "8omzScMFv/eM63QASuG8U61rHmYmrM8sCR9yLtjjOdhFoFMaVyrN1vD0xA8qYoW2ucWeOoF4"
      "q7q+Ww7D3BANcdoWxmg0cFsnjBLdjf+wGuu8aq2Q6fP2elXtul1gcAOJrlt7BBR+O7hPDKWP"
      "PHX0+rWG41PllReOqONtkYIOAQ04iRu3nEafbAydFQZXZFNkl2Fenws0uv9m5TIDQMn5lu+2"
      "xtQOjgpP+7s02s+/JEKyH3Y32CscBH8POQ5082nrrNihe8gsrB65SJg++EWOBmV1P/ffrs+i"
      "rcP4r2O5IHauxYhA4Y6Br9Y0PfxYuU+Nwauev7Wp6yRYzGilu7NIl0OI10Z7Zt7xHoQU1dwv"
      "z2WxvMVSCE8PEyXUZsIytGCfivs/bMl9JNC2pSqqSC6fn1zfu1oPa/nj+Fsdrul6pzFyA7i7"
      "j2fdl3BLqlPtx3/r6xl00MrhGjafswik/7idV0X0c0SA/RfbupLRkMekS0CEEMHT2Yat0ML8"
      "xMxdVmmC"
    },
};
static const uint32_t hvs_reference_25_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_25_words_110[] = {
    0x5900b80a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e100, 0xee87e070, 0xee87e0f0, 0xee87e170, 0x00000010,
    0x00000010, 0x00000010, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_25_segments[] = {
    { 32, 11, hvs_reference_25_words_32 },
    { 110, 26, hvs_reference_25_words_110 },
};
static const HVSReferencePixel hvs_reference_25_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x83ff83 },
    { 4, 4, 0xec0664 },
    { 4, 6, 0xffd7c4 },
    { 4, 7, 0xafbdff },
    { 4, 9, 0xffa1b0 },
    { 4, 14, 0x000000 },
    { 5, 5, 0x81daff },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0x00798d },
    { 8, 8, 0x8b000b },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x001b00 },
    { 9, 4, 0x002b1d },
    { 9, 7, 0xffe6ff },
    { 9, 9, 0xe47dea },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x00b200 },
    { 14, 4, 0x78f864 },
    { 14, 7, 0xffa235 },
    { 14, 9, 0x4b65ff },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x800a00 },
    { 16, 8, 0x1e2acb },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xd44a00 },
    { 17, 7, 0xa450ff },
    { 19, 2, 0x156042 },
    { 19, 4, 0x69d5ff },
    { 19, 7, 0x42a483 },
    { 19, 9, 0x0c0000 },
    { 22, 3, 0x674279 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0x2e96ca },
    { 24, 5, 0xb60d00 },
    { 24, 8, 0xffff77 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x1a1000 },
    { 28, 3, 0xfff9ff },
    { 29, 1, 0x000000 },
    { 29, 4, 0x02699e },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x7be3ff },
    { 31, 5, 0xff7512 },
    { 31, 7, 0x59a87c },
    { 31, 8, 0xa89e15 },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_2020_full.json
 * SHA256 a8b73ab793b0459b6e0d742157062db3cd694d5a243af7c212cdd5675e1d6f37
 */
static const HVSReferenceRegion hvs_reference_26_regions[] = {
    { 0x2e87f000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_26_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_26_words_110[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87f000,
    0xee87f080, 0xee87f100, 0xee87f070, 0xee87f0f0, 0xee87f170, 0x00000010,
    0x00000010, 0x00000010, 0x00000000, 0xf5b6d400, 0x0005e5e2, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_26_segments[] = {
    { 32, 11, hvs_reference_26_words_32 },
    { 110, 26, hvs_reference_26_words_110 },
};
static const HVSReferencePixel hvs_reference_26_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x00bc54 },
    { 4, 4, 0x24a4e6 },
    { 4, 6, 0xffcd5f },
    { 4, 7, 0xafaf9c },
    { 4, 9, 0x677409 },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffd3ae },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xa6edff },
    { 8, 8, 0x5dab00 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x0294f7 },
    { 9, 4, 0x5c4de8 },
    { 9, 7, 0x4f2d55 },
    { 9, 9, 0x2749ea },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x0be100 },
    { 14, 4, 0x00c134 },
    { 14, 7, 0x00228e },
    { 14, 9, 0x6bffe3 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x73ffff },
    { 16, 8, 0x674553 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xcd6c73 },
    { 17, 7, 0x004f96 },
    { 19, 2, 0x22a9ff },
    { 19, 4, 0xffb600 },
    { 19, 7, 0x822e00 },
    { 19, 9, 0x97f3f7 },
    { 22, 3, 0x634500 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xc85000 },
    { 24, 5, 0x5dff0c },
    { 24, 8, 0x6b4646 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x9f7a7a },
    { 28, 3, 0x795c00 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xffdb02 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x650000 },
    { 31, 5, 0x88ff37 },
    { 31, 7, 0xfd9443 },
    { 31, 8, 0x936e6e },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_2020_lim.json
 * SHA256 105778a3359f4f5e4871aaca71b578fd531ac6f137ee5e381d51e12ab8cca05c
 */
static const HVSReferenceRegion hvs_reference_27_regions[] = {
    { 0x2e87e000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_27_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_27_words_54[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e100, 0xee87e070, 0xee87e0f0, 0xee87e170, 0x00000010,
    0x00000010, 0x00000010, 0x00f00000, 0xf43594a8, 0x0006b624, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_27_segments[] = {
    { 32, 11, hvs_reference_27_words_32 },
    { 54, 26, hvs_reference_27_words_54 },
};
static const HVSReferencePixel hvs_reference_27_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x00c651 },
    { 4, 4, 0x1aacf6 },
    { 4, 6, 0xffde60 },
    { 4, 7, 0xb9baa3 },
    { 4, 9, 0x657400 },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffe4ba },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb1ffff },
    { 8, 8, 0x5bb400 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x0098ff },
    { 9, 4, 0x5947f8 },
    { 9, 7, 0x49234f },
    { 9, 9, 0x1c43f9 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x00f100 },
    { 14, 4, 0x00cc2c },
    { 14, 7, 0x00158f },
    { 14, 9, 0x6cfff5 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x77ffff },
    { 16, 8, 0x653e4e },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xda6c74 },
    { 17, 7, 0x004899 },
    { 19, 2, 0x18b2ff },
    { 19, 4, 0xffc200 },
    { 19, 7, 0x832300 },
    { 19, 9, 0x9fffff },
    { 22, 3, 0x603e00 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xd34b00 },
    { 24, 5, 0x5dff00 },
    { 24, 8, 0x693f3f },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0xa67c7c },
    { 28, 3, 0x7a5800 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xffed00 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x720000 },
    { 31, 5, 0x8fff33 },
    { 31, 7, 0xff9b3e },
    { 31, 8, 0x986e6e },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_601_full.json
 * SHA256 153dbb27c5081016b3428fab5987b71af8daaedff15efc112db55b5191c8b49c
 */
static const HVSReferenceRegion hvs_reference_28_regions[] = {
    { 0x2e87f000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_28_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_28_words_180[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87f000,
    0xee87f080, 0xee87f100, 0xee87f070, 0xee87f0f0, 0xee87f170, 0x00000010,
    0x00000010, 0x00000010, 0x00000000, 0xea349400, 0x00059dc6, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_28_segments[] = {
    { 32, 11, hvs_reference_28_words_32 },
    { 180, 26, hvs_reference_28_words_180 },
};
static const HVSReferencePixel hvs_reference_28_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x07cd57 },
    { 4, 4, 0x28a4e0 },
    { 4, 6, 0xffcd68 },
    { 4, 7, 0xafb19d },
    { 4, 9, 0x677d0e },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffcfb3 },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xa9e3ff },
    { 8, 8, 0x60c200 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x0792f0 },
    { 9, 4, 0x5c3fe0 },
    { 9, 7, 0x4e2953 },
    { 9, 9, 0x293de0 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x12ff00 },
    { 14, 4, 0x00d639 },
    { 14, 7, 0x001d87 },
    { 14, 9, 0x70ffe2 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x79ffff },
    { 16, 8, 0x664253 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xc96775 },
    { 17, 7, 0x004f90 },
    { 19, 2, 0x27a5ff },
    { 19, 4, 0xffc005 },
    { 19, 7, 0x7f3000 },
    { 19, 9, 0x9af7f6 },
    { 22, 3, 0x625000 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xc35d00 },
    { 24, 5, 0x62ff17 },
    { 24, 8, 0x6a4447 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0x9e797b },
    { 28, 3, 0x786600 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xffe710 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x600000 },
    { 31, 5, 0x8eff42 },
    { 31, 7, 0xfa9649 },
    { 31, 8, 0x926d6f },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_601_lim.json
 * SHA256 4e2d61abafe20e6d647bc1edc4b8ea40d53b1f8ec655a43e8c7a2a2ab43958fe
 */
static const HVSReferenceRegion hvs_reference_29_regions[] = {
    { 0x2e87e000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_29_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_29_words_154[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e100, 0xee87e070, 0xee87e0f0, 0xee87e170, 0x00000010,
    0x00000010, 0x00000010, 0x00f00000, 0xe73304a8, 0x00066604, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_29_segments[] = {
    { 32, 11, hvs_reference_29_words_32 },
    { 154, 26, hvs_reference_29_words_154 },
};
static const HVSReferencePixel hvs_reference_29_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x00da54 },
    { 4, 4, 0x1facf0 },
    { 4, 6, 0xffdd6b },
    { 4, 7, 0xb9bca4 },
    { 4, 9, 0x657f01 },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffdfbf },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xb4f6ff },
    { 8, 8, 0x5ecd00 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x0096ff },
    { 9, 4, 0x5937ee },
    { 9, 7, 0x481d4d },
    { 9, 9, 0x1e35ee },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x06ff00 },
    { 14, 4, 0x00e431 },
    { 14, 7, 0x000f87 },
    { 14, 9, 0x72fff5 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x7dffff },
    { 16, 8, 0x643b4e },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xd66676 },
    { 17, 7, 0x004992 },
    { 19, 2, 0x1dacff },
    { 19, 4, 0xffcd00 },
    { 19, 7, 0x802600 },
    { 19, 9, 0xa3ffff },
    { 22, 3, 0x5f4b00 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xce5a00 },
    { 24, 5, 0x63ff0e },
    { 24, 8, 0x683d40 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0xa57a7d },
    { 28, 3, 0x786400 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xfffb06 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x6d0000 },
    { 31, 5, 0x95ff40 },
    { 31, 7, 0xff9d45 },
    { 31, 8, 0x976c6f },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_709_full.json
 * SHA256 cef5bd2077f50cba10c9adc0b4f0b6bcb0f6cd686343415cd38363b85e03b80e
 */
static const HVSReferenceRegion hvs_reference_30_regions[] = {
    { 0x2e87f000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_30_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_30_words_110[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87f000,
    0xee87f080, 0xee87f100, 0xee87f070, 0xee87f0f0, 0xee87f170, 0x00000010,
    0x00000010, 0x00000010, 0x00000000, 0xf4388400, 0x00064ddb, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_30_segments[] = {
    { 32, 11, hvs_reference_30_words_32 },
    { 110, 26, hvs_reference_30_words_110 },
};
static const HVSReferencePixel hvs_reference_30_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x00b355 },
    { 4, 4, 0x1d9ce4 },
    { 4, 6, 0xffdb61 },
    { 4, 7, 0xafb09c },
    { 4, 9, 0x67750a },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffdcaf },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xa2e7ff },
    { 8, 8, 0x5aaa00 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x008af5 },
    { 9, 4, 0x5c4be6 },
    { 9, 7, 0x512f54 },
    { 9, 9, 0x2545e7 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x02da00 },
    { 14, 4, 0x00b735 },
    { 14, 7, 0x001c8c },
    { 14, 9, 0x63fce3 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x6bffff },
    { 16, 8, 0x694653 },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xd17174 },
    { 17, 7, 0x004694 },
    { 19, 2, 0x1ba0ff },
    { 19, 4, 0xffbf00 },
    { 19, 7, 0x873400 },
    { 19, 9, 0x93eef7 },
    { 22, 3, 0x654900 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xcf5a00 },
    { 24, 5, 0x55ff0f },
    { 24, 8, 0x6d4846 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0xa17c7b },
    { 28, 3, 0x7b5f00 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xffe506 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x6c0000 },
    { 31, 5, 0x80ff3a },
    { 31, 7, 0xff9b44 },
    { 31, 8, 0x95706e },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/csc_YU24_709_lim.json
 * SHA256 7810f16a3e58ea8241a43f1b564522d5a0b1420269a1699829b526c086c307e3
 */
static const HVSReferenceRegion hvs_reference_31_regions[] = {
    { 0x2e87e000,
      "hJRfdktzX0IkbZYP3EAHjUsrhubfR4O2d/nbutygPLGG5UXh41qWZ1u2gb7rho/KQvt4eGO7"
      "QyXc7ccB2RZzSP7jgVHTgLNnv0Z5/mwVE7euXZdiVDlzVSPbE+GvLwFAnz7Qmokn8LsSAqOr"
      "T/7ycmqXEMBQSoTZboTZeLZ9k9tos9kTosvrLCMXApxh5vbHqh+U3kZLGjm8UG+dm8aUEbFB"
      "M7j74t4Ws5dCGaVVQAJ/TC31viSwh20YeXrhY8kbGjtGvcuZTJiFLL4aD1UHNoouqtlSlBQ4"
      "MJ3PxvHHiU1M5poBDoSNqXN+v9VQ99GFT+ztY8DzYQ4R9ltANAJEnSn+HG3bDQtJmE4b24Nz"
      "+QswfKEE5q5wGtp4BjO8S+60RLB0kS+DREBbNRxIDey2w//TIuOPrcI/aafAJ/TUmQ3lx9L2"
      "HyiAbgFKpplq3JYFaC05QSLFbKBRCFFeGRx231vRKflPORuyGlku2pzXmH2GkNWQj2wxK1IY"
      "PYJMcJwz"
    },
};
static const uint32_t hvs_reference_31_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_31_words_54[] = {
    0x5900980a, 0x00020004, 0x4000fff0, 0x00080020, 0x00070007, 0xee87e000,
    0xee87e080, 0xee87e100, 0xee87e070, 0xee87e0f0, 0xee87e170, 0x00000010,
    0x00000010, 0x00000010, 0x00f00000, 0xf27784a8, 0x00072e1d, 0x00000000,
    0x41000070, 0x41000070, 0x0001c000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_31_segments[] = {
    { 32, 11, hvs_reference_31_words_32 },
    { 54, 26, hvs_reference_31_words_54 },
};
static const HVSReferencePixel hvs_reference_31_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 4, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 10, 0x000000 },
    { 0, 12, 0x000000 },
    { 0, 15, 0x000000 },
    { 1, 0, 0x000000 },
    { 1, 2, 0x000000 },
    { 2, 2, 0x000000 },
    { 2, 5, 0x000000 },
    { 2, 8, 0x000000 },
    { 3, 2, 0x000000 },
    { 4, 2, 0x00bc52 },
    { 4, 4, 0x12a2f5 },
    { 4, 6, 0xffed62 },
    { 4, 7, 0xb9baa3 },
    { 4, 9, 0x657500 },
    { 4, 14, 0x000000 },
    { 5, 5, 0xffefbb },
    { 6, 11, 0x000000 },
    { 7, 0, 0x000000 },
    { 8, 0, 0x000000 },
    { 8, 4, 0xacfaff },
    { 8, 8, 0x58b300 },
    { 8, 12, 0x000000 },
    { 8, 15, 0x000000 },
    { 9, 0, 0x000000 },
    { 9, 2, 0x008dff },
    { 9, 4, 0x5945f5 },
    { 9, 7, 0x4b244f },
    { 9, 9, 0x193ef7 },
    { 9, 15, 0x000000 },
    { 12, 14, 0x000000 },
    { 14, 2, 0x00e900 },
    { 14, 4, 0x00c12d },
    { 14, 7, 0x000d8e },
    { 14, 9, 0x64fff5 },
    { 15, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 4, 0x6dffff },
    { 16, 8, 0x67404e },
    { 16, 12, 0x000000 },
    { 16, 15, 0x000000 },
    { 17, 4, 0xdf7275 },
    { 17, 7, 0x003e98 },
    { 19, 2, 0x0fa6ff },
    { 19, 4, 0xffcd00 },
    { 19, 7, 0x892a00 },
    { 19, 9, 0x9affff },
    { 22, 3, 0x634300 },
    { 22, 15, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 4, 0xdb5700 },
    { 24, 5, 0x53ff03 },
    { 24, 8, 0x6b4240 },
    { 24, 12, 0x000000 },
    { 24, 15, 0x000000 },
    { 26, 8, 0xa87e7c },
    { 28, 3, 0x7c5d00 },
    { 29, 1, 0x000000 },
    { 29, 4, 0xfff800 },
    { 30, 11, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 4, 0x7a0000 },
    { 31, 5, 0x85ff36 },
    { 31, 7, 0xffa33f },
    { 31, 8, 0x9a706e },
    { 31, 12, 0x000000 },
    { 31, 15, 0x000000 },
};
/*
 * cap1/ppf_2d_rand.json
 * SHA256 e17a593b7a4e317b8784390b18ce6b848b44147b77c0ddead959d834c64ac8b9
 */
static const HVSReferenceRegion hvs_reference_32_regions[] = {
    { 0x2e87f000,
      "dSV8WzP3Uk3ZSH3kVLJ2ixKMgiyyKMsjbvjttlUAr3Wbe7jQtd7G4mVfjlJtNs6n3QUDDC9R"
      "OjPL9A66tESEl55PEGUP0fTDkdVa7nFpsWEa+plBcRu8dojq5bRX6mIDp88P6zUm4FMwF1aJ"
      "9DzwsJKXBgYEJx+r7GM2yX5KN41KIVkRMmbtCX/hCHOMtt7e1XLUwtH4ZmJ2dr0cpis523Ui"
      "+A8C9lt08Gq3GRVW5vUZxP21iWI8eIFy4YGLOQUMmFLevQUuFPFh8fLokvEzDZAInXOABOtY"
      "gGEOcMYIp/vKqdf5CZksSKuY0cfyKo7PWFJzDGub8HTQMPduRERteb8EG7twX6XBcB7ufjzy"
      "nZaMpuKbQSpnPdEwtJWdxJYM65OBBhQQ9j8H0ksdIafY2TcfWyr74zmKmlSYCLYW7SiVCQUT"
      "6JtQk4z516dEpycgcB6R0ScU4I/X3+Tf56n0MZlhwyKspQjWrvaOzUg0R1N11M2RvfnO9j4X"
      "xSx1laK2fBqjLgtnzboyDagmp+jqwJ7N0WqrVqZE01r0Oj7/P2m99wIaWyxDKXncyPm+mW6K"
      "PD7T8msNha3yWhuk4czIUJOdqMVk5hnJoXWMgc2pEPB+nKVyoewBM3QdlKajHueY"
    },
};
static const uint32_t hvs_reference_32_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_32_words_104[] = {
    0x51005807, 0x00050003, 0x4000fff0, 0x00250028, 0x000a000c, 0x00090000,
    0xee87f000, 0xee87f1b0, 0x00000030, 0x00000000, 0x404ccc60, 0x40453060,
    0xc0007ff0, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_32_segments[] = {
    { 32, 11, hvs_reference_32_words_32 },
    { 104, 18, hvs_reference_32_words_104 },
};
static const HVSReferencePixel hvs_reference_32_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 11, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 32, 0x000000 },
    { 0, 42, 0x000000 },
    { 0, 48, 0x000000 },
    { 0, 63, 0x000000 },
    { 2, 9, 0x000000 },
    { 3, 2, 0x000000 },
    { 3, 5, 0x831d73 },
    { 3, 17, 0xae84cd },
    { 3, 29, 0x7d8fe9 },
    { 3, 41, 0xd84138 },
    { 4, 10, 0x0e08d3 },
    { 5, 22, 0xb44d79 },
    { 5, 34, 0xa21b63 },
    { 6, 9, 0x344277 },
    { 8, 11, 0x524f39 },
    { 8, 38, 0xa73e3c },
    { 8, 59, 0x000000 },
    { 9, 27, 0xdf644e },
    { 11, 23, 0xf09c8d },
    { 12, 45, 0x000000 },
    { 14, 1, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 5, 0x7aac27 },
    { 16, 16, 0x923eb4 },
    { 16, 17, 0x902ba3 },
    { 16, 29, 0xa96331 },
    { 16, 32, 0xdc7fad },
    { 16, 41, 0xc4b8c0 },
    { 16, 48, 0x000000 },
    { 16, 63, 0x000000 },
    { 18, 3, 0x000000 },
    { 18, 39, 0xacad95 },
    { 18, 60, 0x000000 },
    { 25, 58, 0x000000 },
    { 29, 5, 0xb01d6d },
    { 29, 17, 0xd0701c },
    { 29, 29, 0x9b14a3 },
    { 29, 41, 0x45a2b4 },
    { 30, 2, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 16, 0xa96e37 },
    { 32, 32, 0xb5c98e },
    { 32, 48, 0x000000 },
    { 32, 63, 0x000000 },
    { 34, 19, 0x4b825e },
    { 34, 31, 0x99d0cc },
    { 42, 5, 0xd42e6e },
    { 42, 17, 0x2d508b },
    { 42, 29, 0x2da547 },
    { 42, 41, 0xec1aa3 },
    { 44, 14, 0x000000 },
    { 44, 61, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 21, 0x000000 },
    { 48, 32, 0x000000 },
    { 48, 48, 0x000000 },
    { 48, 63, 0x000000 },
    { 53, 35, 0x000000 },
    { 56, 14, 0x000000 },
    { 58, 5, 0x000000 },
    { 59, 19, 0x000000 },
    { 61, 45, 0x000000 },
    { 62, 31, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 20, 0x000000 },
    { 63, 32, 0x000000 },
    { 63, 48, 0x000000 },
    { 63, 63, 0x000000 },
};
/*
 * cap1/ppf_h_imp_8_256.json
 * SHA256 e47dc73fa06b4d1e04b11440c01e469bc2333a4b14845841fcd67ba43c6760ac
 */
static const HVSReferenceRegion hvs_reference_33_regions[] = {
    { 0x2e87f000,
      "AAAAAAAAAAAAAAAA/////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAA="
    },
};
static const uint32_t hvs_reference_33_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_33_words_99[] = {
    0x4e005c87, 0x00000000, 0x4000fff0, 0x00040100, 0x00040008, 0x00030000,
    0xee87f000, 0xee87f060, 0x00000020, 0x40080060, 0x00000020, 0x00000020,
    0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_33_segments[] = {
    { 32, 11, hvs_reference_33_words_32 },
    { 99, 15, hvs_reference_33_words_99 },
};
static const HVSReferencePixel hvs_reference_33_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 1, 0x000000 },
    { 0, 2, 0x000000 },
    { 0, 3, 0x000000 },
    { 2, 2, 0x000000 },
    { 3, 0, 0x000000 },
    { 9, 0, 0x000000 },
    { 15, 0, 0x000000 },
    { 17, 0, 0x000000 },
    { 20, 1, 0x000000 },
    { 23, 2, 0x000000 },
    { 27, 0, 0x000000 },
    { 32, 2, 0x000000 },
    { 33, 3, 0x000000 },
    { 35, 0, 0x000000 },
    { 38, 1, 0x000000 },
    { 46, 1, 0x000000 },
    { 50, 2, 0x000000 },
    { 58, 0, 0x000000 },
    { 64, 0, 0x000000 },
    { 64, 1, 0x000000 },
    { 64, 2, 0x000000 },
    { 64, 3, 0x000000 },
    { 72, 2, 0x000000 },
    { 73, 0, 0x000000 },
    { 73, 3, 0x000000 },
    { 85, 0, 0x3a3a3a },
    { 85, 1, 0x000000 },
    { 85, 2, 0x000000 },
    { 85, 3, 0x000000 },
    { 102, 3, 0x000000 },
    { 121, 0, 0xb3b3b3 },
    { 128, 0, 0x777777 },
    { 128, 1, 0x000000 },
    { 128, 2, 0x000000 },
    { 128, 3, 0x000000 },
    { 136, 1, 0x000000 },
    { 139, 1, 0x000000 },
    { 170, 0, 0x000000 },
    { 170, 1, 0x000000 },
    { 170, 2, 0x000000 },
    { 170, 3, 0x000000 },
    { 177, 0, 0x000000 },
    { 177, 3, 0x000000 },
    { 192, 0, 0x000000 },
    { 192, 1, 0x000000 },
    { 192, 2, 0x000000 },
    { 192, 3, 0x000000 },
    { 193, 1, 0x000000 },
    { 213, 2, 0x000000 },
    { 225, 0, 0x000000 },
    { 232, 0, 0x000000 },
    { 237, 1, 0x000000 },
    { 246, 2, 0x000000 },
    { 249, 1, 0x000000 },
    { 254, 1, 0x000000 },
    { 255, 0, 0x000000 },
    { 255, 1, 0x000000 },
    { 255, 2, 0x000000 },
    { 255, 3, 0x000000 },
};
/*
 * cap1/ppf_h_rand_16_11.json
 * SHA256 854633107f238048e29ee9dcad11a73981d5604af051489783a7f9a3f6778d9e
 */
static const HVSReferenceRegion hvs_reference_34_regions[] = {
    { 0x2e87f000,
      "HUonl+bEbXkNHEoBAmOb5e0SeU6B+v5MCAMMGCOnE3ZCuDPXsvumfCv1qSs9fx0CcaXwJq13"
      "vZAmcwBDnMRAmlzABMB2vfj6IWNnujNBk/Fes2cJCb3k/AhGKcl+fquCowA9S7r/ErGyDjmy"
      "XjdRNQQA4LK6VQrCBaHcZQvVGTANcxMOW+m0ujfNdAcsKlONc6Pvj8DeniXyVfx1G7tV5EGb"
      "mZDlKirWjGsQCGlClD5khnXLm/eTvC27qB/IwSgocC98CBMRzbKkXZTV5AHaZIkYI1hOZfQ+"
      "baQEJ/LBXFOacOTOPzpTKA3MLlK9oh9V42FDWkZPhUKCdRjRmeKa5A=="
    },
};
static const uint32_t hvs_reference_34_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_34_words_99[] = {
    0x4e005c87, 0x00000000, 0x4000fff0, 0x0004000b, 0x00040010, 0x00030000,
    0xee87f000, 0xee87f0c0, 0x00000040, 0x41745c60, 0x00000020, 0x00000020,
    0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_34_segments[] = {
    { 32, 11, hvs_reference_34_words_32 },
    { 99, 15, hvs_reference_34_words_99 },
};
static const HVSReferencePixel hvs_reference_34_pixels[] = {
    { 0, 0, 0x244515 },
    { 0, 1, 0x00c05b },
    { 0, 2, 0x0c6e09 },
    { 0, 3, 0x793074 },
    { 2, 2, 0x5d7c2e },
    { 3, 0, 0x7d1ad9 },
    { 3, 1, 0x6baa5a },
    { 3, 2, 0xe2996d },
    { 3, 3, 0x341691 },
    { 7, 0, 0xaefb4f },
    { 7, 1, 0x2d4fb7 },
    { 7, 2, 0x1159a7 },
    { 7, 3, 0x35a714 },
    { 9, 0, 0xdb889e },
    { 10, 0, 0x067b32 },
    { 10, 1, 0xc8a606 },
    { 10, 2, 0x9bbc3c },
    { 10, 3, 0x258084 },
    { 15, 0, 0x000000 },
    { 17, 0, 0x000000 },
    { 20, 1, 0x000000 },
    { 23, 2, 0x000000 },
    { 27, 0, 0x000000 },
    { 32, 2, 0x000000 },
    { 33, 3, 0x000000 },
    { 35, 0, 0x000000 },
    { 38, 1, 0x000000 },
    { 46, 1, 0x000000 },
    { 50, 2, 0x000000 },
    { 58, 0, 0x000000 },
    { 64, 0, 0x000000 },
    { 64, 1, 0x000000 },
    { 64, 2, 0x000000 },
    { 64, 3, 0x000000 },
    { 72, 2, 0x000000 },
    { 73, 0, 0x000000 },
    { 73, 3, 0x000000 },
    { 102, 3, 0x000000 },
    { 121, 0, 0x000000 },
    { 128, 0, 0x000000 },
    { 128, 1, 0x000000 },
    { 128, 2, 0x000000 },
    { 128, 3, 0x000000 },
    { 136, 1, 0x000000 },
    { 139, 1, 0x000000 },
    { 177, 0, 0x000000 },
    { 177, 3, 0x000000 },
    { 192, 0, 0x000000 },
    { 192, 1, 0x000000 },
    { 192, 2, 0x000000 },
    { 192, 3, 0x000000 },
    { 193, 1, 0x000000 },
    { 213, 2, 0x000000 },
    { 225, 0, 0x000000 },
    { 232, 0, 0x000000 },
    { 237, 1, 0x000000 },
    { 246, 2, 0x000000 },
    { 249, 1, 0x000000 },
    { 254, 1, 0x000000 },
    { 255, 0, 0x000000 },
    { 255, 1, 0x000000 },
    { 255, 2, 0x000000 },
    { 255, 3, 0x000000 },
};
/*
 * cap1/ppf_v_rand_8_24.json
 * SHA256 39fa6f9645be0896a8d4e746a750facb7afaaa59a2e752e7bf4e71a9988e7aeb
 */
static const HVSReferenceRegion hvs_reference_35_regions[] = {
    { 0x2e87f000,
      "q8JVe7u5d8ItOI0V/3z1eZ4HX0IP46RSCKH2xQNT4yf+/ZD+lPckYKgRmmanLNPcQZCekars"
      "/pAyjxrErdjan8bQj979haN45vF9wYr2POhnxhzjKgqjzGfLkTJBom28hKYh4TNVu1FYNj+P"
      "1F6kb0KjfBBGkos19lijU+21A1/r27XEpd6bqHiryfQDvMdZCxFe2QwGOEModBB5TPa6Mawv"
      "P0VfiiDUuChMS4E8Ly55ZT3LnGCiMARlAeL9JB3D5SHRAcQFjTGQ3rLiib5n/HIDYmwVT8af"
      "ktPqXFEL8t3JLbeQDUbo6rHpnXgqFPjUOwojgWBrBAfEdPabqTnCVg=="
    },
};
static const uint32_t hvs_reference_35_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_35_words_105[] = {
    0x50005da7, 0x00000000, 0x4000fff0, 0x00180008, 0x00080008, 0x00070000,
    0xee87f000, 0xee87f0e0, 0x00000020, 0x00000000, 0x40555460, 0x40007fe0,
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_35_segments[] = {
    { 32, 11, hvs_reference_35_words_32 },
    { 105, 17, hvs_reference_35_words_105 },
};
static const HVSReferencePixel hvs_reference_35_pixels[] = {
    { 0, 0, 0x53c0a8 },
    { 0, 2, 0x62d0be },
    { 0, 8, 0x76c6b6 },
    { 0, 9, 0x4db69c },
    { 0, 10, 0x2dab8c },
    { 0, 11, 0x40b39a },
    { 0, 16, 0x4d43b4 },
    { 0, 22, 0xbb2bcd },
    { 0, 23, 0xb72dc9 },
    { 0, 32, 0x000000 },
    { 0, 34, 0x000000 },
    { 0, 42, 0x000000 },
    { 0, 48, 0x000000 },
    { 0, 63, 0x000000 },
    { 1, 1, 0x77b9bb },
    { 1, 11, 0xb67349 },
    { 1, 23, 0xea4709 },
    { 1, 27, 0x000000 },
    { 1, 38, 0x000000 },
    { 1, 45, 0x000000 },
    { 1, 59, 0x000000 },
    { 2, 0, 0x8d3928 },
    { 2, 3, 0x951a6e },
    { 2, 8, 0x6dcbc8 },
    { 2, 16, 0x5b37b1 },
    { 2, 23, 0x9deab2 },
    { 2, 32, 0x000000 },
    { 2, 39, 0x000000 },
    { 2, 48, 0x000000 },
    { 2, 60, 0x000000 },
    { 2, 63, 0x000000 },
    { 3, 2, 0xf068ed },
    { 3, 58, 0x000000 },
    { 4, 0, 0x5c01a2 },
    { 4, 16, 0x405b6c },
    { 4, 19, 0x5d0e73 },
    { 4, 31, 0x000000 },
    { 4, 32, 0x000000 },
    { 4, 48, 0x000000 },
    { 4, 63, 0x000000 },
    { 5, 0, 0xa1e308 },
    { 5, 8, 0x9d252e },
    { 5, 14, 0x4d172b },
    { 5, 16, 0x96558b },
    { 5, 23, 0x006b61 },
    { 5, 61, 0x000000 },
    { 6, 0, 0xffa206 },
    { 6, 16, 0x026707 },
    { 6, 21, 0xf9a2b2 },
    { 6, 32, 0x000000 },
    { 6, 35, 0x000000 },
    { 6, 48, 0x000000 },
    { 6, 63, 0x000000 },
    { 7, 0, 0xe44e00 },
    { 7, 5, 0xc4cf97 },
    { 7, 8, 0x52a566 },
    { 7, 14, 0x99cb71 },
    { 7, 16, 0x2f3ce9 },
    { 7, 19, 0xda0d65 },
    { 7, 20, 0xec1461 },
    { 7, 23, 0xc139ab },
    { 7, 31, 0x000000 },
    { 7, 32, 0x000000 },
    { 7, 45, 0x000000 },
    { 7, 48, 0x000000 },
    { 7, 63, 0x000000 },
};
/*
 * cap1/tpz_h_rand_32_16.json
 * SHA256 32484ad03f4694cada30e7257014ece92c1cdb1e23a9699f0da01a3962f6e0d9
 */
static const HVSReferenceRegion hvs_reference_36_regions[] = {
    { 0x2e880000,
      "cYTDzxPCqkJndnYwHz5VCZePxeG91ZF+CpuNY1jOxNH9+wSu13p/zC0xCXjx8ODUGOFP7D13"
      "TJTP96ON6l1VyyrygxEulR7dCCQ9myx+EGaM/d2w5/GQXtzAblfGGyURig24pxFk881O00Ad"
      "LP8J8M+lPxnFuzLwtVhJ1/DaW45mCBtVvXGI4yxUokdRzXPklEwN3GkNXTpewN55ig0d9op3"
      "mgr4iswu/AxKYi1+Nkt9xS+dcl/QGlIWCx0Np/y2KV7KECz6dbqJIAvsphQrrH5hRYBZvSel"
      "QVzqvoGNzXRL+NkjVyyPaLPgDgwDCAcF/J7t3Sk2pM0pm1oHJcdHKzkiIeq0oxRBS+A3Ay6+"
      "QVBmW9P7jysryrok1hl8vTI1Jsd7L0qzcEOZUt/uKuq5YVJY62oih9JeX1HmNuotKsZ7IY1F"
      "bAdsUIChfxj0nY0vb6DBWpWlphAqCMIIsmOWPdQa4c3+2CmqszxtJ+QIVIf1TG2EsY0PHMWM"
      "c1+bsVyzS1bwjTDxe+YLziA/5Pd57k2BT+SUi/yx3VKoePN9PpbqDXTjTo3EAPcbPCKjYuQT"
      "dqRi3y2Q/vOxkcnVKoC2rDmVimSUirWFk5IPCoUvW4d6Ik4EqodO522cWksDT9KE7M8kX9kG"
      "FXAjv7Tl7ndtVHW/zFcy1J322+aRZE/WTSI="
    },
};
static const uint32_t hvs_reference_36_words_87[] = {
    0x4b005fe7, 0x00000000, 0x4000fff0, 0x00040010, 0x00040020, 0x00030000,
    0xee880000, 0xee880180, 0x00000080, 0x02000000, 0x00007fff, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_36_segments[] = {
    { 87, 12, hvs_reference_36_words_87 },
};
static const HVSReferencePixel hvs_reference_36_pixels[] = {
    { 0, 0, 0xb7a342 },
    { 0, 1, 0x513c92 },
    { 0, 2, 0x1a6276 },
    { 0, 3, 0xb6a43d },
    { 1, 0, 0x655a43 },
    { 2, 0, 0xabb2aa },
    { 2, 1, 0x352c7e },
    { 2, 2, 0x7f437a },
    { 3, 0, 0xa9b531 },
    { 4, 0, 0x41bbea },
    { 4, 1, 0xb380c1 },
    { 4, 2, 0x75bd38 },
    { 4, 3, 0x9c4d9c },
    { 5, 0, 0x74918f },
    { 5, 1, 0x404595 },
    { 5, 2, 0xcc9e61 },
    { 5, 3, 0x903ecd },
    { 6, 2, 0xdf6f3a },
    { 7, 0, 0x7caadd },
    { 8, 0, 0x50c42c },
    { 8, 1, 0xa0ac2a },
    { 8, 2, 0x7c1473 },
    { 8, 3, 0xa09066 },
    { 9, 0, 0x26511a },
    { 9, 2, 0x869fba },
    { 9, 3, 0x35608c },
    { 10, 0, 0xb7f7ba },
    { 10, 1, 0x368f6b },
    { 10, 2, 0xb4a382 },
    { 10, 3, 0x4e5492 },
    { 12, 3, 0x1c6ae3 },
    { 15, 0, 0x5299d3 },
    { 15, 1, 0x50b127 },
    { 15, 2, 0x679fb0 },
    { 15, 3, 0x6fde95 },
    { 16, 0, 0x000000 },
    { 16, 1, 0x000000 },
    { 16, 2, 0x000000 },
    { 16, 3, 0x000000 },
    { 17, 1, 0x000000 },
    { 22, 0, 0x000000 },
    { 22, 3, 0x000000 },
    { 24, 0, 0x000000 },
    { 24, 1, 0x000000 },
    { 24, 2, 0x000000 },
    { 24, 3, 0x000000 },
    { 26, 2, 0x000000 },
    { 28, 0, 0x000000 },
    { 29, 0, 0x000000 },
    { 29, 1, 0x000000 },
    { 30, 2, 0x000000 },
    { 31, 0, 0x000000 },
    { 31, 1, 0x000000 },
    { 31, 2, 0x000000 },
    { 31, 3, 0x000000 },
};
/*
 * cap1/tpz_v_rand_32_8.json
 * SHA256 e9574f15c576c01e69535d7ac16ce5bb99e552178ac1f734f3db205be3262163
 */
static const HVSReferenceRegion hvs_reference_37_regions[] = {
    { 0x2e880000,
      "Keo5k/AzDOAJJYz4Gm19PrOHcg5yQ0d6I7I0URQjLJ8PfdgHfr0Q0GaRDy8KUC9zTuBvOnqa"
      "d+xABA088pllTjo11v9vRHCV7ofli4M7jtS8NgP3VU9R8TqL2HxTkK7pn2uOSjevT8QNaG5h"
      "n4esRcD6NCPLgC+NZRrC1CYvMa582LeINI2wAZoWh5A9ARaYFarr+rFCP0zbhsHkvVIo9yPi"
      "lrCu2rmqBgRiwdc5I+n6JhekNkx0YN0SlHq5vSjE60t3d07cM9LeQcIWWhWOooqd6PL3urrD"
      "7hRTxd+3vU2Ja7X9pX9rEn00iEAstSSpjtBXM+ZPfeF8eVMz7VPs6cJawges04pJDgxw0f1b"
      "pk+mqhwmXdJ18Pl/EIdsiKGttVHNy4Q3iSmuwyGojxNswo3woaIFcWz5YWb/EivryiN7s+p4"
      "0JxJk/p0h3R7ckcu8wx+3vtihjLlzNszCKjM4y+OuVWWI3CZcY5GDs+z/Bd8zC6mnKkYHLwO"
      "OJbyO063sIVRv9lb2NyphyBKtBPFfeg3w9KK3u9cnLCAV4CgBVl23zooPTuIw2LFrnuhYTn+"
      "PYKWSV4W3nZi31DsbgDogvdkAeuNuoaCkNxbBDMjNu5jqZiTl6BlUHQ/TiW4mb/iW55SsJrR"
      "AJkHp6XZQM76gIyXCekXQTmaG2K5ZZXXncRAwnr3+1kdNjSae3kGzyjDkQArt/Cb+Jf1kiOX"
      "4WtAC67cQcpepPOMGu5UBIHihxn04tj+Om1skiHyZnXNVp+0+yvMcCWRv2Ebq+CATS/nRkx4"
      "PTGbnn4UK3Nznn9lIOE8S60R1QwKunMXdUPCfXAu5vMDl+IlJKeY6R7U29R1WZxjImYexBBD"
      "Mv2pu2rMJItsfHlUh0uOgykC+PZQNm2XetlDrVhw8jomWnzvUO3yJgEZ0EjRf2Fz5j4wy/Dp"
      "MbuVRNwuEO21ynai2AeT4tKHkNrvP+jkG+gkUTK8d/MfhofmzsNlhK2+2X43NzqNf3dz+GLx"
      "THphDIiM5adI4UdxJCfYIB6SXe1UeI6anYvs0ZOrfzvgoyZHPyniF+9GMhObTRmbznYH/IvE"
      "Lzv8Tnkx4G1VeCkg0EUU1PjG3dD26KWpbb2A7s3WDdQeTy4fVNR1acIyCtcB+onHx9AUCoeP"
      "WcLoEPD0kr3iA9hvjrT541yYT+Pfx2F4+9aXuS5wbzLw+R28snAft9GLoB+q3x3LpPgZjTUO"
      "9GFSx1A+qKwd6NLPKS79VF9sk+8jCs1BIhkSDXiq6MnOfC0NPUChKn5pgdsUI6VFKWo77NZu"
      "zl2hdwAZhajshWhL08KgARuneo1gbIdKuejKQv+nQ4DirJlZGsLqOGiOoQ2I1yVDQ4EEBw=="
    },
};
static const uint32_t hvs_reference_37_words_72[] = {
    0x4d005ec7, 0x00000000, 0x4000fff0, 0x00080008, 0x00100008, 0x000f0000,
    0xee880000, 0xee8803c0, 0x00000040, 0x00000000, 0x02000000, 0x00007fff,
    0x00000000, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_37_segments[] = {
    { 72, 14, hvs_reference_37_words_72 },
};
static const HVSReferencePixel hvs_reference_37_pixels[] = {
    { 0, 0, 0x889031 },
    { 0, 1, 0x9792b4 },
    { 0, 2, 0xd6879f },
    { 0, 4, 0xa3769e },
    { 0, 5, 0x7f5399 },
    { 0, 7, 0x18eab9 },
    { 0, 8, 0x000000 },
    { 0, 11, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 17, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 1, 0, 0x3e3bb0 },
    { 1, 5, 0x62d917 },
    { 1, 11, 0x000000 },
    { 1, 13, 0x000000 },
    { 1, 19, 0x000000 },
    { 1, 22, 0x000000 },
    { 1, 29, 0x000000 },
    { 2, 0, 0xb9567b },
    { 2, 1, 0xa52bbc },
    { 2, 2, 0x7b4084 },
    { 2, 5, 0xc1d054 },
    { 2, 7, 0xbbbc86 },
    { 2, 8, 0x000000 },
    { 2, 16, 0x000000 },
    { 2, 19, 0x000000 },
    { 2, 24, 0x000000 },
    { 2, 30, 0x000000 },
    { 2, 31, 0x000000 },
    { 3, 1, 0x520b4b },
    { 3, 29, 0x000000 },
    { 4, 0, 0x3a5eb8 },
    { 4, 8, 0x000000 },
    { 4, 9, 0x000000 },
    { 4, 15, 0x000000 },
    { 4, 16, 0x000000 },
    { 4, 24, 0x000000 },
    { 4, 31, 0x000000 },
    { 5, 0, 0x4c4963 },
    { 5, 2, 0x7d9aac },
    { 5, 5, 0x5498b8 },
    { 5, 7, 0xae4991 },
    { 5, 30, 0x000000 },
    { 6, 0, 0x869f2e },
    { 6, 8, 0x000000 },
    { 6, 10, 0x000000 },
    { 6, 16, 0x000000 },
    { 6, 17, 0x000000 },
    { 6, 24, 0x000000 },
    { 6, 31, 0x000000 },
    { 7, 0, 0x6d5933 },
    { 7, 2, 0xb7983a },
    { 7, 5, 0x469563 },
    { 7, 7, 0x4baa61 },
    { 7, 8, 0x000000 },
    { 7, 9, 0x000000 },
    { 7, 10, 0x000000 },
    { 7, 15, 0x000000 },
    { 7, 16, 0x000000 },
    { 7, 22, 0x000000 },
    { 7, 24, 0x000000 },
    { 7, 31, 0x000000 },
};
/*
 * cap1/blend_none_32768.json
 * SHA256 4dcda76e7e61cfa0a108a5895d96feb57a9bc9f0d65067e40907f8484ff887bf
 */
static const HVSReferenceRegion hvs_reference_38_regions[] = {
    { 0x2e803000,
      "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"
    },
    { 0x2e802000,
      "OsFHk/YQfDinaQxWVWjkkMD6Xjgjwz0VrRmw7WnWKGfC7XahQ4GJzup/GV3edqPoGDlbecqI"
      "3FTqIRgg4VAKCbhbhzpqo59ikh3XhQUPYdcp/1z5O1E5j0+PH53oFae3KPmrim8PhLzxjV6e"
      "iALJKUBun/AK50hxUnePSJviimB63F7YZUfdx6y1GU3Lr68m5YQQj1kE1rnPwoAF5zaR69lZ"
      "0fSGucthao5eDH2GnWxzYL9MioTLn5WODBYXUZgOerLd/Hv92jy1/y2z60Uucb5h4zk//7Z0"
      "dM8ytLKv3dTUJ85Xv8YdTEFjZIRGUnp7FyvN9p0ZA9s3wjE6QK3qsHd4Y0XKG1eDQk4PRDRk"
      "wD6+ubKWhQBbeAakqLUEC7ayVDphjeIa4yOrIRKcUDIgyfciRHJTW2dmUoMzbDObGkSFoGNP"
      "vdLSoz2qPoHzpEtDVxrrI1l3FnQrpD0ZgpD/+eMBRZbmepE5pnzyPaQd89Z0PCEvXKG1ABmY"
      "OAo/DIT5uaE97SM5CdMFNF9F0+OzequqIFncWa6kMFExh3b0VxbQkd6jh9SJURXJWiuvtRUc"
      "i0cn4kvOYXinnbS6zHg8dzF0bkEPv4KMsxT5pCXtIhdQj1wg4m8LDAYvjQ/nQTGcxXrwwdme"
      "asUlR841uOOwtQ4ZPNf2g1wyx3wTu2xOeTk="
    },
};
static const uint32_t hvs_reference_38_words_54[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00100020, 0x000f0000, 0xee803000,
    0xee803780, 0x00000080, 0x4800d807, 0x00020004, 0x50008000, 0x00080010,
    0x00070000, 0xee802000, 0xee8021c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_38_segments[] = {
    { 54, 17, hvs_reference_38_words_54 },
};
static const HVSReferencePixel hvs_reference_38_pixels[] = {
    { 0, 0, 0x54a85d },
    { 0, 2, 0x0139a1 },
    { 0, 4, 0x8ddceb },
    { 0, 5, 0x5cb242 },
    { 0, 8, 0x85a1b7 },
    { 0, 10, 0x4e5671 },
    { 0, 12, 0x730d99 },
    { 0, 15, 0x1df4a1 },
    { 1, 0, 0xd699ae },
    { 1, 2, 0xfb8a29 },
    { 2, 2, 0x23e265 },
    { 2, 5, 0xc1b0e4 },
    { 2, 8, 0x0ef775 },
    { 3, 2, 0x7176c1 },
    { 4, 2, 0x8c4385 },
    { 4, 4, 0x47cbb2 },
    { 4, 6, 0x357bb3 },
    { 4, 7, 0xc6e453 },
    { 4, 9, 0x9268c0 },
    { 4, 14, 0x359d73 },
    { 5, 5, 0x4c7dca },
    { 6, 11, 0x0ae9f0 },
    { 7, 0, 0x5d4d70 },
    { 8, 0, 0x7805a1 },
    { 8, 4, 0x634490 },
    { 8, 8, 0x72693b },
    { 8, 12, 0xb4d8bf },
    { 8, 15, 0xf94971 },
    { 9, 0, 0x0150ad },
    { 9, 2, 0x8f847c },
    { 9, 4, 0xd740ce },
    { 9, 7, 0x999161 },
    { 9, 9, 0x917451 },
    { 9, 15, 0xbf5c8c },
    { 10, 0, 0x074e4f },
    { 10, 5, 0xa5b639 },
    { 10, 10, 0x32e5a6 },
    { 10, 15, 0xdd852d },
    { 12, 14, 0x9dc6c2 },
    { 14, 2, 0x4794cf },
    { 14, 4, 0x4d401d },
    { 14, 7, 0xb5975f },
    { 14, 9, 0xaa9425 },
    { 15, 0, 0x76cf50 },
    { 16, 0, 0x773efc },
    { 16, 4, 0xc8429f },
    { 16, 8, 0x757492 },
    { 16, 12, 0x6c275f },
    { 16, 15, 0xe3672b },
    { 17, 4, 0x5764c9 },
    { 17, 7, 0xc4bf50 },
    { 19, 2, 0x3fb754 },
    { 19, 4, 0x2fbf3b },
    { 19, 7, 0x43909e },
    { 19, 9, 0x746db4 },
    { 21, 0, 0x112bdf },
    { 21, 5, 0xfa41a6 },
    { 21, 10, 0x50143a },
    { 21, 15, 0x0a4957 },
    { 22, 3, 0x0e6ce7 },
    { 22, 15, 0xe8425c },
    { 24, 0, 0x3b3115 },
    { 24, 4, 0x908ec8 },
    { 24, 5, 0xa56591 },
    { 24, 8, 0xb379be },
    { 24, 12, 0xfd2c1d },
    { 24, 15, 0x2c9031 },
    { 26, 8, 0x09d6d4 },
    { 28, 3, 0xf8611a },
    { 29, 1, 0x2f9f95 },
    { 29, 4, 0xfced66 },
    { 30, 11, 0xd39050 },
    { 31, 0, 0xe8a2da },
    { 31, 4, 0x55875e },
    { 31, 5, 0xb56ed4 },
    { 31, 7, 0x606353 },
    { 31, 8, 0xe66238 },
    { 31, 10, 0x288057 },
    { 31, 12, 0x718db0 },
    { 31, 15, 0xa76a3c },
};
/*
 * cap1/blend_coverage_32768.json
 * SHA256 5f0516960cbaf89865c5489feabc255edfc415ecd5a518ffae13fe82e3c2ee6b
 */
static const HVSReferenceRegion hvs_reference_39_regions[] = {
    { 0x2e801000,
      "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"
    },
    { 0x2e802000,
      "OsFHk/YQfDinaQxWVWjkkMD6Xjgjwz0VrRmw7WnWKGfC7XahQ4GJzup/GV3edqPoGDlbecqI"
      "3FTqIRgg4VAKCbhbhzpqo59ikh3XhQUPYdcp/1z5O1E5j0+PH53oFae3KPmrim8PhLzxjV6e"
      "iALJKUBun/AK50hxUnePSJviimB63F7YZUfdx6y1GU3Lr68m5YQQj1kE1rnPwoAF5zaR69lZ"
      "0fSGucthao5eDH2GnWxzYL9MioTLn5WODBYXUZgOerLd/Hv92jy1/y2z60Uucb5h4zk//7Z0"
      "dM8ytLKv3dTUJ85Xv8YdTEFjZIRGUnp7FyvN9p0ZA9s3wjE6QK3qsHd4Y0XKG1eDQk4PRDRk"
      "wD6+ubKWhQBbeAakqLUEC7ayVDphjeIa4yOrIRKcUDIgyfciRHJTW2dmUoMzbDObGkSFoGNP"
      "vdLSoz2qPoHzpEtDVxrrI1l3FnQrpD0ZgpD/+eMBRZbmepE5pnzyPaQd89Z0PCEvXKG1ABmY"
      "OAo/DIT5uaE97SM5CdMFNF9F0+OzequqIFncWa6kMFExh3b0VxbQkd6jh9SJURXJWiuvtRUc"
      "i0cn4kvOYXinnbS6zHg8dzF0bkEPv4KMsxT5pCXtIhdQj1wg4m8LDAYvjQ/nQTGcxXrwwdme"
      "asUlR841uOOwtQ4ZPNf2g1wyx3wTu2xOeTk="
    },
};
static const uint32_t hvs_reference_39_words_123[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00100020, 0x000f0000, 0xee801000,
    0xee801780, 0x00000080, 0x4800d807, 0x00020004, 0x10008000, 0x00080010,
    0x00070000, 0xee802000, 0xee8021c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_39_segments[] = {
    { 123, 17, hvs_reference_39_words_123 },
};
static const HVSReferencePixel hvs_reference_39_pixels[] = {
    { 0, 0, 0x54a85d },
    { 0, 2, 0x0139a1 },
    { 0, 4, 0x8ddceb },
    { 0, 5, 0x5cb242 },
    { 0, 8, 0x85a1b7 },
    { 0, 10, 0x4e5671 },
    { 0, 12, 0x730d99 },
    { 0, 15, 0x1df4a1 },
    { 1, 0, 0xd699ae },
    { 1, 2, 0xfb8a29 },
    { 2, 2, 0x23e265 },
    { 2, 5, 0xc1b0e4 },
    { 2, 8, 0x0ef775 },
    { 3, 2, 0x7176c1 },
    { 4, 2, 0x894981 },
    { 4, 4, 0x4ccfa5 },
    { 4, 6, 0x2e7bbc },
    { 4, 7, 0xd2ec4d },
    { 4, 9, 0x9866d7 },
    { 4, 14, 0x359d73 },
    { 5, 5, 0x3366d8 },
    { 6, 11, 0x0ae9f0 },
    { 7, 0, 0x5d4d70 },
    { 8, 0, 0x7805a1 },
    { 8, 4, 0x604793 },
    { 8, 8, 0x7b6330 },
    { 8, 12, 0xb4d8bf },
    { 8, 15, 0xf94971 },
    { 9, 0, 0x0150ad },
    { 9, 2, 0xa67294 },
    { 9, 4, 0xd737bd },
    { 9, 7, 0x9d9261 },
    { 9, 9, 0xb97625 },
    { 9, 15, 0xbf5c8c },
    { 10, 0, 0x074e4f },
    { 10, 5, 0x95ba41 },
    { 10, 10, 0x32e5a6 },
    { 10, 15, 0xdd852d },
    { 12, 14, 0x9dc6c2 },
    { 14, 2, 0x4b96cd },
    { 14, 4, 0x482806 },
    { 14, 7, 0xaa9c52 },
    { 14, 9, 0xa3a325 },
    { 15, 0, 0x76cf50 },
    { 16, 0, 0x773efc },
    { 16, 4, 0xc93ea4 },
    { 16, 8, 0x616890 },
    { 16, 12, 0x6c275f },
    { 16, 15, 0xe3672b },
    { 17, 4, 0x6067c3 },
    { 17, 7, 0xc8c84c },
    { 19, 2, 0x50d728 },
    { 19, 4, 0x0fe046 },
    { 19, 7, 0x586680 },
    { 19, 9, 0x7472c2 },
    { 21, 0, 0x112bdf },
    { 21, 5, 0xfa41a6 },
    { 21, 10, 0x50143a },
    { 21, 15, 0x0a4957 },
    { 22, 3, 0x0e6ce7 },
    { 22, 15, 0xe8425c },
    { 24, 0, 0x3b3115 },
    { 24, 4, 0x908ec8 },
    { 24, 5, 0xa56591 },
    { 24, 8, 0xb379be },
    { 24, 12, 0xfd2c1d },
    { 24, 15, 0x2c9031 },
    { 26, 8, 0x09d6d4 },
    { 28, 3, 0xf8611a },
    { 29, 1, 0x2f9f95 },
    { 29, 4, 0xfced66 },
    { 30, 11, 0xd39050 },
    { 31, 0, 0xe8a2da },
    { 31, 4, 0x55875e },
    { 31, 5, 0xb56ed4 },
    { 31, 7, 0x606353 },
    { 31, 8, 0xe66238 },
    { 31, 10, 0x288057 },
    { 31, 12, 0x718db0 },
    { 31, 15, 0xa76a3c },
};
/*
 * cap1/blend_premult_32768.json
 * SHA256 d42d6f59040e781f2796c01fc0691340d3349f7262d66e7b950fbb4e491dd16f
 */
static const HVSReferenceRegion hvs_reference_40_regions[] = {
    { 0x2e803000,
      "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"
    },
    { 0x2e802000,
      "OsFHk/YQfDinaQxWVWjkkMD6Xjgjwz0VrRmw7WnWKGfC7XahQ4GJzup/GV3edqPoGDlbecqI"
      "3FTqIRgg4VAKCbhbhzpqo59ikh3XhQUPYdcp/1z5O1E5j0+PH53oFae3KPmrim8PhLzxjV6e"
      "iALJKUBun/AK50hxUnePSJviimB63F7YZUfdx6y1GU3Lr68m5YQQj1kE1rnPwoAF5zaR69lZ"
      "0fSGucthao5eDH2GnWxzYL9MioTLn5WODBYXUZgOerLd/Hv92jy1/y2z60Uucb5h4zk//7Z0"
      "dM8ytLKv3dTUJ85Xv8YdTEFjZIRGUnp7FyvN9p0ZA9s3wjE6QK3qsHd4Y0XKG1eDQk4PRDRk"
      "wD6+ubKWhQBbeAakqLUEC7ayVDphjeIa4yOrIRKcUDIgyfciRHJTW2dmUoMzbDObGkSFoGNP"
      "vdLSoz2qPoHzpEtDVxrrI1l3FnQrpD0ZgpD/+eMBRZbmepE5pnzyPaQd89Z0PCEvXKG1ABmY"
      "OAo/DIT5uaE97SM5CdMFNF9F0+OzequqIFncWa6kMFExh3b0VxbQkd6jh9SJURXJWiuvtRUc"
      "i0cn4kvOYXinnbS6zHg8dzF0bkEPv4KMsxT5pCXtIhdQj1wg4m8LDAYvjQ/nQTGcxXrwwdme"
      "asUlR841uOOwtQ4ZPNf2g1wyx3wTu2xOeTk="
    },
};
static const uint32_t hvs_reference_40_words_54[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00100020, 0x000f0000, 0xee803000,
    0xee803780, 0x00000080, 0x4800d807, 0x00020004, 0x30008000, 0x00080010,
    0x00070000, 0xee802000, 0xee8021c0, 0x00000040, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_40_segments[] = {
    { 54, 17, hvs_reference_40_words_54 },
};
static const HVSReferencePixel hvs_reference_40_pixels[] = {
    { 0, 0, 0x54a85d },
    { 0, 2, 0x0139a1 },
    { 0, 4, 0x8ddceb },
    { 0, 5, 0x5cb242 },
    { 0, 8, 0x85a1b7 },
    { 0, 10, 0x4e5671 },
    { 0, 12, 0x730d99 },
    { 0, 15, 0x1df4a1 },
    { 1, 0, 0xd699ae },
    { 1, 2, 0xfb8a29 },
    { 2, 2, 0x23e265 },
    { 2, 5, 0xc1b0e4 },
    { 2, 8, 0x0ef775 },
    { 3, 2, 0x7176c1 },
    { 4, 2, 0x98728d },
    { 4, 4, 0x54e0ae },
    { 4, 6, 0x52a7e8 },
    { 4, 7, 0xf4ff7b },
    { 4, 9, 0xc191e9 },
    { 4, 14, 0x359d73 },
    { 5, 5, 0x87c7ff },
    { 6, 11, 0x0ae9f0 },
    { 7, 0, 0x5d4d70 },
    { 8, 0, 0x7805a1 },
    { 8, 4, 0x6364c6 },
    { 8, 8, 0x859a68 },
    { 8, 12, 0xb4d8bf },
    { 8, 15, 0xf94971 },
    { 9, 0, 0x0150ad },
    { 9, 2, 0xc2cca4 },
    { 9, 4, 0xf437c9 },
    { 9, 7, 0xa3b37a },
    { 9, 9, 0xbeab91 },
    { 9, 15, 0xbf5c8c },
    { 10, 0, 0x074e4f },
    { 10, 5, 0x9cd852 },
    { 10, 10, 0x32e5a6 },
    { 10, 15, 0xdd852d },
    { 12, 14, 0x9dc6c2 },
    { 14, 2, 0x53beff },
    { 14, 4, 0x756c39 },
    { 14, 7, 0xffcb92 },
    { 14, 9, 0xf5c033 },
    { 15, 0, 0x76cf50 },
    { 16, 0, 0x773efc },
    { 16, 4, 0xff60cd },
    { 16, 8, 0x636c98 },
    { 16, 12, 0x6c275f },
    { 16, 15, 0xe3672b },
    { 17, 4, 0x8780dd },
    { 17, 7, 0xffff7a },
    { 19, 2, 0x54fe95 },
    { 19, 4, 0x58ff51 },
    { 19, 7, 0x596681 },
    { 19, 9, 0xa391ec },
    { 21, 0, 0x112bdf },
    { 21, 5, 0xfa41a6 },
    { 21, 10, 0x50143a },
    { 21, 15, 0x0a4957 },
    { 22, 3, 0x0e6ce7 },
    { 22, 15, 0xe8425c },
    { 24, 0, 0x3b3115 },
    { 24, 4, 0x908ec8 },
    { 24, 5, 0xa56591 },
    { 24, 8, 0xb379be },
    { 24, 12, 0xfd2c1d },
    { 24, 15, 0x2c9031 },
    { 26, 8, 0x09d6d4 },
    { 28, 3, 0xf8611a },
    { 29, 1, 0x2f9f95 },
    { 29, 4, 0xfced66 },
    { 30, 11, 0xd39050 },
    { 31, 0, 0xe8a2da },
    { 31, 4, 0x55875e },
    { 31, 5, 0xb56ed4 },
    { 31, 7, 0x606353 },
    { 31, 8, 0xe66238 },
    { 31, 10, 0x288057 },
    { 31, 12, 0x718db0 },
    { 31, 15, 0xa76a3c },
};
/*
 * cap2/crop_nv12_frac.json
 * SHA256 e3a36a1dd397e5614042d23602d9620841b8eecb85e998dc7030b18e230f561b
 */
static const HVSReferenceRegion hvs_reference_41_regions[] = {
    { 0x2e800000,
      "y2GqXgcEru1Fcj5HWOLSpd9HnFM97aweTn7iZXBaB3joDP7BxfEi+0vDBkZA258KRDUrxZKs"
      "bt67DO23p8dRsZ/bq9PzFpnectKzERqke+aQKRueSBC0ZnEI3/FFxBdF95mQLpDhYYkBFaZV"
      "Zsdc88zFfhKZu6DMuL3iyM2qyVGfY75emv2m0NZpCKpBCF4w5s+baaTnQ/R7aFObU6sYOp1X"
      "JnOs0YGaa8TF1gKef0UkyhshS6EaaaPV5uHbPXE91J57lsGZkKc1W77rbaGeK75uXM4KxHve"
      "O5AM45mWhb0L/GGkBruumfeilQevveC9VGyKESqZ7hbDREE4ndi6Vfx0gKZw/3QM+4GGtIMk"
      "bCtUrrCz9r7ONyd8UXfk+O1vcubSz7ObEWkapKTne0Pm9JB7KWgbU56bSFMQq7QYZjpxnQhX"
      "3ybxc0WsxNEXgUWa92uZxJDFLtaQAuGeYX+JRQEkFcqmG1UhZkvHoVwa82nMo8XVfuYS4Znb"
      "uz2gccw9uNS9nuJ7yJbNwaqZyZBRp581Y1u+vl7rmm39oaae0CvWvmluCFyqzkEKCMReezDe"
    },
};
static const uint32_t hvs_reference_41_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_41_words_81[] = {
    0x5a001809, 0x00020004, 0x4000fff0, 0x00170028, 0x0007000a, 0x00060002,
    0xee800031, 0xee800138, 0xee8000c1, 0xee800168, 0x00000018, 0x00000018,
    0x00000000, 0xf5b6d400, 0x0005e5e2, 0x00000080, 0x401e6620, 0x401e9b78,
    0x00009fed, 0x403ccc00, 0x403d3670, 0xc0003fda, 0x00000020, 0x00000020,
    0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_41_segments[] = {
    { 32, 11, hvs_reference_41_words_32 },
    { 81, 27, hvs_reference_41_words_81 },
};
static const HVSReferencePixel hvs_reference_41_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 5, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x000000 },
    { 3, 1, 0x000000 },
    { 4, 2, 0x433b00 },
    { 4, 5, 0x9f8529 },
    { 4, 9, 0xe0a5f3 },
    { 4, 17, 0xd4b9ff },
    { 4, 24, 0xc2bcff },
    { 5, 11, 0xd67ef9 },
    { 5, 17, 0xe4b4ff },
    { 6, 4, 0x845000 },
    { 8, 5, 0xb36500 },
    { 8, 19, 0xcb8bff },
    { 8, 29, 0x000000 },
    { 9, 13, 0xf97fe6 },
    { 11, 11, 0xa71e6b },
    { 12, 22, 0x72bcc7 },
    { 14, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x49205e },
    { 16, 16, 0xa0d7b1 },
    { 16, 24, 0x006100 },
    { 16, 31, 0x000000 },
    { 17, 2, 0x05c9ea },
    { 17, 9, 0x93477c },
    { 17, 17, 0x81e1ab },
    { 17, 24, 0x006000 },
    { 18, 1, 0x000000 },
    { 18, 19, 0x4ff1bb },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 30, 2, 0xd4e1b4 },
    { 30, 9, 0x65be78 },
    { 30, 17, 0x928bff },
    { 30, 24, 0x5e26f9 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x31b282 },
    { 32, 16, 0x6b92f1 },
    { 32, 24, 0x5b0bcb },
    { 32, 31, 0x000000 },
    { 34, 9, 0x00704b },
    { 34, 15, 0x379dde },
    { 43, 2, 0xffe9dc },
    { 43, 9, 0x008c67 },
    { 43, 17, 0x8995ff },
    { 43, 24, 0x5e00b6 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/crop_frac_ppf2.json
 * SHA256 190ae46f52c8afda61b271322c60870e5aaacaa2fc1c9c936b85d01017783a92
 */
static const HVSReferenceRegion hvs_reference_42_regions[] = {
    { 0x2e805000,
      "l8z+4eKY6wVBOC3GhwdBMHYnwVU+jRG9ZPcZPK2ZbENEFmD3jY9ZBdGmnki6tyrTtL0b+ISb"
      "0sTdZU6SVr208qjVmoWq9Gd0vIzzNup2dV7qw+T1yOFVwI4pFjxz/Q/wMJIK/iHC7lSnU7K2"
      "GYjFfsJmXzrw1KKlefv0cRVk6J0ZOqQqV/Ph6joQdbulOEXo4sXQxz2tXwW0O0GdaqoGo+i0"
      "vP93+uKEkd9g7yUL5CJ2PxHXH/M+1hfK8c/tUduQHsFLbilsbKrE/4tU8nqDKTZsMq//m6El"
      "jKomO0pBUVyiOzZlmiLT57fUI13bDl2qzDmA2Its7jxHaoojlYqjlKMB0yDQe/7D0zBS89ig"
      "riA4uX8inceYMK/PC0aPtt425GnlyUDCykBvUjW7B/AaSc+i6EFsHDkH2FTtuxX7U3hr5h4F"
      "qK/1QYXbiOyLcPPPAEQn+eNYyt/lcBoUxuTe7OIaSBn2b+TwqXXCMRrzIHP2Y5WYPd3BK7gr"
      "of6G58LF1Z5UKBUKc/coAi+WtN8TV9uxN5u4/RuYf9yCbVN4GH/rcfv1/MnfJWLqO4RxEx8K"
      "K9t4c4gB10kfgNUhSpkzIPG8ISc6P5CBcFWRvQc28JkvHW0WzaNOQrP9fv/0QeH9POLb5OZN"
      "cIhyFDdQMsOs1A/wdJtLDrWdD+XjHfAN4/S8/7otLKgJyjAyUAY4lbn9hP/4x8Bm363Lqfdb"
      "TZeQhm0O7fAWQUL7mD23OoLsSs+n/ZTX5lFxhO/7gEIalkRc4XdN8aJ86mK0Q7IZNlR4+2oe"
      "045FZ9vKRNYcURIvIjHeZGm2GzmiGre8jBQL36Sdhe5So+9rCvzh7kmhz05joMGVVl4We5G7"
      "QMUiednl1+hum18q8qXjXOKsli4lEw50jKVYJJIjZZMP3qYojOEa8QBK6VyXv1Hs8yignpER"
      "GdB1LiRxb2YM3D5LUQ23IAfb1reykwYsHyB0pcW4bdiQPKGs7p6hWf5tsOPCM1FerbF+wYb2"
      "PHNZcg/RZYMo1dD4K+GN0KyiEKNWOu9Tncbr/8Ts1lcLcrhaJe2EonQo8dsvxLMLTqQaQguy"
      "GUp1V4hAXWN1FVeOlRZ+joC9NF+vmMq5ZuFRCQQfoy7FecRpAlMGZWzwUFH8ra7PMKwCM+ML"
      "6atdkHLAE1CAsfD+I3RiE0cuNT62nHga5QReGnCMOrT33TJ/KtbPpdULMkc79GP1IouY4XHd"
      "+fFIECia0vOo9gZmp80CzuMa0E+UXBTYiigjFNX84Ywj2v+LnzgPGQv9gb5d5jmRhp9oEKci"
      "f07t871GVrAYp3McVXX+rbIKK91rhomckZN2O+gdfvxr0eNEDhM6IH8SLpZOn8/3rdIjfM1K"
      "cDc4qhTIR2QvmhbecnJHcUzkJKoR3BuGk+IZxHYAqpv3JkNBP0Kn2wEZh6PKhiuFa+H26wKI"
      "pEYlmKT6+o33zHiOGE7GWc0nhHbGPzTZv0gkBSWd71fZKvqBJHyG0YlxLN1CE8dcNJy/gyaO"
      "nEB2+afLRpULp/cXaGvQ2DHZ"
    },
};
static const uint32_t hvs_reference_42_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_42_words_117[] = {
    0x51005807, 0x00000000, 0x4000fff0, 0x000c0018, 0x000d0019, 0x000c0000,
    0xee805000, 0xee805480, 0x00000060, 0x00000000, 0x40f55400, 0x40eaaa00,
    0x0000fff8, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_42_segments[] = {
    { 32, 11, hvs_reference_42_words_32 },
    { 117, 18, hvs_reference_42_words_117 },
};
static const HVSReferencePixel hvs_reference_42_pixels[] = {
    { 0, 0, 0xedc595 },
    { 0, 4, 0x7c87d3 },
    { 0, 5, 0xa8c46f },
    { 0, 8, 0x6bb863 },
    { 0, 11, 0x75b682 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x331a37 },
    { 3, 1, 0xba7d2f },
    { 4, 5, 0x5be055 },
    { 5, 11, 0x60a34a },
    { 5, 17, 0x000000 },
    { 6, 4, 0x87b99e },
    { 8, 0, 0x6b355a },
    { 8, 4, 0x9393a0 },
    { 8, 5, 0xcda1d1 },
    { 8, 8, 0x634d5f },
    { 8, 11, 0x74758f },
    { 8, 19, 0x000000 },
    { 8, 29, 0x000000 },
    { 9, 13, 0x000000 },
    { 11, 11, 0xb280aa },
    { 12, 22, 0x000000 },
    { 14, 0, 0x937dac },
    { 16, 0, 0xa8c773 },
    { 16, 4, 0x42a9a4 },
    { 16, 8, 0xaab376 },
    { 16, 11, 0x4199b0 },
    { 16, 16, 0x000000 },
    { 16, 24, 0x000000 },
    { 16, 31, 0x000000 },
    { 18, 1, 0xbaf482 },
    { 18, 19, 0x000000 },
    { 18, 30, 0x000000 },
    { 23, 0, 0x244885 },
    { 23, 4, 0x93b9b7 },
    { 23, 8, 0x5bce3d },
    { 23, 11, 0x8a7f3a },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x000000 },
    { 32, 16, 0x000000 },
    { 32, 24, 0x000000 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x000000 },
    { 34, 15, 0x000000 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/flip_xy_ppf.json
 * SHA256 5574cf174f362a6e2b84c6778f2d5f600383ac302438ad443f69c5f106a64792
 */
static const HVSReferenceRegion hvs_reference_43_regions[] = {
    { 0x2e804000,
      "mY4lBArEXy+iJLCtFyW/B/ianU1xeEISFE6v5rYNd1lsN8Lvs0tJU4+kDYvOBnEcaKnpf/8u"
      "BmFDSOSEMuUjd8sjl1LafS610eai7aU5CW5A19V6LDSCwkHYiR14mLFTGNu9Dk6VcGsys78S"
      "bkD5W9FTf12Y2bOvjT/+tIQ0XpOtjZnU1PvQ6yuJqyZLRU7H753oryP5bmwSfxVnxQEcmq0l"
      "qJgZSdawhOAfVOrGwf0zpSUb2KFLtdYSmH1x3B944/HiPolr+mXfLhVXypr5O9oCyFgbiHfZ"
      "mrU95xSq/1wRGCjmL3uFejLM6f9K1aoSo4T1b1SGHGiHRimMx06bPwfEnsBgNLmwHOZ5L/ii"
      "YQcO94gsf0ZLPthQM3nESNdRjtoGmuOOG8+VM/5hToBoyGRcCENzwqKMZzFmv73HbDP0+z6p"
      "VxCltsRJ4N/kTXpFTKG1Wa9MZHMCbQOTF7D+ZaRxTmflWsNvjPo0TGb2q8IwO5kp/jn2t+1e"
      "WPvbBo3A3S0OtOuu9j7U6FXzV8zR2YvWxb2Pew/Zzai0PiclKpM+RGBa1u3ZdAqNukEt6+vg"
      "DD4Sl5hubw9CSaeuqc4PSPLgtVlV2CDEGJ1Cqr2eNmi+UtNv/NQVnipvPLtjAhA7mGmskayU"
      "o78WZrUldkQFUzk0FoRuo2GJdWY6yUIBapc="
    },
};
static const uint32_t hvs_reference_43_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_43_words_99[] = {
    0x51005807, 0x80028004, 0x4000fff0, 0x00170028, 0x00080010, 0x00070000,
    0xee8041c0, 0xee804000, 0x00000040, 0x00000000, 0x40666660, 0x40590a60,
    0x40007fe6, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_43_segments[] = {
    { 32, 11, hvs_reference_43_words_32 },
    { 99, 18, hvs_reference_43_words_99 },
};
static const HVSReferencePixel hvs_reference_43_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 5, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x000000 },
    { 3, 1, 0x000000 },
    { 4, 2, 0x6e003d },
    { 4, 5, 0x2e847e },
    { 4, 9, 0x8614d8 },
    { 4, 17, 0x3abe84 },
    { 4, 24, 0x21e930 },
    { 5, 11, 0x7178ba },
    { 5, 17, 0x4b9495 },
    { 6, 4, 0x544e61 },
    { 8, 5, 0x786b69 },
    { 8, 19, 0xb36abf },
    { 8, 29, 0x000000 },
    { 9, 13, 0x689d60 },
    { 11, 11, 0x626a6e },
    { 12, 22, 0xb777a1 },
    { 14, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x81ef56 },
    { 16, 16, 0x9ce06b },
    { 16, 24, 0x286ba8 },
    { 16, 31, 0x000000 },
    { 17, 2, 0xb16721 },
    { 17, 9, 0x4ae68c },
    { 17, 17, 0x53d66d },
    { 17, 24, 0x109c94 },
    { 18, 1, 0x000000 },
    { 18, 19, 0x81a443 },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 30, 2, 0x45b7f5 },
    { 30, 9, 0x766496 },
    { 30, 17, 0x8fcc54 },
    { 30, 24, 0x548499 },
    { 32, 0, 0x000000 },
    { 32, 8, 0xb8af68 },
    { 32, 16, 0xd53de7 },
    { 32, 24, 0x9d91ea },
    { 32, 31, 0x000000 },
    { 34, 9, 0x8663c7 },
    { 34, 15, 0xe77ec5 },
    { 43, 2, 0xc2e8fa },
    { 43, 9, 0x3af8f2 },
    { 43, 17, 0xa9a4bc },
    { 43, 24, 0x218d9e },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/flip_xy_nv12_ppf.json
 * SHA256 691c16f31a5ad67efe450c899aece94189667aeec766de0794286e7918ff206b
 */
static const HVSReferenceRegion hvs_reference_44_regions[] = {
    { 0x2e805000,
      "5t/oFfKwM6IqgIz+sviJ0Q7B7GVDZEma/FbtCZ0eIxyKpWMdLtM9nbk67pWVbH3AAg2bpeOX"
      "CVDhBIqiX7WE2//7PsuFu+yyMZDFyaoJ+Ke80ie9Qw7Mk2O15WBJfhPa4an3awPftSq5T02B"
      "bIL52/9fxEvPPrB+bwa49I/wb72K/6X7Yz4dyy6F07s97J2yuTE6kO7FlcmVqmwJffjApwK8"
      "DdKbJ6W940OXDgnMUJPhYwS1iuWiYF9JtX6EE9va"
    },
};
static const uint32_t hvs_reference_44_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_44_words_156[] = {
    0x5a001809, 0x80028004, 0x4000fff0, 0x00170028, 0x00080010, 0x00070003,
    0xee805070, 0xee8050b0, 0xee805000, 0xee805080, 0x00000010, 0x00000010,
    0x00000000, 0xf5b6d400, 0x0005e5e2, 0x00000000, 0x40333370, 0x402c8570,
    0x4000bff3, 0x40666660, 0x40590a60, 0x40007fe6, 0x00000020, 0x00000020,
    0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_44_segments[] = {
    { 32, 11, hvs_reference_44_words_32 },
    { 156, 27, hvs_reference_44_words_156 },
};
static const HVSReferencePixel hvs_reference_44_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 5, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x000000 },
    { 3, 1, 0x000000 },
    { 4, 2, 0xff75ff },
    { 4, 5, 0xff9fff },
    { 4, 9, 0xf9d496 },
    { 4, 17, 0xffa9ff },
    { 4, 24, 0xffb3ff },
    { 5, 11, 0xd6aba7 },
    { 5, 17, 0xf696ff },
    { 6, 4, 0xff68ff },
    { 8, 5, 0xffa2f4 },
    { 8, 19, 0x9f1c69 },
    { 8, 29, 0x000000 },
    { 9, 13, 0xc33368 },
    { 11, 11, 0xe73e00 },
    { 12, 22, 0xff7337 },
    { 14, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x32bd99 },
    { 16, 16, 0x78c9b0 },
    { 16, 24, 0xff82ff },
    { 16, 31, 0x000000 },
    { 17, 2, 0xa8aeff },
    { 17, 9, 0x43fffe },
    { 17, 17, 0x93deea },
    { 17, 24, 0xea5bff },
    { 18, 1, 0x000000 },
    { 18, 19, 0xc099ef },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 30, 2, 0xaa3b91 },
    { 30, 9, 0x002866 },
    { 30, 17, 0xef93e3 },
    { 30, 24, 0xafdc50 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x004752 },
    { 32, 16, 0xd773ff },
    { 32, 24, 0x93ffa9 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x8ab87a },
    { 34, 15, 0xea82eb },
    { 43, 2, 0xd2ffff },
    { 43, 9, 0xffb400 },
    { 43, 17, 0x6662c0 },
    { 43, 24, 0xff9ffa },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/yuv_NV12_mixed.json
 * SHA256 04be6ea36945ebafc55e35becad137b6af13734aacee1f532d21f117a1caae83
 */
static const HVSReferenceRegion hvs_reference_45_regions[] = {
    { 0x2e800000,
      "fHGaXLNNnAImT/DVM4OWUPpf+0kZjtsOW1IgHNsswO4R9mQ7vth3BNp8qvp4z4vqmb0kMxjZ"
      "hV7pW+aw11WVOX64o8QLPnpZYSfuYom6h4Gk6NLRbWno7Yiuexf10utVJbJ0JslU7OVREfnG"
      "0rPXIzfSAgpJp/FiCRkym+Zq4yrqaBftei+d8Vl4y3Ly0m4L/O4M1CBRpDUddN89GOEchp4G"
      "tDQm1sxsqFx6Jqpza/b8Dy4ITX1xwHlBYm6jEfhvt/7IsmrtARMAHZMuoU9Op7iirVbzN2sS"
      "CiGzcscAO3SXg7Ri1J3h/tVS7qF7CynHWDmaB41ANDQe6Kwuhw57vDwuVeO+cSvlkmxaNCIt"
      "MoPVwp1jL/a5pjAwjizx3HNK8DuT1daC+OIbmEPcwiA73uei7r/1WC1ZYbDejf7x+T6knugG"
      "0rTRNG0madbozO1siKiuXHt6Fyb1qtJz62tV9iX8sg90LiYIyU1Ufexx5cBReRFB+WLGbtKj"
      "sxHX+CNvN7fS/gLICrJJaqft8QFiEwkAGR0yk5su5qFqT+NOKqfquGiiF63tVnrzLzeda/ES"
      "WQp4IcuzcnLyx9IAbjsLdPyX7oMMtNRiINRRnaThNf4d1XRS3+49oRh74QscKYbH"
    },
};
static const uint32_t hvs_reference_45_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_45_words_108[] = {
    0x5b001909, 0x00000000, 0x4000fff0, 0x00140010, 0x00080028, 0x00070003,
    0xee800000, 0xee800140, 0xee800118, 0xee8001b8, 0x00000028, 0x00000028,
    0x00000000, 0xf5b6d400, 0x0005e5e2, 0x000000c0, 0x41400070, 0x40333370,
    0x4000bffc, 0x40666660, 0x40007ff8, 0x02800000, 0x00006666, 0x00000020,
    0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_45_segments[] = {
    { 32, 11, hvs_reference_45_words_32 },
    { 108, 28, hvs_reference_45_words_108 },
};
static const HVSReferencePixel hvs_reference_45_pixels[] = {
    { 0, 0, 0xad63be },
    { 0, 5, 0x92cbff },
    { 0, 6, 0x79d7ff },
    { 0, 8, 0x007632 },
    { 0, 13, 0x005a00 },
    { 0, 16, 0xb71eff },
    { 0, 19, 0xb801ff },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0xd09fff },
    { 3, 1, 0x006160 },
    { 4, 5, 0x186800 },
    { 5, 0, 0xe340ff },
    { 5, 6, 0xb6d8ff },
    { 5, 11, 0x4083ff },
    { 5, 13, 0x697888 },
    { 5, 17, 0xbe6d00 },
    { 5, 19, 0xffb800 },
    { 6, 4, 0x887eff },
    { 8, 5, 0xf56eff },
    { 8, 19, 0xe43197 },
    { 8, 29, 0x000000 },
    { 9, 13, 0xaa65c3 },
    { 10, 0, 0x5a0aff },
    { 10, 6, 0xff6a75 },
    { 10, 13, 0xd36c22 },
    { 10, 19, 0xffbc35 },
    { 11, 11, 0x297737 },
    { 12, 22, 0x000000 },
    { 14, 0, 0x00c9d7 },
    { 15, 0, 0x009f00 },
    { 15, 6, 0x00e5e1 },
    { 15, 13, 0x704b51 },
    { 15, 19, 0xb2b353 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x000000 },
    { 16, 16, 0x000000 },
    { 16, 24, 0x000000 },
    { 16, 31, 0x000000 },
    { 18, 1, 0x000000 },
    { 18, 19, 0x000000 },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x000000 },
    { 32, 16, 0x000000 },
    { 32, 24, 0x000000 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x000000 },
    { 34, 15, 0x000000 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/yuv_YU12_dn04.json
 * SHA256 a3e0e122c7df302c515a91e1b64d9550023bb344fa53676486b23c8faf9c2583
 */
static const HVSReferenceRegion hvs_reference_46_regions[] = {
    { 0x2e800000,
      "jgJaHtWPuhCiVMuf1wvy2Ql1bzPOoWpIxmG2l8xXNug5xfU44y5DxH9e6RPlz7TsKFggHx14"
      "M/Dx8Sn2DvB5zVFwCqC28d9P8qK8JRyAyC05yGln2XIkKhARUzhXL2pnVhLS9fEAGy3tti8l"
      "29ZbuHCC33bgCNy2ea/iRb2mKD/nLKGjk/1JIAblv1UuqizjFoaWiOpBPIsf4jx219cu1yHG"
      "9tnrYTFPLXiuL+tegX8nTjbv3ULGwnYGnhm+XqErjDpX9SufTBCeRbGbJSvB2Lvowrom6PFK"
      "gjRv9n1MCzRo3wWwxCAwCpBcN9+KDN5Fn05qmyKGEPVL6qQcitBZRGcKTDnYcgRHDADAk3H8"
      "764ChWHqIk+d0dI462+I5wQx7DrjUjMP+DMXBO/mgyCB4sCoUt9Jy67vQt47VQkiYPKq9nrb"
      "kHN8hutEfEtBAsSWaD6LVzfQQvIzIa6mFBG+BoQSbsifowY1RQ6x0D/EbLf60HV5tpy+Md8u"
      "roBUpd/pYahXuyo+c4BkygNHWUwy9JaLOCrOlpmivSCK+Rv6hq8+xI0OMuHMSB7GcP4IT5EW"
      "Dzmn+okWmwX7m6dzPkeVL+UzjBl3hbePdH/Z0FMK7GLFZoIWgsUxoPoWnkNYbY8GBfSV2avX"
      "fpSCCirQNrPWmd5ns+SNOHw34Y7RJP1eok+SnPGjTPe7JewzBBipZalVn49DKuypbkQt4cJs"
      "QX+nD7jXwgHyG0OmYO/1J5uhi/uCZDln/3UhN6psiZYmKZRJu6yzHLoX/T9WCtns7HWba8SJ"
      "3TytGOT9F6+ROB9Jn/ZUaLDXGPRfhkAOnYlSlj3jGeiwwIXFfvkRB4aE6K2JhCo4CTCPPozV"
      "MD5fAaq56YWAmyrl277UOZgneP6Haz8FAOC6ATusKBfzbMdSQxZXvekmTgRtsBDXnEiAvVrx"
      "SWRAfDjeB/aSo2hB4J9BMCAruwJorq6saCa0L4OqCKTOj6jSjQUz2CLsRU9i3rbRP4Mav1R5"
      "2tLteYxeScuHZMsa5fw6E5f+8MgKYxzJkbZgk0cpKR2nQ4cqFs4zEf6WXVqeRbGbJSvB2Lvo"
      "wrom6PFKgjRv9n1MCzRo3wWwxCAwCpBcN9+KDN5Fn05qmyKGEPVL6qQcitBZRGcKTDnYcgRH"
      "DADAk3H8764ChWHqIk+d0dI462+I5wQx7DrjUjMP+DMXBO/mgyCB4sCoUt9Jy67vQt47VQki"
      "YPKq9nrbkHN8hutEfEtBAsSWaD6LVzfQQvIzIa6mFBG+BoQSbsifowY1RQ6x0D/EbLf60HV5"
      "tpy+Md8uroBUpd/pYahXuyo+c4BkygNHWUwy9JaLOCrOlpmivSCK+Rv6hq8+xI0OMuHMSB7G"
      "cP4IT5EWDzmn+okWmwX7m6dzPkeVL+UzjBl3hbePdH/Z0FMK7GLFZoIWgsUxoPoWnkNYbY8G"
      "BfSV2avXfpSCCirQNrPWmd5ns+SNOHw34Y7RJP1eok+SnPGjTPe7JewzBBipZalVn49DKuyp"
      "bkQt4cJsQX+nD7jXwgHyG0OmYO/1J5uhi/uCZDln/3UhN6psiZYmKZRJu6yzHLoX/T9WCtns"
      "7HWba8SJ3TytGOT9"
    },
};
static const uint32_t hvs_reference_46_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_46_words_174[] = {
    0x5f001b08, 0x00000000, 0x4000fff0, 0x00080010, 0x00140028, 0x00130009,
    0xee800000, 0xee800320, 0xee8003e8, 0xee8002f8, 0xee8003d4, 0xee80049c,
    0x00000028, 0x00000014, 0x00000014, 0x00000000, 0xf5b6d400, 0x0005e5e2,
    0x000000c0, 0x41400070, 0x41400070, 0x8001c000, 0x02800000, 0x00006666,
    0x02800000, 0x00006666, 0x00000000, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_46_segments[] = {
    { 32, 11, hvs_reference_46_words_32 },
    { 174, 32, hvs_reference_46_words_174 },
};
static const HVSReferencePixel hvs_reference_46_pixels[] = {
    { 0, 0, 0x905aa9 },
    { 0, 2, 0xfa47df },
    { 0, 5, 0xff34b0 },
    { 0, 7, 0xad6818 },
    { 0, 8, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x7386d8 },
    { 3, 1, 0x009000 },
    { 4, 5, 0x627152 },
    { 5, 0, 0x8e4de7 },
    { 5, 2, 0xada34d },
    { 5, 5, 0xb96d83 },
    { 5, 7, 0xb4ac79 },
    { 5, 11, 0x000000 },
    { 5, 17, 0x000000 },
    { 6, 4, 0x6f90d5 },
    { 8, 5, 0x319462 },
    { 8, 19, 0x000000 },
    { 8, 29, 0x000000 },
    { 9, 13, 0x000000 },
    { 10, 0, 0x358926 },
    { 10, 2, 0x976f7b },
    { 10, 5, 0xff7000 },
    { 10, 7, 0x3f5987 },
    { 11, 11, 0x000000 },
    { 12, 22, 0x000000 },
    { 14, 0, 0xbb4900 },
    { 15, 0, 0x2798f5 },
    { 15, 2, 0xce7858 },
    { 15, 5, 0xd4ae8e },
    { 15, 7, 0xc659b8 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x000000 },
    { 16, 16, 0x000000 },
    { 16, 24, 0x000000 },
    { 16, 31, 0x000000 },
    { 18, 1, 0x000000 },
    { 18, 19, 0x000000 },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x000000 },
    { 32, 16, 0x000000 },
    { 32, 24, 0x000000 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x000000 },
    { 34, 15, 0x000000 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/yuv_YU16_up15.json
 * SHA256 c9fb72fb5c8e7e32f4ff437c78de208b90d6710d51f358e2fb674840c1ba98ec
 */
static const HVSReferenceRegion hvs_reference_47_regions[] = {
    { 0x2e800000,
      "bW1tmpqaJCQku7u7cHBw6m1tbZqamiQkJLu7u3BwcOptbW2ampokJCS7u7twcHDqUlJSNjY2"
      "m5ub5ubme3t7y1JSUjY2Npubm+bm5nt7e8tSUlI2Njabm5vm5uZ7e3vLwcHBSEhIqKioj4+P"
      "8fHxJcHBwUhISKioqI+Pj/Hx8SXm5uZ7e3vLy+bm5nt7e8vL5ubme3t7y8vBwcFISEioqMHB"
      "wUhISKiowcHBSEhIqKiPj4/x8fElJY+Pj/Hx8SUlxsbGPT095+fGxsY9PT3n58bGxj09Pefn"
      "KSkp/Pz8kJApKSn8/PyQkCkpKfz8/JCQvb29FBQUoaG9vb0UFBShoQ=="
    },
};
static const uint32_t hvs_reference_47_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_47_words_114[] = {
    0x5d00180a, 0x00010003, 0x4000fff0, 0x000c0018, 0x00080010, 0x00070007,
    0xee800000, 0xee800080, 0xee8000c0, 0xee800070, 0xee8000b8, 0xee8000f8,
    0x00000010, 0x00000008, 0x00000008, 0x00000000, 0xf5b6d400, 0x0005e5e2,
    0x00000000, 0x40555570, 0x40aaaa70, 0x4000bff8, 0x40aaaa60, 0x40aaaa60,
    0x00017ff8, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_47_segments[] = {
    { 32, 11, hvs_reference_47_words_32 },
    { 114, 30, hvs_reference_47_words_114 },
};
static const HVSReferencePixel hvs_reference_47_pixels[] = {
    { 0, 0, 0x000000 },
    { 0, 5, 0x000000 },
    { 0, 8, 0x000000 },
    { 0, 16, 0x000000 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0x000000 },
    { 3, 1, 0xd533ff },
    { 3, 5, 0x635bff },
    { 3, 9, 0x006ec0 },
    { 3, 12, 0xff9cdf },
    { 4, 5, 0x635bff },
    { 5, 11, 0xff9add },
    { 5, 17, 0x000000 },
    { 6, 4, 0xd731ff },
    { 8, 5, 0x7c75ff },
    { 8, 19, 0x000000 },
    { 8, 29, 0x000000 },
    { 9, 13, 0x000000 },
    { 11, 1, 0xa48efd },
    { 11, 5, 0x927ac1 },
    { 11, 9, 0x412f55 },
    { 11, 11, 0x2d48c0 },
    { 11, 12, 0x2e48bc },
    { 12, 22, 0x000000 },
    { 14, 0, 0x000000 },
    { 16, 0, 0x000000 },
    { 16, 8, 0xff6c37 },
    { 16, 16, 0x000000 },
    { 16, 24, 0x000000 },
    { 16, 31, 0x000000 },
    { 18, 1, 0x5ae9b5 },
    { 18, 19, 0x000000 },
    { 18, 30, 0x000000 },
    { 19, 1, 0x7ed3c9 },
    { 19, 5, 0xefb8a8 },
    { 19, 9, 0xffbdbb },
    { 19, 12, 0x0db2ff },
    { 25, 29, 0x000000 },
    { 26, 1, 0xff94ff },
    { 26, 5, 0xffa9ff },
    { 26, 9, 0xdeb3eb },
    { 26, 12, 0x6d3700 },
    { 30, 1, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x000000 },
    { 32, 16, 0x000000 },
    { 32, 24, 0x000000 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x000000 },
    { 34, 15, 0x000000 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/yuv_YU24_mixed.json
 * SHA256 31f9b58f24e826cfa431e730731d1b454a63cb50fe8ca03921491d6fd486a82b
 */
static const HVSReferenceRegion hvs_reference_48_regions[] = {
    { 0x2e800000,
      "fHGaXLNNnAImT/DVM4OWUPpf+0kZjtsOW1IgHNsswO4R9mQ7vth3BNp8qvp4z4vqmb0kMxjZ"
      "hV7pW+aw11WVOX64o8QLPnpZYSfuYom6h4Gk6NLRbWno7Yiuexf10utVJbJ0JslU7OVREfnG"
      "0rPXIzfSAgpJp/FiCRkym+Zq4yrqaBftei+d8Vl4y3Ly0m4L/O4M1CBRpDUddN89GOEchp4G"
      "tDQm1sxsqFx6Jqpza/b8Dy4ITX1xwHlBYm6jEfhvt/7IsmrtARMAHZMuoU9Op7iirVbzN2sS"
      "CiGzcscAO3SXg7Ri1J3h/tVS7qF7CynHWDmaB41ANDQe6Kwuhw57vDwuVeO+cSvlkmxaNCIt"
      "MoPVwp1jL/a5pjAwjizx3HNK8DuT1daC+OIbmEPcwiA73uei7r/1WC1ZYbDejf7x+T7tQAsf"
      "cx3ySG31Uk+w+RqyukPz5lrLKEFaygNCTgZ5YKihaw+rWI0XEkk8CrLLB9lpL29X8lT7mEwx"
      "21KRe9ByAqpe3DYakqbSNHgBHcjKwJmFzJvocnrb4Mk8NI2QY900sYFbDAAWpZjb5QDcOOJD"
      "JfJESURUumL2mwPQ6Oy1OxK4lVOqbdT72wJ7ZvJhfstXNggwq4/QbSfATDT6/AbCrLXw0oo9"
      "y8luNGejRTgCGmiT6yXXaGFpqobI0vOLYG4DbppvwClZdQ+kYj62Hj4I3V+7S00cA50OyZJz"
      "i0Gq/XG/WGNoLO2SbAoMalPnBobROGmZzRgt8yCw6G0JLEmqcKAcL0duBNr1JACwCZMgUAX5"
      "jZjIL0mDN3A5Xvw4VXZwVZ3Uk7uLeN7Yn+2zowZQ/x74AIb40vL/yWsv/zJo2AhprH3ULDPl"
      "9wR7M1mVFiv2Zugn1C1pJG9+vQGRPySUnaGnLn3pOsN6BDgqHb5rHu6di0sa9Im2GLYj4lU/"
      "PCkHEfLFLYB+v2wvBJInneWrCOYGpHLroP5NFTmiYqd6EbPh0Upw7kmhEyPvWUIorrYhtPjz"
      "ho+H1AGTBKXnH4nF4SY+U5GHNT5FSzaDwaTleQHZpRy+zjvmojhNLJ+dARh4eUJNbc9O/xS7"
      "eMi1L+fFTXZ9TYJptbScYqgtXZmZ6/DZ2+uN3DiXGaE9gnpvRVPFVZUobGQ8nkXrdxUR+uV/"
      "qph1TPl4KXolojkZcdC/y9fJ83wq8i992+YxhfAzcBj4L5MLv2yk0kXslAJRj7ZxOekssJbW"
      "lFCYetfG47xcPyo2dFeuS9pQZO44zgU3k0I3hWr/TAWS9J4eYvLyHubo"
    },
};
static const uint32_t hvs_reference_48_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_48_words_144[] = {
    0x5f00192a, 0x00000000, 0x4000fff0, 0x00140010, 0x00080050, 0x00070007,
    0xee800000, 0xee800140, 0xee800280, 0xee800118, 0xee800258, 0xee800398,
    0x00000028, 0x00000028, 0x00000028, 0x00000000, 0xf5b6d400, 0x0005e5e2,
    0x000000c0, 0x40666670, 0x4000bff8, 0x02800000, 0x00006666, 0x40666660,
    0x40007ff8, 0x02800000, 0x00006666, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_48_segments[] = {
    { 32, 11, hvs_reference_48_words_32 },
    { 144, 32, hvs_reference_48_words_144 },
};
static const HVSReferencePixel hvs_reference_48_pixels[] = {
    { 0, 0, 0x359874 },
    { 0, 5, 0x8cd8ce },
    { 0, 6, 0x8bd5fd },
    { 0, 8, 0x006082 },
    { 0, 13, 0x045100 },
    { 0, 16, 0xc72b43 },
    { 0, 19, 0x643e00 },
    { 0, 21, 0x000000 },
    { 0, 24, 0x000000 },
    { 0, 31, 0x000000 },
    { 2, 4, 0xeea5c0 },
    { 3, 1, 0x532151 },
    { 4, 5, 0x255c00 },
    { 5, 0, 0x3b8f9a },
    { 5, 6, 0xdddbe7 },
    { 5, 11, 0x858500 },
    { 5, 13, 0x3c9b00 },
    { 5, 17, 0x877365 },
    { 5, 19, 0xe9aeff },
    { 6, 4, 0x829073 },
    { 8, 5, 0xaaa600 },
    { 8, 19, 0x00917e },
    { 8, 29, 0x000000 },
    { 9, 13, 0xb360de },
    { 10, 0, 0x2c3400 },
    { 10, 6, 0x9d8dbd },
    { 10, 13, 0x7d8487 },
    { 10, 19, 0xe3bfad },
    { 11, 11, 0x515db3 },
    { 12, 22, 0x000000 },
    { 14, 0, 0x7d9c4d },
    { 15, 0, 0x785604 },
    { 15, 6, 0xba9fab },
    { 15, 13, 0xb73323 },
    { 15, 19, 0xff85c4 },
    { 16, 0, 0x000000 },
    { 16, 8, 0x000000 },
    { 16, 16, 0x000000 },
    { 16, 24, 0x000000 },
    { 16, 31, 0x000000 },
    { 18, 1, 0x000000 },
    { 18, 19, 0x000000 },
    { 18, 30, 0x000000 },
    { 25, 29, 0x000000 },
    { 30, 1, 0x000000 },
    { 32, 0, 0x000000 },
    { 32, 8, 0x000000 },
    { 32, 16, 0x000000 },
    { 32, 24, 0x000000 },
    { 32, 31, 0x000000 },
    { 34, 9, 0x000000 },
    { 34, 15, 0x000000 },
    { 44, 7, 0x000000 },
    { 44, 30, 0x000000 },
    { 48, 0, 0x000000 },
    { 48, 8, 0x000000 },
    { 48, 10, 0x000000 },
    { 48, 16, 0x000000 },
    { 48, 24, 0x000000 },
    { 48, 31, 0x000000 },
    { 53, 17, 0x000000 },
    { 56, 7, 0x000000 },
    { 58, 2, 0x000000 },
    { 59, 9, 0x000000 },
    { 61, 22, 0x000000 },
    { 62, 15, 0x000000 },
    { 63, 0, 0x000000 },
    { 63, 8, 0x000000 },
    { 63, 10, 0x000000 },
    { 63, 16, 0x000000 },
    { 63, 24, 0x000000 },
    { 63, 31, 0x000000 },
};
/*
 * cap2/alpha_cov_ppf.json
 * SHA256 8501879c900c60ad4b4d41d8eb81518b5fced53f91bbb67fcafb087219c9e649
 */
static const HVSReferenceRegion hvs_reference_49_regions[] = {
    { 0x0e826000,
      "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"
    },
    { 0x2e800000,
      "+TvTC1sTT2mHaNCt3h0fYGz4VyrhHBVxLoyd+G5pPkI65AwIbgt/s5qn5svfwoT3a4FncFEO"
      "51EVjFebIL/NBqlwfus/P/4k/E+/N1Xpw/IG7arjxRxkZyG6IE+VcxSFbc21zckULoHvZE21"
      "uyO3NbCLImdzt5JhguKDvWfOt7swu0PR+ggVmaF1902ry3ctYjo1etdE6VaPfEAkg0plv8iT"
      "Hz2VKKJWTAeweTLmwy7n/ye6JZ8pQo0yu5/dMCoPukiLdz0W8k4LJEfYUhLMIP9WTzRqgmi5"
      "LoZHiUzDTpg2cDe2KALVORdSeoOZZwWtwFfx9ibMxLqi8F5xlNKG+xW36Byfgz3CPSFCtGr+"
      "CkwEwnGSZcY5sSRfBEpYEvqwerrFkoLezghRNyQwJxj8ciSc9FVyDGn/B9bsbMsDOSDF/738"
      "NR1WbF55VY5SNE1dIaCUT1Yhfml/2mBGqvUWpt2n8QgAa6yP/mMi1JIrG7oVXTFnv6+DKbWm"
      "RARaNaJlPLfLTwLw+CDmpmw8qmy7L3y2Nf64SoWplxG6Gl8FmsXFt62sGDVRPMRxMBjtCfVE"
      "R6tlF+7ZMkyPJTPnGPrpd1rsgK8TQdBFju4KIYK3KDxzmuJcPfAoeZjLkn5alT+xe5Cg52dd"
      "fqM6sXFWmNPqASs5JoNUkBe2XQcpT+32Up8="
    },
};
static const uint32_t hvs_reference_49_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_49_words_80[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00200040, 0x001f0000, 0xce826000,
    0xce827f00, 0x00000100, 0x51005807, 0x00020004, 0x10008000, 0x00170028,
    0x00080010, 0x00070000, 0xee800000, 0xee8001c0, 0x00000040, 0x00000000,
    0x40666660, 0x40590a60, 0x40007fe6, 0x00000020, 0x00000020, 0x00000020,
    0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_49_segments[] = {
    { 32, 11, hvs_reference_49_words_32 },
    { 80, 26, hvs_reference_49_words_80 },
};
static const HVSReferencePixel hvs_reference_49_pixels[] = {
    { 0, 0, 0xa1eaaf },
    { 0, 5, 0xb198e1 },
    { 0, 8, 0x0ac160 },
    { 0, 10, 0xfa6a0e },
    { 0, 16, 0xbf0ee4 },
    { 0, 21, 0xd7b765 },
    { 0, 24, 0xc91a20 },
    { 0, 31, 0xd6c94a },
    { 2, 4, 0x3293f1 },
    { 3, 1, 0x1321f4 },
    { 4, 2, 0x59b94b },
    { 4, 5, 0x9dc5c8 },
    { 4, 9, 0x35543f },
    { 4, 17, 0xe0774b },
    { 4, 24, 0x935bb6 },
    { 5, 11, 0x938375 },
    { 5, 17, 0xb2dbc0 },
    { 6, 4, 0x22e3c8 },
    { 8, 5, 0x4a1b8e },
    { 8, 19, 0x8a34af },
    { 8, 29, 0x6684d8 },
    { 9, 13, 0x2a272b },
    { 11, 11, 0x5c1481 },
    { 12, 22, 0xc9a974 },
    { 14, 0, 0x662edb },
    { 16, 0, 0x1ae4d4 },
    { 16, 8, 0x284573 },
    { 16, 16, 0x9133ad },
    { 16, 24, 0x7f7680 },
    { 16, 31, 0x967fcd },
    { 17, 2, 0x256f2e },
    { 17, 9, 0x86734b },
    { 17, 17, 0x5447cc },
    { 17, 24, 0x81e64f },
    { 18, 1, 0xe09eaf },
    { 18, 19, 0x707edb },
    { 18, 30, 0xf094d2 },
    { 21, 0, 0x213cad },
    { 21, 10, 0x80897d },
    { 21, 21, 0x6a4a5b },
    { 21, 31, 0x22b404 },
    { 25, 29, 0xb68bf7 },
    { 30, 1, 0xf5896e },
    { 30, 2, 0x734c8c },
    { 30, 9, 0x8b3bd0 },
    { 30, 17, 0x3445b5 },
    { 30, 24, 0xc36c9b },
    { 32, 0, 0x3e42ab },
    { 32, 8, 0xd00f99 },
    { 32, 16, 0x34941c },
    { 32, 24, 0xb5f010 },
    { 32, 31, 0x4a1630 },
    { 34, 9, 0x7964bc },
    { 34, 15, 0xf2757a },
    { 42, 0, 0x057a8c },
    { 42, 10, 0xb27fd2 },
    { 42, 21, 0x36d3c0 },
    { 42, 31, 0xe95086 },
    { 43, 2, 0x36d4d4 },
    { 43, 9, 0x86bdee },
    { 43, 17, 0x8fb16e },
    { 43, 24, 0xb2799d },
    { 44, 7, 0xa59896 },
    { 44, 30, 0x79052f },
    { 48, 0, 0xe865da },
    { 48, 8, 0xe90169 },
    { 48, 10, 0xa97bb2 },
    { 48, 16, 0x811055 },
    { 48, 24, 0x944a85 },
    { 48, 31, 0xa4ae6c },
    { 53, 17, 0x053394 },
    { 56, 7, 0x5adaf9 },
    { 58, 2, 0xddb817 },
    { 59, 9, 0x0b5040 },
    { 61, 22, 0x335551 },
    { 62, 15, 0xfe07d3 },
    { 63, 0, 0x841321 },
    { 63, 8, 0x9c40ef },
    { 63, 10, 0xe24ccb },
    { 63, 16, 0x5e16a5 },
    { 63, 21, 0xfc0a98 },
    { 63, 24, 0xa5b5a8 },
    { 63, 31, 0xea4414 },
};
/*
 * cap2/ar30_blend.json
 * SHA256 ef6cb0031d679ed35e697c6c6a34f66f2501de9f2e2990d724f297d574783a5e
 */
static const HVSReferenceRegion hvs_reference_50_regions[] = {
    { 0x2e802000,
      "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"
    },
    { 0x2e800000,
      "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"
    },
};
static const uint32_t hvs_reference_50_words_71[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00100040, 0x000f0000, 0xee802000,
    0xee802f00, 0x00000100, 0x4800d810, 0x00000000, 0x10004e20, 0x00100040,
    0x000f0000, 0xee800000, 0xee800f00, 0x00000100, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_50_segments[] = {
    { 71, 17, hvs_reference_50_words_71 },
};
static const HVSReferencePixel hvs_reference_50_pixels[] = {
    { 0, 0, 0x9a3c71 },
    { 0, 2, 0xcb69d1 },
    { 0, 4, 0x1f61c6 },
    { 0, 5, 0x4da571 },
    { 0, 8, 0x82afef },
    { 0, 10, 0x24271e },
    { 0, 12, 0x412cbc },
    { 0, 15, 0xa3a852 },
    { 2, 2, 0x3447c7 },
    { 3, 0, 0xb87884 },
    { 4, 2, 0x47a18e },
    { 5, 5, 0xdc6b5d },
    { 5, 8, 0x65433c },
    { 6, 2, 0x2fd60e },
    { 8, 2, 0x2f601d },
    { 8, 9, 0x43a879 },
    { 8, 14, 0x4a7989 },
    { 9, 6, 0x77750f },
    { 11, 5, 0xab2f9a },
    { 12, 11, 0x72473a },
    { 14, 0, 0xa0b6b7 },
    { 16, 0, 0xd32966 },
    { 16, 4, 0x2cb734 },
    { 16, 8, 0x277bd7 },
    { 16, 12, 0x2f371c },
    { 16, 15, 0x72bdaa },
    { 18, 0, 0x8d5367 },
    { 18, 9, 0x5fb071 },
    { 18, 15, 0x43f22c },
    { 21, 0, 0x0fb13d },
    { 21, 5, 0x19d431 },
    { 21, 10, 0xa6c1d5 },
    { 21, 15, 0x7d8503 },
    { 25, 14, 0x9ee824 },
    { 30, 0, 0x5319a0 },
    { 32, 0, 0xae6fce },
    { 32, 4, 0x2e5a90 },
    { 32, 8, 0x752a8d },
    { 32, 12, 0x237670 },
    { 32, 15, 0x819e24 },
    { 34, 4, 0x59bb41 },
    { 34, 7, 0x20a824 },
    { 42, 0, 0x80b865 },
    { 42, 5, 0xf5ca21 },
    { 42, 10, 0x6ab993 },
    { 42, 15, 0x50c257 },
    { 44, 3, 0x51adc8 },
    { 44, 15, 0x4eb756 },
    { 48, 0, 0xcd209a },
    { 48, 4, 0x9f4adb },
    { 48, 5, 0x82a87b },
    { 48, 8, 0x4ce5dc },
    { 48, 12, 0x698583 },
    { 48, 15, 0xe4ce70 },
    { 53, 8, 0xd98622 },
    { 56, 3, 0x4ec579 },
    { 58, 1, 0x7c5aa4 },
    { 59, 4, 0xc45a83 },
    { 61, 11, 0x878a5e },
    { 62, 7, 0x7cc266 },
    { 63, 0, 0x9cd589 },
    { 63, 4, 0xbea48b },
    { 63, 5, 0xa99458 },
    { 63, 8, 0x95548b },
    { 63, 10, 0x56e635 },
    { 63, 12, 0x4f957f },
    { 63, 15, 0xb7999b },
};
/*
 * cap2/ar15_blend.json
 * SHA256 1a0d13ad9fbf4dff3544ad7f0d9c75d6a93e1ecdcec251ea0a58b8a980731ef2
 */
static const HVSReferenceRegion hvs_reference_51_regions[] = {
    { 0x2e801000,
      "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"
    },
    { 0x2e800000,
      "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"
    },
};
static const uint32_t hvs_reference_51_words_105[] = {
    0x4800d807, 0x00000000, 0x4000fff0, 0x00100040, 0x000f0000, 0xee801000,
    0xee801f00, 0x00000100, 0x4800d803, 0x00000000, 0x10004e20, 0x00100040,
    0x000f0000, 0xee800000, 0xee800780, 0x00000080, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_51_segments[] = {
    { 105, 17, hvs_reference_51_words_105 },
};
static const HVSReferencePixel hvs_reference_51_pixels[] = {
    { 0, 0, 0xb6a27f },
    { 0, 2, 0x6a35c8 },
    { 0, 4, 0xa8fac0 },
    { 0, 5, 0xa3bd7d },
    { 0, 8, 0xc0acec },
    { 0, 10, 0x7d4f5a },
    { 0, 12, 0x66c6db },
    { 0, 15, 0x8127b6 },
    { 2, 2, 0xc63fd3 },
    { 3, 0, 0x7ea4cc },
    { 4, 2, 0x4b6565 },
    { 5, 5, 0x124cef },
    { 5, 8, 0xbf80b5 },
    { 6, 2, 0xe58441 },
    { 8, 2, 0x50df1e },
    { 8, 9, 0xa67771 },
    { 8, 14, 0xb55aed },
    { 9, 6, 0x2660dc },
    { 11, 5, 0x7b5f8c },
    { 12, 11, 0xa85177 },
    { 14, 0, 0xca7e5a },
    { 16, 0, 0x4544d0 },
    { 16, 4, 0xbe0d69 },
    { 16, 8, 0x4e7a4d },
    { 16, 12, 0x70511a },
    { 16, 15, 0xa12f9c },
    { 18, 0, 0xdec587 },
    { 18, 9, 0x1c0aec },
    { 18, 15, 0xf1f4b3 },
    { 21, 0, 0x86c835 },
    { 21, 5, 0xa6b9d0 },
    { 21, 10, 0x5e4aa2 },
    { 21, 15, 0xb9c26d },
    { 25, 14, 0x0f9553 },
    { 30, 0, 0x8b963e },
    { 32, 0, 0xec22c0 },
    { 32, 4, 0x1fc86b },
    { 32, 8, 0x4b5a59 },
    { 32, 12, 0x5840a3 },
    { 32, 15, 0x9989ac },
    { 34, 4, 0x1331b9 },
    { 34, 7, 0xe01913 },
    { 42, 0, 0x3c80f9 },
    { 42, 5, 0xe23c79 },
    { 42, 10, 0xe26182 },
    { 42, 15, 0xdcc813 },
    { 44, 3, 0xdffefa },
    { 44, 15, 0xe7d46c },
    { 48, 0, 0x38d027 },
    { 48, 4, 0x5a48b8 },
    { 48, 5, 0xbb654f },
    { 48, 8, 0x2cd4ed },
    { 48, 12, 0xcf28f6 },
    { 48, 15, 0xcc5f2c },
    { 53, 8, 0x562896 },
    { 56, 3, 0xc4aabf },
    { 58, 1, 0x52f359 },
    { 59, 4, 0x302dce },
    { 61, 11, 0x25dcc2 },
    { 62, 7, 0x7b7e7d },
    { 63, 0, 0x31e663 },
    { 63, 4, 0xe3518f },
    { 63, 5, 0x83adc0 },
    { 63, 8, 0x2994dd },
    { 63, 10, 0xd1ad5f },
    { 63, 12, 0xa5692d },
    { 63, 15, 0x2b6d79 },
};
/*
 * cap2/tile_XR24_unity.json
 * SHA256 6544ecdcb34ccd283a37669a76acc2ef9f567cd833f9a31a84f46aee252fcf64
 */
static const HVSReferenceRegion hvs_reference_52_regions[] = {
    { 0x0e828000,
      "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"
      "oe+Tp+vrUFqQl4c8qhkad8f5n0Qsww=="
    },
};
static const uint32_t hvs_reference_52_words_63[] = {
    0x4830d807, 0x00000000, 0x4000fff0, 0x00400040, 0x003f0000, 0xce828000,
    0xce82bf00, 0x00000002, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_52_segments[] = {
    { 63, 9, hvs_reference_52_words_63 },
};
static const HVSReferencePixel hvs_reference_52_pixels[] = {
    { 0, 0, 0x63b9f6 },
    { 0, 11, 0x94fa7f },
    { 0, 16, 0x880825 },
    { 0, 21, 0x44706d },
    { 0, 32, 0xca6a40 },
    { 0, 42, 0x411084 },
    { 0, 48, 0xbbc5d2 },
    { 0, 63, 0xb4f0ae },
    { 2, 9, 0x81bb6a },
    { 3, 2, 0x662b58 },
    { 4, 10, 0xad767c },
    { 5, 22, 0x456fb0 },
    { 5, 34, 0x17181e },
    { 6, 9, 0xe05cf7 },
    { 8, 11, 0xb4116a },
    { 8, 38, 0x4d2a20 },
    { 8, 59, 0xed365b },
    { 9, 27, 0xc01cb6 },
    { 11, 23, 0x323c4b },
    { 12, 45, 0x3f9560 },
    { 14, 1, 0xa01756 },
    { 16, 0, 0x6dcdac },
    { 16, 16, 0xd8f5f1 },
    { 16, 32, 0x7a073b },
    { 16, 48, 0xaa8343 },
    { 16, 63, 0xa8b298 },
    { 18, 3, 0x59eea4 },
    { 18, 39, 0x7c6d46 },
    { 18, 60, 0xecdabb },
    { 21, 0, 0xe24a79 },
    { 21, 21, 0x7d3a9e },
    { 21, 42, 0xb5f98c },
    { 21, 63, 0x875cee },
    { 25, 58, 0xa742d6 },
    { 30, 2, 0xa63172 },
    { 32, 0, 0x6c9699 },
    { 32, 16, 0x03f539 },
    { 32, 32, 0x717fef },
    { 32, 48, 0xbb8846 },
    { 32, 63, 0xa8ff44 },
    { 34, 19, 0x08cc05 },
    { 34, 31, 0x17a67f },
    { 42, 0, 0xea9f46 },
    { 42, 21, 0x3bf7b4 },
    { 42, 42, 0xb2c098 },
    { 42, 63, 0xc0fe98 },
    { 44, 14, 0xfd7188 },
    { 44, 61, 0x214165 },
    { 48, 0, 0x82f9a2 },
    { 48, 16, 0xb0951f },
    { 48, 21, 0xc63af1 },
    { 48, 32, 0xb1b9c1 },
    { 48, 48, 0x1d31a9 },
    { 48, 63, 0x25f387 },
    { 53, 35, 0xdf0f7a },
    { 56, 14, 0x227535 },
    { 58, 5, 0xe1f255 },
    { 59, 19, 0x7549ef },
    { 61, 45, 0x76f58f },
    { 62, 31, 0xfb6655 },
    { 63, 0, 0x44bdf3 },
    { 63, 16, 0xa89c5e },
    { 63, 20, 0x4a05ff },
    { 63, 21, 0xc1ab89 },
    { 63, 32, 0x1f92b8 },
    { 63, 42, 0x70ff0b },
    { 63, 48, 0x671359 },
    { 63, 63, 0x2c449f },
};
/*
 * cap2/tile_crop_ppf.json
 * SHA256 850495efd7cfb2656eeaef30a75b18318a036b2755a6287bffb357725c2e54d6
 */
static const HVSReferenceRegion hvs_reference_53_regions[] = {
    { 0x0e830000,
      "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"
      "RniHC3r9+iHVOmrRiHerCHhcpBBwgxcYEsTLrDuM84GyQUEoYVxsL/EBHAoj1TghdDiW440k"
      "lg3u+m+9mkO7P4xFEnofkJd0TwHZc/btS9yqIL2qSY04uS3NJjsIh3TD42Hc6GZlV/aaA2Hy"
      "rKCt/nsdrNK0dOTckdgFUzdq8VdXlWOlRClVbVYUrHkVsfKRCfbmMyKR9Puv1uZClWVY805u"
      "HIsUhZl+sjSjbJH0xSaRNCZO5VmN6QrkwTr9/gM9WqlR1eoVSzK+3VdsyRenBuJZE4vjvw6S"
      "M6J3lAoVQB8bvaT6pwFwAl/0sDc6cZo+EN7c/y5bxv62OAIolv47EBioWKn+xL4IPp9besYQ"
      "wxFECV+Sebh7srX8AdByyR1QOCKOo083BSXlz9wvGbTE8x6ZZeYFWE0bxsHR109jPM6zj+3h"
      "CW//b882Af3PhuFVZHBXYYKUkQEnv6BTpS8OHckEUvfixcVkj/uUuvEZ6fYhaK/vYBwC+O7Y"
      "B9VaZ5+IYYxp7KLEvXzcYDTc0xyR1UKqbgxdYBxQ5FPjnqrt8qOQY7Gc+I1FXqooQGPXIXkf"
      "mAOeX9s2EttNSkMPTXqkM8Pa25lZ3RbZ7CeNpvy1DOGJbEtLGBAt5ohHoCjcMpOzeBFE4Ypc"
      "81/Hu3qfrqV7vpYgsPbvbmetLiOE6Kmr0aNfayadxZlyIdEXjXAqtrjrNKM4+qsTQXKErUHO"
      "9gl/Ow/IH3BXcve96VzJI9mj2sbZR60uHMXwObkqyZF07ROn1nudM/hNO1ZSeHw0yOlfQ/cs"
      "uN/PabPJee/84TNyk9S6jm1mZbTNLN4ikG7EcTM0aaa+LNOoYyFZp/sg8+r/gd+VUNSqRSD7"
      "Pzrwkk+TmivNTRxo8dz40HL03qRwEIxGDz9oMbneGsJvnXSTZwf/smKrl2bZN6NU3S/Y9sFc"
      "X0L0HRGEpHv9knSsY3JaAv58HP4Q+AmzmEsBa/zA+yj2roNXXuqmJgU1YM8lA+5E8B8JjeyD"
      "mdrJwpuWvqtjM559onKvPpt1pAwdXu+8LOCGEI54ADLt7tEAa7/t48D30IAthJCInrdMTJqi"
      "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"
      "OESNBh9TmigftiZX"
    },
};
static const uint32_t hvs_reference_53_words_32[] = {
    0x07ebfc00, 0x07e3edf8, 0x004805fd, 0x01dca432, 0x0355769b, 0x0001c6e3,
    0x0355769b, 0x01dca432, 0x004805fd, 0x07e3edf8, 0x07ebfc00,
};
static const uint32_t hvs_reference_53_words_81[] = {
    0x51305807, 0x00000000, 0x4000fff0, 0x005a0064, 0x00220029, 0x00210000,
    0xce836830, 0xce83aa30, 0x14016303, 0x00000000, 0x4067ae60, 0x405e9260,
    0x8000bf54, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_53_segments[] = {
    { 32, 11, hvs_reference_53_words_32 },
    { 81, 18, hvs_reference_53_words_81 },
};
static const HVSReferencePixel hvs_reference_53_pixels[] = {
    { 0, 0, 0xee6000 },
    { 0, 24, 0x2c7fdd },
    { 0, 30, 0xac7101 },
    { 0, 48, 0x30aa3a },
    { 0, 60, 0xb48f63 },
    { 0, 72, 0xe02807 },
    { 0, 89, 0x5a6d3d },
    { 0, 95, 0x000000 },
    { 1, 11, 0xdb8ef9 },
    { 4, 3, 0xb5536e },
    { 4, 44, 0xb08a88 },
    { 4, 65, 0xaa96ba },
    { 6, 44, 0x563fd9 },
    { 8, 85, 0xac5a68 },
    { 11, 34, 0x2b8932 },
    { 13, 9, 0xaa9899 },
    { 16, 82, 0xbdcf9a },
    { 16, 85, 0xae50a8 },
    { 18, 0, 0xeac9c1 },
    { 19, 71, 0x44b2ca },
    { 21, 12, 0x78af8c },
    { 22, 65, 0xae67aa },
    { 23, 23, 0x8dde7e },
    { 28, 67, 0x2e829a },
    { 29, 1, 0xb1513e },
    { 32, 0, 0xd1b3d2 },
    { 32, 24, 0xd97c78 },
    { 32, 48, 0xc85b89 },
    { 32, 72, 0x622d9f },
    { 32, 95, 0x000000 },
    { 33, 0, 0xd791df },
    { 33, 30, 0xc756bd },
    { 33, 60, 0xc83db6 },
    { 33, 89, 0x9abc6b },
    { 51, 58, 0x48623a },
    { 55, 18, 0xa044c6 },
    { 63, 83, 0x7399b6 },
    { 64, 0, 0x785e0e },
    { 64, 24, 0x9e88aa },
    { 64, 48, 0xa86a72 },
    { 64, 72, 0x88d765 },
    { 64, 95, 0x000000 },
    { 66, 0, 0x2ea45a },
    { 66, 30, 0x8a6d49 },
    { 66, 60, 0x8d265c },
    { 66, 89, 0x894792 },
    { 77, 61, 0x8a405b },
    { 85, 18, 0x568cda },
    { 90, 30, 0x6cabb0 },
    { 90, 34, 0xad4bb6 },
    { 96, 0, 0xac64aa },
    { 96, 21, 0x71451a },
    { 96, 24, 0x386f59 },
    { 96, 48, 0x715ba0 },
    { 96, 72, 0x9a6ec0 },
    { 96, 95, 0x000000 },
    { 99, 0, 0xa06920 },
    { 99, 30, 0x6d6764 },
    { 99, 60, 0x825eb1 },
    { 99, 89, 0x80af5f },
    { 112, 14, 0x000000 },
    { 118, 19, 0x000000 },
    { 119, 8, 0x000000 },
    { 121, 84, 0x000000 },
    { 123, 78, 0x000000 },
    { 124, 31, 0x000000 },
    { 127, 0, 0x000000 },
    { 127, 20, 0x000000 },
    { 127, 24, 0x000000 },
    { 127, 48, 0x000000 },
    { 127, 72, 0x000000 },
    { 127, 95, 0x000000 },
};
/*
 * cap2/tile_flip_xy_crop.json
 * SHA256 0b44218352ce86dbdc846f3e898c273d1483692a03e17cdb9d0570c8eb026d2b
 */
static const HVSReferenceRegion hvs_reference_54_regions[] = {
    { 0x0e830000,
      "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"
      "+gbWLAtpyXa3aasg"
    },
};
static const uint32_t hvs_reference_54_words_81[] = {
    0x4830d807, 0x80008000, 0x4000fff0, 0x00280032, 0x00270000, 0xce839220,
    0xce834420, 0x1401b503, 0x80000000,
};
static const HVSReferenceSegment hvs_reference_54_segments[] = {
    { 81, 9, hvs_reference_54_words_81 },
};
static const HVSReferencePixel hvs_reference_54_pixels[] = {
    { 0, 0, 0x5fc717 },
    { 0, 13, 0x33cb89 },
    { 0, 24, 0xff1059 },
    { 0, 26, 0x5cacfa },
    { 0, 39, 0x0bff20 },
    { 0, 48, 0x000000 },
    { 0, 72, 0x000000 },
    { 0, 95, 0x000000 },
    { 1, 11, 0x8807ad },
    { 4, 3, 0x6aabce },
    { 4, 44, 0x000000 },
    { 4, 65, 0x000000 },
    { 6, 44, 0x000000 },
    { 8, 85, 0x000000 },
    { 11, 34, 0x95fcf8 },
    { 13, 9, 0x7709b9 },
    { 16, 0, 0x1ee868 },
    { 16, 13, 0x66f6fd },
    { 16, 26, 0x2ecf4f },
    { 16, 39, 0x257d6d },
    { 16, 82, 0x000000 },
    { 16, 85, 0x000000 },
    { 18, 0, 0xc90182 },
    { 19, 71, 0x000000 },
    { 21, 12, 0xdd64f5 },
    { 22, 65, 0x000000 },
    { 23, 23, 0x1b7bc4 },
    { 28, 67, 0x000000 },
    { 29, 1, 0x913fbb },
    { 32, 0, 0xeeb2d9 },
    { 32, 24, 0x8907da },
    { 32, 48, 0x000000 },
    { 32, 72, 0x000000 },
    { 32, 95, 0x000000 },
    { 33, 0, 0xc17543 },
    { 33, 13, 0xf6d381 },
    { 33, 26, 0xf8b69a },
    { 33, 39, 0xd2d4ac },
    { 49, 0, 0xe4a41c },
    { 49, 13, 0xa39bc8 },
    { 49, 26, 0x061047 },
    { 49, 39, 0xa7bf61 },
    { 51, 58, 0x000000 },
    { 55, 18, 0x000000 },
    { 63, 83, 0x000000 },
    { 64, 0, 0x000000 },
    { 64, 24, 0x000000 },
    { 64, 48, 0x000000 },
    { 64, 72, 0x000000 },
    { 64, 95, 0x000000 },
    { 77, 61, 0x000000 },
    { 85, 18, 0x000000 },
    { 90, 30, 0x000000 },
    { 90, 34, 0x000000 },
    { 96, 0, 0x000000 },
    { 96, 21, 0x000000 },
    { 96, 24, 0x000000 },
    { 96, 48, 0x000000 },
    { 96, 72, 0x000000 },
    { 96, 95, 0x000000 },
    { 112, 14, 0x000000 },
    { 118, 19, 0x000000 },
    { 119, 8, 0x000000 },
    { 121, 84, 0x000000 },
    { 123, 78, 0x000000 },
    { 124, 31, 0x000000 },
    { 127, 0, 0x000000 },
    { 127, 20, 0x000000 },
    { 127, 24, 0x000000 },
    { 127, 48, 0x000000 },
    { 127, 72, 0x000000 },
    { 127, 95, 0x000000 },
};
static const HVSReference hvs_references[] = {
    { "fmt_AB24_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_0_regions,
      hvs_reference_0_segments, hvs_reference_0_pixels },
    { "fmt_AB30_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_1_regions,
      hvs_reference_1_segments, hvs_reference_1_pixels },
    { "fmt_AR15_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_2_regions,
      hvs_reference_2_segments, hvs_reference_2_pixels },
    { "fmt_AR24_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_3_regions,
      hvs_reference_3_segments, hvs_reference_3_pixels },
    { "fmt_AR30_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_4_regions,
      hvs_reference_4_segments, hvs_reference_4_pixels },
    { "fmt_BG16_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_5_regions,
      hvs_reference_5_segments, hvs_reference_5_pixels },
    { "fmt_BG24_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_6_regions,
      hvs_reference_6_segments, hvs_reference_6_pixels },
    { "fmt_BGR8_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_7_regions,
      hvs_reference_7_segments, hvs_reference_7_pixels },
    { "fmt_NV12_unity", 32, 16, 2, 107,
      1, 2, 70, hvs_reference_8_regions,
      hvs_reference_8_segments, hvs_reference_8_pixels },
    { "fmt_NV16_unity", 32, 16, 2, 104,
      1, 2, 70, hvs_reference_9_regions,
      hvs_reference_9_segments, hvs_reference_9_pixels },
    { "fmt_NV21_unity", 32, 16, 2, 104,
      1, 2, 70, hvs_reference_10_regions,
      hvs_reference_10_segments, hvs_reference_10_pixels },
    { "fmt_NV61_unity", 32, 16, 2, 104,
      1, 2, 70, hvs_reference_11_regions,
      hvs_reference_11_segments, hvs_reference_11_pixels },
    { "fmt_RG16_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_12_regions,
      hvs_reference_12_segments, hvs_reference_12_pixels },
    { "fmt_RG24_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_13_regions,
      hvs_reference_13_segments, hvs_reference_13_pixels },
    { "fmt_RGB8_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_14_regions,
      hvs_reference_14_segments, hvs_reference_14_pixels },
    { "fmt_XB24_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_15_regions,
      hvs_reference_15_segments, hvs_reference_15_pixels },
    { "fmt_XB30_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_16_regions,
      hvs_reference_16_segments, hvs_reference_16_pixels },
    { "fmt_XR15_unity", 32, 16, 2, 72,
      1, 1, 70, hvs_reference_17_regions,
      hvs_reference_17_segments, hvs_reference_17_pixels },
    { "fmt_XR24_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_18_regions,
      hvs_reference_18_segments, hvs_reference_18_pixels },
    { "fmt_XR30_unity", 32, 16, 2, 99,
      1, 1, 70, hvs_reference_19_regions,
      hvs_reference_19_segments, hvs_reference_19_pixels },
    { "fmt_YU12_unity", 32, 16, 2, 111,
      1, 2, 70, hvs_reference_20_regions,
      hvs_reference_20_segments, hvs_reference_20_pixels },
    { "fmt_YU16_unity", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_21_regions,
      hvs_reference_21_segments, hvs_reference_21_pixels },
    { "fmt_YU24_unity", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_22_regions,
      hvs_reference_22_segments, hvs_reference_22_pixels },
    { "fmt_YV12_unity", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_23_regions,
      hvs_reference_23_segments, hvs_reference_23_pixels },
    { "fmt_YV16_unity", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_24_regions,
      hvs_reference_24_segments, hvs_reference_24_pixels },
    { "fmt_YV24_unity", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_25_regions,
      hvs_reference_25_segments, hvs_reference_25_pixels },
    { "csc_YU24_2020_full", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_26_regions,
      hvs_reference_26_segments, hvs_reference_26_pixels },
    { "csc_YU24_2020_lim", 32, 16, 2, 54,
      1, 2, 70, hvs_reference_27_regions,
      hvs_reference_27_segments, hvs_reference_27_pixels },
    { "csc_YU24_601_full", 32, 16, 2, 180,
      1, 2, 70, hvs_reference_28_regions,
      hvs_reference_28_segments, hvs_reference_28_pixels },
    { "csc_YU24_601_lim", 32, 16, 2, 154,
      1, 2, 70, hvs_reference_29_regions,
      hvs_reference_29_segments, hvs_reference_29_pixels },
    { "csc_YU24_709_full", 32, 16, 2, 110,
      1, 2, 70, hvs_reference_30_regions,
      hvs_reference_30_segments, hvs_reference_30_pixels },
    { "csc_YU24_709_lim", 32, 16, 2, 54,
      1, 2, 70, hvs_reference_31_regions,
      hvs_reference_31_segments, hvs_reference_31_pixels },
    { "ppf_2d_rand", 64, 64, 2, 104,
      1, 2, 73, hvs_reference_32_regions,
      hvs_reference_32_segments, hvs_reference_32_pixels },
    { "ppf_h_imp_8_256", 256, 4, 2, 99,
      1, 2, 60, hvs_reference_33_regions,
      hvs_reference_33_segments, hvs_reference_33_pixels },
    { "ppf_h_rand_16_11", 256, 4, 2, 99,
      1, 2, 63, hvs_reference_34_regions,
      hvs_reference_34_segments, hvs_reference_34_pixels },
    { "ppf_v_rand_8_24", 8, 64, 2, 105,
      1, 2, 66, hvs_reference_35_regions,
      hvs_reference_35_segments, hvs_reference_35_pixels },
    { "tpz_h_rand_32_16", 32, 4, 2, 87,
      1, 1, 55, hvs_reference_36_regions,
      hvs_reference_36_segments, hvs_reference_36_pixels },
    { "tpz_v_rand_32_8", 8, 32, 2, 72,
      1, 1, 64, hvs_reference_37_regions,
      hvs_reference_37_segments, hvs_reference_37_pixels },
    { "blend_none_32768", 32, 16, 2, 54,
      2, 1, 80, hvs_reference_38_regions,
      hvs_reference_38_segments, hvs_reference_38_pixels },
    { "blend_coverage_32768", 32, 16, 2, 123,
      2, 1, 80, hvs_reference_39_regions,
      hvs_reference_39_segments, hvs_reference_39_pixels },
    { "blend_premult_32768", 32, 16, 2, 54,
      2, 1, 80, hvs_reference_40_regions,
      hvs_reference_40_segments, hvs_reference_40_pixels },
    { "crop_nv12_frac", 64, 32, 2, 81,
      1, 2, 73, hvs_reference_41_regions,
      hvs_reference_41_segments, hvs_reference_41_pixels },
    { "crop_frac_ppf2", 64, 32, 2, 117,
      1, 2, 69, hvs_reference_42_regions,
      hvs_reference_42_segments, hvs_reference_42_pixels },
    { "flip_xy_ppf", 64, 32, 2, 99,
      1, 2, 73, hvs_reference_43_regions,
      hvs_reference_43_segments, hvs_reference_43_pixels },
    { "flip_xy_nv12_ppf", 64, 32, 2, 156,
      1, 2, 73, hvs_reference_44_regions,
      hvs_reference_44_segments, hvs_reference_44_pixels },
    { "yuv_NV12_mixed", 64, 32, 2, 108,
      1, 2, 72, hvs_reference_45_regions,
      hvs_reference_45_segments, hvs_reference_45_pixels },
    { "yuv_YU12_dn04", 64, 32, 2, 174,
      1, 2, 71, hvs_reference_46_regions,
      hvs_reference_46_segments, hvs_reference_46_pixels },
    { "yuv_YU16_up15", 64, 32, 2, 114,
      1, 2, 72, hvs_reference_47_regions,
      hvs_reference_47_segments, hvs_reference_47_pixels },
    { "yuv_YU24_mixed", 64, 32, 2, 144,
      1, 2, 72, hvs_reference_48_regions,
      hvs_reference_48_segments, hvs_reference_48_pixels },
    { "alpha_cov_ppf", 64, 32, 2, 80,
      2, 2, 83, hvs_reference_49_regions,
      hvs_reference_49_segments, hvs_reference_49_pixels },
    { "ar30_blend", 64, 16, 2, 71,
      2, 1, 67, hvs_reference_50_regions,
      hvs_reference_50_segments, hvs_reference_50_pixels },
    { "ar15_blend", 64, 16, 2, 105,
      2, 1, 67, hvs_reference_51_regions,
      hvs_reference_51_segments, hvs_reference_51_pixels },
    { "tile_XR24_unity", 64, 64, 2, 63,
      1, 1, 68, hvs_reference_52_regions,
      hvs_reference_52_segments, hvs_reference_52_pixels },
    { "tile_crop_ppf", 128, 96, 2, 81,
      1, 2, 72, hvs_reference_53_regions,
      hvs_reference_53_segments, hvs_reference_53_pixels },
    { "tile_flip_xy_crop", 128, 96, 2, 81,
      1, 1, 72, hvs_reference_54_regions,
      hvs_reference_54_segments, hvs_reference_54_pixels },
};
