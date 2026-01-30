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

#include <stdlib.h>
#include <string.h>
#include "quirc_internal.h"

const char *quirc_version(void)
{
    return "1.2";
}

struct quirc *quirc_new(void)
{
    struct quirc *q = (struct quirc *)calloc(1, sizeof(*q));
    return q;
}

void quirc_destroy(struct quirc *q)
{
    if (q->image)
        free(q->image);
    if (q->pixels && q->pixels != (quirc_pixel_t *)q->image)
        free(q->pixels);
    if (q->flood_fill_work)
        free(q->flood_fill_work);
    free(q);
}

int quirc_resize(struct quirc *q, int w, int h)
{
    uint8_t *new_image = NULL;
    quirc_pixel_t *new_pixels = NULL;
    size_t num_pixels;
    size_t new_ff_size;

    /* Reject excessively large images */
    if (w <= 0 || h <= 0)
        return -1;

    num_pixels = (size_t)w * (size_t)h;

    /* Integer overflow check */
    if (num_pixels / (size_t)w != (size_t)h)
        return -1;

    new_image = (uint8_t *)calloc(num_pixels, sizeof(*new_image));
    if (!new_image)
        goto fail;

    if (sizeof(*new_pixels) != sizeof(*new_image)) {
        new_pixels = (quirc_pixel_t *)calloc(num_pixels, sizeof(*new_pixels));
        if (!new_pixels)
            goto fail;
    }

    /* Compute flood-fill work size */
    new_ff_size = (size_t)h * (3 * 2 + 1);
    if (new_ff_size / (size_t)h != (3 * 2 + 1))
        goto fail;

    if (new_ff_size > q->flood_fill_work_size) {
        uint8_t *new_ff = (uint8_t *)realloc(q->flood_fill_work, new_ff_size);
        if (!new_ff)
            goto fail;
        q->flood_fill_work = new_ff;
        q->flood_fill_work_size = new_ff_size;
    }

    /* Free old buffers */
    if (q->image)
        free(q->image);
    if (q->pixels && q->pixels != (quirc_pixel_t *)q->image)
        free(q->pixels);

    q->image = new_image;
    q->pixels = new_pixels ? new_pixels : (quirc_pixel_t *)new_image;
    q->w = w;
    q->h = h;

    return 0;

fail:
    if (new_image)
        free(new_image);
    if (new_pixels)
        free(new_pixels);
    return -1;
}

int quirc_count(const struct quirc *q)
{
    return q->num_grids;
}

static const char *error_table[] = {
    [QUIRC_SUCCESS] = "Success",
    [QUIRC_ERROR_INVALID_GRID_SIZE] = "Invalid grid size",
    [QUIRC_ERROR_INVALID_VERSION] = "Invalid version",
    [QUIRC_ERROR_FORMAT_ECC] = "Format data ECC failure",
    [QUIRC_ERROR_DATA_ECC] = "ECC failure",
    [QUIRC_ERROR_UNKNOWN_DATA_TYPE] = "Unknown data type",
    [QUIRC_ERROR_DATA_OVERFLOW] = "Data overflow",
    [QUIRC_ERROR_DATA_UNDERFLOW] = "Data underflow"
};

const char *quirc_strerror(quirc_decode_error_t err)
{
    if ((int)err >= 0 && (int)err < (int)(sizeof(error_table) / sizeof(error_table[0])))
        return error_table[err];

    return "Unknown error";
}
