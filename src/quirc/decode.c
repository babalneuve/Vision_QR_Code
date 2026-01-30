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

#include <string.h>
#include <stdlib.h>
#include "quirc_internal.h"

#define MAX_POLY       64
#define MAX_MSG_SIZE   1024

/************************************************************************
 * Galois field arithmetic
 */

static const uint8_t gf_exp[256] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26,
    0x4c, 0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9,
    0x8f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x9d, 0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35,
    0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23,
    0x46, 0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0,
    0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1,
    0x5f, 0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc,
    0x65, 0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0,
    0xfd, 0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f,
    0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2,
    0xd9, 0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
    0x0d, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce,
    0x81, 0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93,
    0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc,
    0x85, 0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9,
    0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54,
    0xa8, 0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa,
    0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73,
    0xe6, 0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e,
    0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff,
    0xe3, 0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4,
    0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41,
    0x82, 0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e,
    0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6,
    0x51, 0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef,
    0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09,
    0x12, 0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5,
    0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16,
    0x2c, 0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83,
    0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01
};

static const uint8_t gf_log[256] = {
    0x00, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1a, 0xc6,
    0x03, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b,
    0x04, 0x64, 0xe0, 0x0e, 0x34, 0x8d, 0xef, 0x81,
    0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x08, 0x4c, 0x71,
    0x05, 0x8a, 0x65, 0x2f, 0xe1, 0x24, 0x0f, 0x21,
    0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45,
    0x1d, 0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9,
    0xc9, 0x9a, 0x09, 0x78, 0x4d, 0xe4, 0x72, 0xa6,
    0x06, 0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd,
    0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88,
    0x36, 0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd,
    0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40,
    0x1e, 0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e,
    0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d,
    0xca, 0x5e, 0x9b, 0x9f, 0x0a, 0x15, 0x79, 0x2b,
    0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57,
    0x07, 0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0x0d,
    0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18,
    0xe3, 0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c,
    0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e,
    0x37, 0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd,
    0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61,
    0xf2, 0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e,
    0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2,
    0x1f, 0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76,
    0xc4, 0x17, 0x49, 0xec, 0x7f, 0x0c, 0x6f, 0xf6,
    0x6c, 0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa,
    0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a,
    0xcb, 0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51,
    0x0b, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7,
    0x4f, 0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8,
    0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf
};

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (!a || !b)
        return 0;

    return gf_exp[(gf_log[a] + gf_log[b]) % 255];
}

static uint8_t gf_div(uint8_t a, uint8_t b)
{
    if (!a)
        return 0;

    return gf_exp[(gf_log[a] + 255 - gf_log[b]) % 255];
}

/************************************************************************
 * Polynomial operations
 */

static void poly_add(uint8_t *dst, const uint8_t *src, int c, int shift)
{
    int i;

    for (i = 0; i < c; i++)
        dst[i + shift] ^= src[i];
}

static uint8_t poly_eval(const uint8_t *poly, int c, uint8_t x)
{
    uint8_t result = 0;
    int i;

    for (i = 0; i < c; i++)
        result = gf_mul(result, x) ^ poly[i];

    return result;
}

/************************************************************************
 * Reed-Solomon error correction
 */

static void berlekamp_massey(const uint8_t *syndrome, int n_syn,
                             uint8_t *sigma)
{
    uint8_t T[MAX_POLY];
    uint8_t C[MAX_POLY];
    int L = 0;
    int m = 1;
    uint8_t b = 1;
    int n;

    memset(C, 0, sizeof(C));
    memset(T, 0, sizeof(T));
    C[0] = 1;

    for (n = 0; n < n_syn; n++) {
        uint8_t d = syndrome[n];
        int i;

        for (i = 1; i <= L; i++)
            d ^= gf_mul(C[i], syndrome[n - i]);

        if (!d) {
            m++;
        } else if (2 * L <= n) {
            memcpy(T, C, sizeof(T));
            poly_add(C, sigma, MAX_POLY, m);
            for (i = 0; i < MAX_POLY - m; i++)
                C[i + m] ^= gf_mul(d, gf_div(sigma[i], b));
            L = n + 1 - L;
            memcpy(sigma, T, MAX_POLY);
            b = d;
            m = 1;
        } else {
            for (i = 0; i < MAX_POLY - m; i++)
                C[i + m] ^= gf_mul(d, gf_div(sigma[i], b));
            m++;
        }
    }

    memcpy(sigma, C, MAX_POLY);
}

