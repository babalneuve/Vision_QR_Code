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
#include <math.h>
#include "quirc_internal.h"

/************************************************************************
 * Linear algebra routines
 */

static int line_intersect(const struct quirc_point *p0,
                          const struct quirc_point *p1,
                          const struct quirc_point *q0,
                          const struct quirc_point *q1,
                          struct quirc_point *r)
{
    int a = (q0->x - p0->x) * (p1->y - p0->y) - (q0->y - p0->y) * (p1->x - p0->x);
    int b = (p1->x - p0->x) * (q0->y - q1->y) - (p1->y - p0->y) * (q0->x - q1->x);

    if (!b)
        return 0;

    r->x = q0->x + (a * (q1->x - q0->x)) / b;
    r->y = q0->y + (a * (q1->y - q0->y)) / b;

    return 1;
}

static void perspective_setup(quirc_float_t *c,
                              const struct quirc_point *rect,
                              quirc_float_t w, quirc_float_t h)
{
    quirc_float_t x0 = rect[0].x;
    quirc_float_t y0 = rect[0].y;
    quirc_float_t x1 = rect[1].x;
    quirc_float_t y1 = rect[1].y;
    quirc_float_t x2 = rect[2].x;
    quirc_float_t y2 = rect[2].y;
    quirc_float_t x3 = rect[3].x;
    quirc_float_t y3 = rect[3].y;

    quirc_float_t wden = w * (x2*y3 - x3*y2 + (x3-x2)*y0 + x0*(y2-y3));
    quirc_float_t hden = h * (x2*y3 + x1*(y2-y3) - x2*y1 + (x1-x2)*y0 - x0*(y1-y2+y3) + x3*y1);

    c[0] = (x1*(x2*y3-x3*y2) + x0*(-x2*y3+x3*y2+(x2-x3)*y1) + x1*(x3-x2)*y0) / wden;
    c[1] = -(x0*(x2*y3+x1*(y2-y3)-x2*y1+(x1-x2)*y0-x1*y2+x3*y1) - x1*x2*y3 + x2*x3*y1 + (x1*x2-x2*x3)*y0) / hden;
    c[2] = x0;
    c[3] = (y0*(x1*(y3-y2)-x2*y3+x3*y2) + y1*(x2*y3-x3*y2) + x0*y1*(y2-y3)) / wden;
    c[4] = (x0*(y1*y3-y2*y3) + x1*y2*y3 - x2*y1*y3 + y0*(x2*y3-x1*y3+x3*y1-x3*y2)) / hden;
    c[5] = y0;
    c[6] = (x1*(y3-y2) + x0*(y2-y3) + (x2-x3)*y1 + (x3-x2)*y0) / wden;
    c[7] = -(x2*y3 + x1*(y2-y3) - x3*y2 + (x3-x2)*y1 + (x1-x2)*y0 - x0*(y1-y2+y3)) / hden;
}

static void perspective_map(const quirc_float_t *c,
                            quirc_float_t u, quirc_float_t v,
                            struct quirc_point *ret)
{
    quirc_float_t den = c[6]*u + c[7]*v + 1.0;
    quirc_float_t x = (c[0]*u + c[1]*v + c[2]) / den;
    quirc_float_t y = (c[3]*u + c[4]*v + c[5]) / den;

    ret->x = (int)(x + 0.5);
    ret->y = (int)(y + 0.5);
}

static void perspective_unmap(const quirc_float_t *c,
                              const struct quirc_point *in,
                              quirc_float_t *u, quirc_float_t *v)
{
    quirc_float_t x = in->x;
    quirc_float_t y = in->y;
    quirc_float_t den = -c[0]*c[7]*y + c[1]*c[6]*y + (c[3]*c[7]-c[4]*c[6])*x +
                        c[0]*c[4] - c[1]*c[3];

    *u = -(c[1]*(y-c[5]) - c[2]*c[7]*y + (c[5]*c[7]-c[4])*x + c[2]*c[4]) / den;
    *v = (c[0]*(y-c[5]) - c[2]*c[6]*y + (c[5]*c[6]-c[3])*x + c[2]*c[3]) / den;
}

