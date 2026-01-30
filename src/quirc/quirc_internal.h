/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef QUIRC_INTERNAL_H_
#define QUIRC_INTERNAL_H_

#include <stdint.h>
#include "quirc.h"

#define QUIRC_PIXEL_WHITE   0
#define QUIRC_PIXEL_BLACK   1
#define QUIRC_PIXEL_REGION  2

#ifndef QUIRC_MAX_REGIONS
#define QUIRC_MAX_REGIONS   254
#endif

#if QUIRC_MAX_REGIONS < UINT8_MAX
#define QUIRC_REGION_MAX    QUIRC_MAX_REGIONS
typedef uint8_t quirc_pixel_t;
#elif QUIRC_MAX_REGIONS < UINT16_MAX
#define QUIRC_REGION_MAX    QUIRC_MAX_REGIONS
typedef uint16_t quirc_pixel_t;
#else
#error "QUIRC_MAX_REGIONS > 65534 is not supported"
#endif

#ifdef QUIRC_FLOAT_TYPE
typedef QUIRC_FLOAT_TYPE quirc_float_t;
#else
typedef double quirc_float_t;
#endif

struct quirc_region {
    struct quirc_point seed;
    int count;
    int capstone;
};

struct quirc_capstone {
    int ring;
    int stone;

    struct quirc_point corners[4];
    struct quirc_point center;
    quirc_float_t c[8];

    int qr_grid;
};

struct quirc_grid {
    int caps[3];

    int align_region;
    struct quirc_point align;

    quirc_float_t tpep[3];
    int hscan;
    int vscan;

    int grid_size;
    quirc_float_t c[8];
};

#define QUIRC_MAX_CAPSTONES 32
#define QUIRC_MAX_GRIDS     8

struct quirc {
    uint8_t *image;
    quirc_pixel_t *pixels;
    int w;
    int h;

    int num_regions;
    struct quirc_region regions[QUIRC_MAX_REGIONS];

    int num_capstones;
    struct quirc_capstone capstones[QUIRC_MAX_CAPSTONES];

    int num_grids;
    struct quirc_grid grids[QUIRC_MAX_GRIDS];

    size_t flood_fill_work_size;
    uint8_t *flood_fill_work;
};

/* Definitions for Reed-Solomon error correction */
#define QUIRC_MAX_POLY      64

struct quirc_rs_params {
    int bs;
    int dw;
    int ns;
};

/* quirc version database */
#define QUIRC_ECC_LEVEL_M   0
#define QUIRC_ECC_LEVEL_L   1
#define QUIRC_ECC_LEVEL_H   2
#define QUIRC_ECC_LEVEL_Q   3

struct quirc_version_info {
    uint16_t data_bytes;
    uint8_t apat[7];
    struct quirc_rs_params ecc[4];
};

extern const struct quirc_version_info quirc_version_db[QUIRC_MAX_VERSION + 1];

#endif