static int correct_block(uint8_t *data,
                         const struct quirc_rs_params *ecc)
{
    uint8_t syndrome[MAX_POLY];
    uint8_t sigma[MAX_POLY];
    int n_ecc = ecc->bs - ecc->dw;
    int i;

    /* Compute syndromes */
    memset(syndrome, 0, n_ecc);
    for (i = 0; i < n_ecc; i++)
        syndrome[i] = poly_eval(data, ecc->bs, gf_exp[i]);

    /* Check if message is already correct */
    for (i = 0; i < n_ecc; i++)
        if (syndrome[i])
            break;
    if (i == n_ecc)
        return 0;

    /* Compute error locator polynomial */
    memset(sigma, 0, sizeof(sigma));
    sigma[0] = 1;
    berlekamp_massey(syndrome, n_ecc, sigma);

    /* Find and correct errors */
    for (i = 0; i < ecc->bs; i++) {
        if (!poly_eval(sigma, n_ecc + 1, gf_exp[255 - i]))
            continue;

        /* Found an error at position i */
        /* Compute error value using Forney algorithm (simplified) */
        uint8_t err_val = syndrome[0];
        data[ecc->bs - 1 - i] ^= err_val;
    }

    return 0;
}

/************************************************************************
 * Format value decoding
 */

#define FORMAT_MAX_ERROR    3
#define FORMAT_SYNDROMES    10
#define FORMAT_BITS         15

static int format_syndromes(uint16_t u, uint8_t *s)
{
    int i;
    uint8_t nonzero = 0;

    memset(s, 0, MAX_POLY);

    for (i = 0; i < FORMAT_SYNDROMES; i++) {
        s[i] = 0;
        {
            int j;
            for (j = 0; j < FORMAT_BITS; j++)
                if (u & (1 << j))
                    s[i] ^= gf_exp[(i * j) % 255];
        }
        nonzero |= s[i];
    }

    return nonzero ? -1 : 0;
}

static const uint16_t format_masks[8] = {
    0x5412, 0x5125, 0x5e7c, 0x5b4b,
    0x45f9, 0x40ce, 0x4f97, 0x4aa0
};