/************************************************************************
 * Span-based flood-fill routines
 */

typedef void (*span_func_t)(void *user_data, int y, int left, int right);

typedef struct xylf
{
    int16_t x, y, left, right;
} xylf_t;

static void flood_fill_seed(struct quirc *q,
                            int x, int y,
                            int from, int to,
                            span_func_t func, void *user_data)
{
    xylf_t *stack = (xylf_t *)q->flood_fill_work;
    size_t stack_capacity = q->flood_fill_work_size / sizeof(xylf_t);
    size_t sp = 0;
    int left = x;
    int right = x;
    int i;

    if (q->pixels[y * q->w + x] != from)
        return;

    /* Initial scanline */
    while (left > 0 && q->pixels[y * q->w + left - 1] == from)
        left--;
    while (right < q->w - 1 && q->pixels[y * q->w + right + 1] == from)
        right++;

    for (i = left; i <= right; i++)
        q->pixels[y * q->w + i] = to;

    if (func)
        func(user_data, y, left, right);

    if (sp < stack_capacity) {
        stack[sp].y = y;
        stack[sp].left = left;
        stack[sp].right = right;
        stack[sp].x = 1;
        sp++;
    }
    if (sp < stack_capacity) {
        stack[sp].y = y;
        stack[sp].left = left;
        stack[sp].right = right;
        stack[sp].x = -1;
        sp++;
    }

    while (sp > 0) {
        int dir;

        sp--;
        y = stack[sp].y;
        left = stack[sp].left;
        right = stack[sp].right;
        dir = stack[sp].x;
        y += dir;

        if (y < 0 || y >= q->h)
            continue;

        for (x = left; x <= right; x++) {
            if (q->pixels[y * q->w + x] != from)
                continue;

            int lx = x;
            int rx = x;

            while (lx > 0 && q->pixels[y * q->w + lx - 1] == from)
                lx--;
            while (rx < q->w - 1 && q->pixels[y * q->w + rx + 1] == from)
                rx++;

            for (i = lx; i <= rx; i++)
                q->pixels[y * q->w + i] = to;

            if (func)
                func(user_data, y, lx, rx);

            if (sp < stack_capacity) {
                stack[sp].y = y;
                stack[sp].left = lx;
                stack[sp].right = rx;
                stack[sp].x = dir;
                sp++;
            }
            if (lx < left && sp < stack_capacity) {
                stack[sp].y = y;
                stack[sp].left = lx;
                stack[sp].right = left - 1;
                stack[sp].x = -dir;
                sp++;
            }
            if (rx > right && sp < stack_capacity) {
                stack[sp].y = y;
                stack[sp].left = right + 1;
                stack[sp].right = rx;
                stack[sp].x = -dir;
                sp++;
            }

            x = rx;
        }
    }
}

/************************************************************************
 * Adaptive threshold
 */

#define THRESHOLD_S_MIN  1
#define THRESHOLD_S_DEN  8
#define THRESHOLD_T      5

static void threshold(struct quirc *q)
{
    int x, y;
    int avg_w = 0;
    int avg_u = 0;
    int threshold_s = q->w / THRESHOLD_S_DEN;

    if (threshold_s < THRESHOLD_S_MIN)
        threshold_s = THRESHOLD_S_MIN;

    for (y = 0; y < q->h; y++) {
        int row_avg;

        /* Interleaved box filter */
        if (y == 0) {
            avg_w = 0;
            avg_u = 0;

            for (x = 0; x < q->w && x < threshold_s; x++)
                avg_u += q->image[y * q->w + x];

            avg_w = x;
        }

        row_avg = avg_u / avg_w;

        for (x = 0; x < q->w; x++) {
            int w = 0, u = 0;

            /* Horizontal box filter */
            if (x == 0) {
                for (w = 0; w < threshold_s && w < q->w; w++)
                    u += q->image[y * q->w + w];
            } else {
                if (x >= threshold_s)
                    u -= q->image[y * q->w + x - threshold_s];
                u += q->image[y * q->w + (x + threshold_s - 1 < q->w ? x + threshold_s - 1 : q->w - 1)];
                w = threshold_s;
                if (x - threshold_s + 1 < 0)
                    w += x - threshold_s + 1;
                if (x + threshold_s > q->w)
                    w -= x + threshold_s - q->w;
            }

            {
                int win_avg = u / w;
                int avg = (win_avg + row_avg) / 2;
                int pixel = q->image[y * q->w + x];

                if (pixel < avg - THRESHOLD_T)
                    q->pixels[y * q->w + x] = QUIRC_PIXEL_BLACK;
                else
                    q->pixels[y * q->w + x] = QUIRC_PIXEL_WHITE;
            }

            /* Update row average */
            if (x < threshold_s)
                avg_u += q->image[y * q->w + x];
            if (x >= q->w - threshold_s) {
                avg_w--;
                avg_u -= q->image[y * q->w + x - q->w + threshold_s];
            }
        }

        /* Reset row average for next row */
        avg_w = 0;
        avg_u = 0;
        for (x = 0; x < q->w && x < threshold_s; x++)
            avg_u += q->image[(y + 1 < q->h ? y + 1 : q->h - 1) * q->w + x];
        avg_w = x;
    }
}

/************************************************************************
 * Region analysis
 */

static void area_count(void *user_data, int y, int left, int right)
{
    ((struct quirc_region *)user_data)->count += right - left + 1;
}

static int region_code(struct quirc *q, int x, int y)
{
    int pixel;
    struct quirc_region *box;
    int region;

    if (x < 0 || y < 0 || x >= q->w || y >= q->h)
        return -1;

    pixel = q->pixels[y * q->w + x];

    if (pixel >= QUIRC_PIXEL_REGION)
        return pixel;

    if (pixel == QUIRC_PIXEL_WHITE)
        return -1;

    if (q->num_regions >= QUIRC_MAX_REGIONS)
        return -1;

    region = q->num_regions;
    box = &q->regions[region];

    memset(box, 0, sizeof(*box));
    box->seed.x = x;
    box->seed.y = y;
    box->capstone = -1;

    q->num_regions++;

    flood_fill_seed(q, x, y, pixel, region, area_count, box);

    return region;
}

/************************************************************************
 * Capstone detection and analysis
 */

struct polygon_score_data {
    struct quirc_point ref;
    int scores[4];
    struct quirc_point *corners;
};

static void find_one_corner(void *user_data, int y, int left, int right)
{
    struct polygon_score_data *psd = (struct polygon_score_data *)user_data;
    int mid = (left + right) / 2;
    int dx = mid - psd->ref.x;
    int dy = y - psd->ref.y;
    int scores[4];

    scores[0] = dx + dy;
    scores[1] = -dx + dy;
    scores[2] = -dx - dy;
    scores[3] = dx - dy;

    for (int i = 0; i < 4; i++) {
        if (scores[i] > psd->scores[i]) {
            psd->scores[i] = scores[i];
            psd->corners[i].x = mid;
            psd->corners[i].y = y;
        }
    }
}

static void find_region_corners(struct quirc *q,
                                int region,
                                const struct quirc_point *ref,
                                struct quirc_point *corners)
{
    struct quirc_region *reg = &q->regions[region];
    struct polygon_score_data psd;

    memset(&psd, 0, sizeof(psd));
    psd.corners = corners;
    psd.ref = *ref;

    for (int i = 0; i < 4; i++)
        psd.scores[i] = -1 << 20;