static int decode_format(uint16_t format, int *level, int *mask)
{
    int best = -1;
    int best_dist = FORMAT_MAX_ERROR + 1;
    int i;

    format ^= 0x5412;

    for (i = 0; i < 32; i++) {
        uint16_t v = i << 10;
        uint16_t d;
        int dist = 0;

        /* Compute BCH syndrome */
        {
            int j;
            for (j = 14; j >= 10; j--)
                if (v & (1 << j))
                    v ^= 0x537 << (j - 10);
        }
        v |= i << 10;

        d = v ^ format;
        while (d) {
            dist++;
            d &= d - 1;
        }

        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    if (best < 0)
        return -1;

    *level = best >> 3;
    *mask = best & 7;
    return 0;
}

/************************************************************************
 * Data stream reading
 */

struct datastream {
    uint8_t raw[MAX_MSG_SIZE];
    int data_bits;
    int ptr;
    uint8_t data[MAX_MSG_SIZE];
};

static int grid_bit(const struct quirc_code *code, int x, int y)
{
    int p = y * code->size + x;
    return (code->cell_bitmap[p >> 3] >> (p & 7)) & 1;
}

static int reserved_cell(int version, int i, int j)
{
    int size = version * 4 + 17;
    int a = 6;

    /* Finder patterns and separators */
    if (i < 9 && j < 9)
        return 1;
    if (i < 9 && j >= size - 8)
        return 1;
    if (j < 9 && i >= size - 8)
        return 1;

    /* Timing patterns */
    if (i == 6 || j == 6)
        return 1;

    /* Alignment patterns (for version >= 2) */
    if (version >= 2) {
        const struct quirc_version_info *vi = &quirc_version_db[version];
        int ai;

        for (ai = 0; vi->apat[ai] && ai < 7; ai++) {
            int aj;

            for (aj = 0; vi->apat[aj] && aj < 7; aj++) {
                int di = abs(i - vi->apat[ai]);
                int dj = abs(j - vi->apat[aj]);

                if (di <= 2 && dj <= 2) {
                    /* Skip alignment patterns overlapping with finder patterns */
                    if (ai == 0 && aj == 0)
                        continue;
                    if (ai == 0 && !vi->apat[aj + 1])
                        continue;
                    if (!vi->apat[ai + 1] && aj == 0)
                        continue;
                    return 1;
                }
            }
        }
    }

    /* Version information (for version >= 7) */
    if (version >= 7) {
        if (i < 6 && j >= size - 11)
            return 1;
        if (j < 6 && i >= size - 11)
            return 1;
    }

    return 0;
}

static void read_format(const struct quirc_code *code,
                        struct quirc_data *data, int which)
{
    uint16_t format = 0;
    int i;

    if (!which) {
        /* Read format from top-left */
        for (i = 0; i < 6; i++)
            format |= grid_bit(code, i, 8) << i;
        format |= grid_bit(code, 7, 8) << 6;
        format |= grid_bit(code, 8, 8) << 7;
        format |= grid_bit(code, 8, 7) << 8;
        for (i = 9; i < 15; i++)
            format |= grid_bit(code, 8, 14 - i) << i;
    } else {
        /* Read format from top-right and bottom-left */
        for (i = 0; i < 8; i++)
            format |= grid_bit(code, 8, code->size - 1 - i) << i;
        for (i = 8; i < 15; i++)
            format |= grid_bit(code, code->size - 15 + i, 8) << i;
    }

    decode_format(format, &data->ecc_level, &data->mask);
}

static int mask_bit(int mask, int i, int j)
{
    switch (mask) {
    case 0: return !((i + j) % 2);
    case 1: return !(i % 2);
    case 2: return !(j % 3);
    case 3: return !((i + j) % 3);
    case 4: return !((i / 2 + j / 3) % 2);
    case 5: return !(((i * j) % 2) + ((i * j) % 3));
    case 6: return !((((i * j) % 2) + ((i * j) % 3)) % 2);
    case 7: return !((((i + j) % 2) + ((i * j) % 3)) % 2);
    }
    return 0;
}

static void read_data(const struct quirc_code *code,
                      struct quirc_data *data,
                      struct datastream *ds)
{
    int y = code->size - 1;
    int x = code->size - 1;
    int dir = -1;
    int version = (code->size - 17) / 4;

    ds->data_bits = 0;
    memset(ds->raw, 0, sizeof(ds->raw));

    while (x > 0) {
        /* Skip timing pattern column */
        if (x == 6)
            x--;

        if (!reserved_cell(version, y, x)) {
            int v = grid_bit(code, x, y);
            if (mask_bit(data->mask, y, x))
                v ^= 1;
            ds->raw[ds->data_bits >> 3] |= v << (7 - (ds->data_bits & 7));
            ds->data_bits++;
        }

        if (!reserved_cell(version, y, x - 1)) {
            int v = grid_bit(code, x - 1, y);
            if (mask_bit(data->mask, y, x - 1))
                v ^= 1;
            ds->raw[ds->data_bits >> 3] |= v << (7 - (ds->data_bits & 7));
            ds->data_bits++;
        }

        y += dir;
        if (y < 0 || y >= code->size) {
            dir = -dir;
            y += dir;
            x -= 2;
        }
    }
}

static int bits_remaining(struct datastream *ds)
{
    return ds->data_bits - ds->ptr;
}

static int take_bits(struct datastream *ds, int count)
{
    int ret = 0;

    while (count--) {
        if (ds->ptr >= ds->data_bits)
            return -1;
        ret = (ret << 1) | ((ds->raw[ds->ptr >> 3] >> (7 - (ds->ptr & 7))) & 1);
        ds->ptr++;
    }

    return ret;
}

/************************************************************************
 * Data decoding
 */

static int decode_numeric(struct datastream *ds,
                          struct quirc_data *data, int count)
{
    while (count >= 3) {
        int num = take_bits(ds, 10);
        if (num < 0)
            return -1;
        if (num >= 1000)
            return -1;
        data->payload[data->payload_len++] = '0' + num / 100;
        data->payload[data->payload_len++] = '0' + (num / 10) % 10;
        data->payload[data->payload_len++] = '0' + num % 10;
        count -= 3;
    }

    if (count == 2) {
        int num = take_bits(ds, 7);
        if (num < 0)
            return -1;
        if (num >= 100)
            return -1;
        data->payload[data->payload_len++] = '0' + num / 10;
        data->payload[data->payload_len++] = '0' + num % 10;
    }

    if (count == 1) {
        int num = take_bits(ds, 4);
        if (num < 0)
            return -1;
        if (num >= 10)
            return -1;
        data->payload[data->payload_len++] = '0' + num;
    }

    return 0;
}

static const char alphanumeric_map[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

static int decode_alpha(struct datastream *ds,
                        struct quirc_data *data, int count)
{
    while (count >= 2) {
        int val = take_bits(ds, 11);
        if (val < 0)
            return -1;
        if (val >= 45 * 45)
            return -1;
        data->payload[data->payload_len++] = alphanumeric_map[val / 45];
        data->payload[data->payload_len++] = alphanumeric_map[val % 45];
        count -= 2;
    }

    if (count == 1) {
        int val = take_bits(ds, 6);
        if (val < 0)
            return -1;
        if (val >= 45)
            return -1;
        data->payload[data->payload_len++] = alphanumeric_map[val];
    }

    return 0;
}

static int decode_byte(struct datastream *ds,
                       struct quirc_data *data, int count)
{
    while (count--) {
        int val = take_bits(ds, 8);
        if (val < 0)
            return -1;
        data->payload[data->payload_len++] = val;
    }

    return 0;
}

static int decode_kanji(struct datastream *ds,
                        struct quirc_data *data, int count)
{
    while (count--) {
        int d = take_bits(ds, 13);
        int w;

        if (d < 0)
            return -1;

        if (d >= 0x1f00)
            d += 0xc240;
        else
            d += 0x8140;

        w = (d >> 8) & 0xff;
        data->payload[data->payload_len++] = w;
        data->payload[data->payload_len++] = d & 0xff;
    }

    return 0;
}

static int decode_eci(struct datastream *ds, struct quirc_data *data)
{
    if (bits_remaining(ds) < 8)
        return -1;

    data->eci = take_bits(ds, 8);

    if ((data->eci & 0xc0) == 0x80) {
        if (bits_remaining(ds) < 8)
            return -1;
        data->eci = (data->eci << 8) | take_bits(ds, 8);
    } else if ((data->eci & 0xe0) == 0xc0) {
        if (bits_remaining(ds) < 16)
            return -1;
        data->eci = (data->eci << 16) | take_bits(ds, 16);
    }

    return 0;
}

static int decode_payload(struct datastream *ds, struct quirc_data *data)
{
    int version = data->version;
    int count_len_table[] = {10, 12, 14};
    int count_len;

    if (version >= 27)
        count_len = count_len_table[2];
    else if (version >= 10)
        count_len = count_len_table[1];
    else
        count_len = count_len_table[0];

    while (bits_remaining(ds) >= 4) {
        int mode = take_bits(ds, 4);
        int count;

        switch (mode) {
        case 0: /* Terminator */
            return 0;

        case 1: /* Numeric */
            if (version >= 27)
                count = take_bits(ds, 14);
            else if (version >= 10)
                count = take_bits(ds, 12);
            else
                count = take_bits(ds, 10);
            if (count < 0)
                return -1;
            if (decode_numeric(ds, data, count) < 0)
                return -1;
            break;

        case 2: /* Alphanumeric */
            if (version >= 27)
                count = take_bits(ds, 13);
            else if (version >= 10)
                count = take_bits(ds, 11);
            else
                count = take_bits(ds, 9);
            if (count < 0)
                return -1;
            if (decode_alpha(ds, data, count) < 0)
                return -1;
            break;

        case 4: /* Byte */
            if (version >= 10)
                count = take_bits(ds, 16);
            else
                count = take_bits(ds, 8);
            if (count < 0)
                return -1;
            if (decode_byte(ds, data, count) < 0)
                return -1;
            break;

        case 7: /* ECI */
            if (decode_eci(ds, data) < 0)
                return -1;
            break;

        case 8: /* Kanji */
            if (version >= 27)
                count = take_bits(ds, 12);
            else if (version >= 10)
                count = take_bits(ds, 10);
            else
                count = take_bits(ds, 8);
            if (count < 0)
                return -1;
            if (decode_kanji(ds, data, count) < 0)
                return -1;
            break;

        default:
            return -1;
        }

        if (data->data_type < mode)
            data->data_type = mode;
    }

    return 0;
}

/************************************************************************
 * Main decode function
 */

quirc_decode_error_t quirc_decode(const struct quirc_code *code,
                                  struct quirc_data *data)
{
    struct datastream ds;
    int version;

    memset(data, 0, sizeof(*data));

    if (code->size < 21 || code->size > QUIRC_MAX_GRID_SIZE ||
        ((code->size - 17) % 4))
        return QUIRC_ERROR_INVALID_GRID_SIZE;

    version = (code->size - 17) / 4;
    if (version < 1 || version > QUIRC_MAX_VERSION)
        return QUIRC_ERROR_INVALID_VERSION;

    data->version = version;

    /* Read format info */
    read_format(code, data, 0);

    /* Read raw data stream */
    memset(&ds, 0, sizeof(ds));
    read_data(code, data, &ds);

    /* Perform error correction */
    {
        const struct quirc_version_info *vi = &quirc_version_db[version];
        const struct quirc_rs_params *ecc = &vi->ecc[data->ecc_level];
        int total_bytes = vi->data_bytes;
        int ecc_bytes = ecc->bs - ecc->dw;
        int num_blocks = ecc->ns;
        int i;

        for (i = 0; i < num_blocks; i++) {
            uint8_t block[MAX_MSG_SIZE];
            int block_size = ecc->bs;
            int data_size = ecc->dw;
            int j;

            /* Extract block */
            for (j = 0; j < block_size; j++)
                block[j] = ds.raw[i + j * num_blocks];

            /* Correct errors */
            if (correct_block(block, ecc) < 0)
                return QUIRC_ERROR_DATA_ECC;

            /* Copy back */
            for (j = 0; j < data_size && i + j * num_blocks < total_bytes; j++)
                ds.data[i + j * num_blocks] = block[j];
        }
    }

    /* Copy corrected data to datastream raw buffer */
    memcpy(ds.raw, ds.data, sizeof(ds.data));
    ds.ptr = 0;

    /* Decode payload */
    if (decode_payload(&ds, data) < 0)
        return QUIRC_ERROR_DATA_UNDERFLOW;

    data->payload[data->payload_len] = 0;

    return QUIRC_SUCCESS;
}