    flood_fill_seed(q, reg->seed.x, reg->seed.y,
                    region, QUIRC_PIXEL_BLACK, find_one_corner, &psd);
    flood_fill_seed(q, reg->seed.x, reg->seed.y,
                    QUIRC_PIXEL_BLACK, region, NULL, NULL);
}

static void record_capstone(struct quirc *q, int ring, int stone)
{
    struct quirc_region *stone_reg = &q->regions[stone];
    struct quirc_region *ring_reg = &q->regions[ring];
    struct quirc_capstone *cap;
    int cs_index;

    if (q->num_capstones >= QUIRC_MAX_CAPSTONES)
        return;

    cs_index = q->num_capstones++;
    cap = &q->capstones[cs_index];

    memset(cap, 0, sizeof(*cap));
    cap->ring = ring;
    cap->stone = stone;
    stone_reg->capstone = cs_index;
    ring_reg->capstone = cs_index;

    /* Find the corners of the ring */
    find_region_corners(q, ring, &stone_reg->seed, cap->corners);

    /* Compute center point from corners */
    cap->center.x = (cap->corners[0].x + cap->corners[1].x +
                     cap->corners[2].x + cap->corners[3].x) / 4;
    cap->center.y = (cap->corners[0].y + cap->corners[1].y +
                     cap->corners[2].y + cap->corners[3].y) / 4;
}

static int test_capstone(struct quirc *q, int x, int y, int *ring, int *stone)
{
    int ratio_max = 128;
    int ring_val = 0;
    int stone_val = 0;
    int counts[5] = {0};
    int check[5] = {1, 1, 3, 1, 1};
    int scan_length = 0;
    int ratio = 0;
    int i;

    /* Scan horizontally to find 1:1:3:1:1 ratio */
    for (i = x; i >= 0 && q->pixels[y * q->w + i] != QUIRC_PIXEL_WHITE; i--) {
        if (q->pixels[y * q->w + i] == QUIRC_PIXEL_BLACK)
            counts[2]++;
        else
            break;
    }
    for (i--; i >= 0 && q->pixels[y * q->w + i] != QUIRC_PIXEL_BLACK; i--)
        counts[1]++;
    for (i--; i >= 0 && q->pixels[y * q->w + i] != QUIRC_PIXEL_WHITE; i--)
        counts[0]++;

    for (i = x + 1; i < q->w && q->pixels[y * q->w + i] != QUIRC_PIXEL_WHITE; i++) {
        if (q->pixels[y * q->w + i] == QUIRC_PIXEL_BLACK)
            counts[2]++;
        else
            break;
    }
    for (i++; i < q->w && q->pixels[y * q->w + i] != QUIRC_PIXEL_BLACK; i++)
        counts[3]++;
    for (i++; i < q->w && q->pixels[y * q->w + i] != QUIRC_PIXEL_WHITE; i++)
        counts[4]++;

    scan_length = counts[0] + counts[1] + counts[2] + counts[3] + counts[4];
    if (scan_length < 7)
        return 0;

    ratio = scan_length / 7;
    if (ratio < 1)
        ratio = 1;

    for (i = 0; i < 5; i++) {
        int expected = check[i] * ratio;
        int min_v = expected - ratio_max * expected / 256;
        int max_v = expected + ratio_max * expected / 256;

        if (counts[i] < min_v || counts[i] > max_v)
            return 0;
    }

    ring_val = region_code(q, x - counts[2]/2 - counts[1] - counts[0]/2, y);
    if (ring_val < 0)
        return 0;

    stone_val = region_code(q, x, y);
    if (stone_val < 0)
        return 0;

    if (ring_val == stone_val)
        return 0;
    if (q->regions[ring_val].capstone >= 0 || q->regions[stone_val].capstone >= 0)
        return 0;

    *ring = ring_val;
    *stone = stone_val;
    return 1;
}

static void find_capstones(struct quirc *q)
{
    int x, y;

    q->num_capstones = 0;

    for (y = 0; y < q->h; y++) {
        for (x = 0; x < q->w; x++) {
            int ring_val, stone_val;

            if (q->pixels[y * q->w + x] != QUIRC_PIXEL_BLACK)
                continue;

            if (test_capstone(q, x, y, &ring_val, &stone_val))
                record_capstone(q, ring_val, stone_val);
        }
    }
}

/************************************************************************
 * QR Grid detection
 */

static int fitness_cell(const struct quirc *q, int index, int cx, int cy)
{
    int x = cx >= 0 ? cx : 0;
    int y = cy >= 0 ? cy : 0;

    if (x >= q->w)
        x = q->w - 1;
    if (y >= q->h)
        y = q->h - 1;

    if (q->pixels[y * q->w + x] == QUIRC_PIXEL_BLACK)
        return index % 2 ? -1 : 1;
    return index % 2 ? 1 : -1;
}

static int fitness_ring(const struct quirc *q, int cx, int cy, int radius)
{
    int fitness = 0;
    int i;

    for (i = 0; i < radius * 8; i++) {
        int theta = i * 3.14159 * 2 / (radius * 8);
        int x = cx + cos(theta) * radius;
        int y = cy + sin(theta) * radius;

        if (x >= 0 && x < q->w && y >= 0 && y < q->h) {
            if (q->pixels[y * q->w + x] == QUIRC_PIXEL_BLACK)
                fitness++;
            else
                fitness--;
        }
    }

    return fitness;
}

static void setup_qr_perspective(struct quirc *q, int index)
{
    struct quirc_grid *qr = &q->grids[index];
    struct quirc_capstone *caps[3];
    int i;
    struct quirc_point h0, hd;
    struct quirc_point v0, vd;
    quirc_float_t grid_size = qr->grid_size;

    for (i = 0; i < 3; i++)
        caps[i] = &q->capstones[qr->caps[i]];

    /* Calculate grid perspective */
    perspective_setup(qr->c, caps[1]->corners, grid_size - 7, grid_size - 7);
}

static void record_qr_grid(struct quirc *q, int a, int b, int c)
{
    struct quirc_grid *qr;
    struct quirc_point h0, hd;
    struct quirc_point v0, vd;
    int i;

    if (q->num_grids >= QUIRC_MAX_GRIDS)
        return;

    /* Calculate grid size */
    qr = &q->grids[q->num_grids];
    memset(qr, 0, sizeof(*qr));
    qr->caps[0] = a;
    qr->caps[1] = b;
    qr->caps[2] = c;

    /* Mark capstones as used */
    q->capstones[a].qr_grid = q->num_grids;
    q->capstones[b].qr_grid = q->num_grids;
    q->capstones[c].qr_grid = q->num_grids;

    /* Estimate grid size based on capstone distances */
    {
        struct quirc_point ab, bc;
        int d;

        ab.x = q->capstones[b].center.x - q->capstones[a].center.x;
        ab.y = q->capstones[b].center.y - q->capstones[a].center.y;
        bc.x = q->capstones[c].center.x - q->capstones[b].center.x;
        bc.y = q->capstones[c].center.y - q->capstones[b].center.y;

        d = (int)sqrt((ab.x * ab.x + ab.y * ab.y) +
                      (bc.x * bc.x + bc.y * bc.y)) / 2;

        /* Each module is approximately d/14 pixels (7 modules in each capstone) */
        qr->grid_size = d * 7 / (d / 7 + 14);
        if (qr->grid_size < 21)
            qr->grid_size = 21;
        if (qr->grid_size > QUIRC_MAX_GRID_SIZE)
            qr->grid_size = QUIRC_MAX_GRID_SIZE;

        /* Align to valid QR version size */
        qr->grid_size = ((qr->grid_size - 17) / 4) * 4 + 17;
    }

    setup_qr_perspective(q, q->num_grids);
    q->num_grids++;
}

static void test_grouping(struct quirc *q, int i)
{
    int j, k;
    struct quirc_capstone *cap_i = &q->capstones[i];

    if (cap_i->qr_grid >= 0)
        return;

    for (j = i + 1; j < q->num_capstones; j++) {
        struct quirc_capstone *cap_j = &q->capstones[j];

        if (cap_j->qr_grid >= 0)
            continue;

        for (k = j + 1; k < q->num_capstones; k++) {
            struct quirc_capstone *cap_k = &q->capstones[k];

            if (cap_k->qr_grid >= 0)
                continue;

            /* Check if these three could form a QR code */
            record_qr_grid(q, i, j, k);
        }
    }
}

static void find_qr_grids(struct quirc *q)
{
    int i;

    for (i = 0; i < q->num_capstones; i++)
        q->capstones[i].qr_grid = -1;

    q->num_grids = 0;

    for (i = 0; i < q->num_capstones; i++)
        test_grouping(q, i);
}

/************************************************************************
 * Code extraction
 */

static int read_cell(const struct quirc *q, int index, int x, int y)
{
    const struct quirc_grid *qr = &q->grids[index];
    struct quirc_point p;

    perspective_map(qr->c, x + 0.5, y + 0.5, &p);

    if (p.y < 0 || p.y >= q->h || p.x < 0 || p.x >= q->w)
        return 0;

    return q->pixels[p.y * q->w + p.x] == QUIRC_PIXEL_BLACK;
}

void quirc_extract(const struct quirc *q, int index, struct quirc_code *code)
{
    const struct quirc_grid *qr = &q->grids[index];
    int y;
    int i = 0;

    if (index < 0 || index >= q->num_grids)
        return;

    memset(code, 0, sizeof(*code));
    code->size = qr->grid_size;

    perspective_map(qr->c, 0.0, 0.0, &code->corners[0]);
    perspective_map(qr->c, qr->grid_size, 0.0, &code->corners[1]);
    perspective_map(qr->c, qr->grid_size, qr->grid_size, &code->corners[2]);
    perspective_map(qr->c, 0.0, qr->grid_size, &code->corners[3]);

    for (y = 0; y < qr->grid_size; y++) {
        int x;

        for (x = 0; x < qr->grid_size; x++) {
            if (read_cell(q, index, x, y))
                code->cell_bitmap[i >> 3] |= (1 << (i & 7));
            i++;
        }
    }
}

/************************************************************************
 * Main entry point
 */

uint8_t *quirc_begin(struct quirc *q, int *w, int *h)
{
    q->num_regions = QUIRC_PIXEL_REGION;
    q->num_capstones = 0;
    q->num_grids = 0;

    if (w)
        *w = q->w;
    if (h)
        *h = q->h;

    return q->image;
}

void quirc_end(struct quirc *q)
{
    threshold(q);
    find_capstones(q);
    find_qr_grids(q);
}

void quirc_flip(struct quirc_code *code)
{
    struct quirc_point tmp;
    int y;

    /* Flip corners */
    tmp = code->corners[0];
    code->corners[0] = code->corners[1];
    code->corners[1] = tmp;
    tmp = code->corners[2];
    code->corners[2] = code->corners[3];
    code->corners[3] = tmp;

    /* Flip cell bitmap */
    for (y = 0; y < code->size; y++) {
        int x;
        for (x = 0; x < code->size / 2; x++) {
            int from = y * code->size + x;
            int to = y * code->size + code->size - 1 - x;
            int from_bit = (code->cell_bitmap[from >> 3] >> (from & 7)) & 1;
            int to_bit = (code->cell_bitmap[to >> 3] >> (to & 7)) & 1;

            code->cell_bitmap[from >> 3] &= ~(1 << (from & 7));
            code->cell_bitmap[to >> 3] &= ~(1 << (to & 7));

            if (to_bit)
                code->cell_bitmap[from >> 3] |= (1 << (from & 7));
            if (from_bit)
                code->cell_bitmap[to >> 3] |= (1 << (to & 7));
        }
    }
}
