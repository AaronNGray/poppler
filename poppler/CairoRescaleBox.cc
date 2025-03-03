/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2009 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MOZILLA CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
 * SHALL MOZILLA CORPORATION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Author: Jeff Muizelaar, Mozilla Corp.
 */

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2012 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2012, 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2019 Marek Kasik <mkasik@redhat.com>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

/* This implements a box filter that supports non-integer box sizes */

#include <config.h>

#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cmath>
#include "goo/gmem.h"
#include "CairoRescaleBox.h"

/* we work in fixed point where 1. == 1 << 24 */
#define FIXED_SHIFT 24

static void downsample_row_box_filter(int start, int width, uint32_t *src, const uint32_t *src_limit, uint32_t *dest, const int coverage[], int pixel_coverage)
{
    /* we need an array of the pixel contribution of each destination pixel on the boundaries.
     * we invert the value to get the value on the other size of the box */
    /*

       value  = a * contribution * 1/box_size
       value += a * 1/box_size
       value += a * 1/box_size
       value += a * 1/box_size
       value += a * (1 - contribution) * 1/box_size
                a * (1/box_size - contribution * 1/box_size)

        box size is constant


       value = a * contribution_a * 1/box_size + b * contribution_b * 1/box_size
               contribution_b = (1 - contribution_a)
                              = (1 - contribution_a_next)
    */

    /* box size = ceil(src_width/dest_width) */
    int x = 0;

    /* skip to start */
    /* XXX: it might be possible to do this directly instead of iteratively, however
     * the iterative solution is simple */
    while (x < start && src < src_limit) {
        int box = 1 << FIXED_SHIFT;
        int start_coverage = coverage[x];
        box -= start_coverage;
        src++;
        while (box >= pixel_coverage && src < src_limit) {
            src++;
            box -= pixel_coverage;
        }
        x++;
    }

    while (x < start + width && src < src_limit) {
        uint32_t a = 0;
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        int box = 1 << FIXED_SHIFT;
        int start_coverage = coverage[x];

        a = ((*src >> 24) & 0xff) * start_coverage;
        r = ((*src >> 16) & 0xff) * start_coverage;
        g = ((*src >> 8) & 0xff) * start_coverage;
        b = ((*src >> 0) & 0xff) * start_coverage;
        src++;
        x++;
        box -= start_coverage;

        while (box >= pixel_coverage && src < src_limit) {
            a += ((*src >> 24) & 0xff) * pixel_coverage;
            r += ((*src >> 16) & 0xff) * pixel_coverage;
            g += ((*src >> 8) & 0xff) * pixel_coverage;
            b += ((*src >> 0) & 0xff) * pixel_coverage;
            src++;

            box -= pixel_coverage;
        }

        /* multiply by whatever is leftover
         * this ensures that we don't bias down.
         * i.e. start_coverage + n*pixel_coverage + box == 1 << 24 */
        if (box > 0 && src < src_limit) {
            a += ((*src >> 24) & 0xff) * box;
            r += ((*src >> 16) & 0xff) * box;
            g += ((*src >> 8) & 0xff) * box;
            b += ((*src >> 0) & 0xff) * box;
        }

        a >>= FIXED_SHIFT;
        r >>= FIXED_SHIFT;
        g >>= FIXED_SHIFT;
        b >>= FIXED_SHIFT;

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
    }
}

static void downsample_columns_box_filter(int n, int start_coverage, int pixel_coverage, uint32_t *src, uint32_t *dest)
{
    int stride = n;
    while (n--) {
        uint32_t a = 0;
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        uint32_t *column_src = src;
        int box = 1 << FIXED_SHIFT;

        a = ((*column_src >> 24) & 0xff) * start_coverage;
        r = ((*column_src >> 16) & 0xff) * start_coverage;
        g = ((*column_src >> 8) & 0xff) * start_coverage;
        b = ((*column_src >> 0) & 0xff) * start_coverage;
        column_src += stride;
        box -= start_coverage;

        while (box >= pixel_coverage) {
            a += ((*column_src >> 24) & 0xff) * pixel_coverage;
            r += ((*column_src >> 16) & 0xff) * pixel_coverage;
            g += ((*column_src >> 8) & 0xff) * pixel_coverage;
            b += ((*column_src >> 0) & 0xff) * pixel_coverage;
            column_src += stride;
            box -= pixel_coverage;
        }

        if (box > 0) {
            a += ((*column_src >> 24) & 0xff) * box;
            r += ((*column_src >> 16) & 0xff) * box;
            g += ((*column_src >> 8) & 0xff) * box;
            b += ((*column_src >> 0) & 0xff) * box;
        }

        a >>= FIXED_SHIFT;
        r >>= FIXED_SHIFT;
        g >>= FIXED_SHIFT;
        b >>= FIXED_SHIFT;

        *dest = (a << 24) | (r << 16) | (g << 8) | b;
        dest++;
        src++;
    }
}

static int compute_coverage(int coverage[], int src_length, int dest_length)
{
    int i;
    /* num = src_length/dest_length
       total = sum(pixel) / num

       pixel * 1/num == pixel * dest_length / src_length
    */
    /* the average contribution of each source pixel */
    int ratio = ((1 << 24) * (long long int)dest_length) / src_length;
    /* because ((1 << 24)*(long long int)dest_length) won't always be divisible by src_length
     * we'll need someplace to put the other bits.
     *
     * We want to ensure a + n*ratio < 1<<24
     *
     * 1<<24
     * */

    double scale = (double)src_length / dest_length;

    /* for each destination pixel compute the coverage of the left most pixel included in the box */
    /* I have a proof of this, which this margin is too narrow to contain */
    for (i = 0; i < dest_length; i++) {
        double left_side = i * scale;
        double right_side = (i + 1) * scale;
        double right_fract = right_side - floor(right_side);
        double left_fract = ceil(left_side) - left_side;
        int overage;
        /* find out how many source pixels will be used to fill the box */
        int count = floor(right_side) - ceil(left_side);
        /* what's the maximum value this expression can become?
           floor((i+1)*scale) - ceil(i*scale)

           (i+1)*scale - i*scale == scale

           since floor((i+1)*scale) <= (i+1)*scale
           and   ceil(i*scale)      >= i*scale

           floor((i+1)*scale) - ceil(i*scale) <= scale

           further since: floor((i+1)*scale) - ceil(i*scale) is an integer

           therefore:
           floor((i+1)*scale) - ceil(i*scale) <= floor(scale)
        */

        if (left_fract == 0.) {
            count--;
        }

        /* compute how much the right-most pixel contributes */
        overage = ratio * (right_fract);

        /* the remainder is the amount that the left-most pixel
         * contributes */
        coverage[i] = (1 << 24) - (count * ratio + overage);
    }

    return ratio;
}

// start_column and start_row must be 0-indexed and based on scaled_width and scaled_height respectively.
bool CairoRescaleBox::downScaleImage(unsigned orig_width, unsigned orig_height, signed scaled_width, signed scaled_height, unsigned short int start_column, unsigned short int start_row, unsigned short int width, unsigned short int height,
                                     cairo_surface_t *dest_surface)
{
    int pixel_coverage_x, pixel_coverage_y;
    int dest_y;
    int src_y = 0;
    uint32_t *scanline;
    int *x_coverage = nullptr;
    int *y_coverage = nullptr;
    uint32_t *temp_buf = nullptr;
    bool retval = false;
    unsigned int *dest;
    int dst_stride;

    dest = reinterpret_cast<unsigned int *>(cairo_image_surface_get_data(dest_surface));
    dst_stride = cairo_image_surface_get_stride(dest_surface);

    scanline = (uint32_t *)gmallocn(orig_width, sizeof(int));

    x_coverage = (int *)gmallocn(orig_width, sizeof(int));
    y_coverage = (int *)gmallocn(orig_height, sizeof(int));

    /* we need to allocate enough room for ceil(src_height/dest_height)+1
       Example:
       src_height = 140
       dest_height = 50
       src_height/dest_height = 2.8

       |-------------|       2.8 pixels
       |----|----|----|----| 4 pixels
       need to sample 3 pixels

       |-------------|       2.8 pixels
       |----|----|----|----| 4 pixels
       need to sample 4 pixels
    */

    temp_buf = (uint32_t *)gmallocn3((orig_height + scaled_height - 1) / scaled_height + 1, scaled_width, sizeof(uint32_t));

    if (!x_coverage || !y_coverage || !scanline || !temp_buf) {
        goto cleanup;
    }

    pixel_coverage_x = compute_coverage(x_coverage, orig_width, scaled_width);
    pixel_coverage_y = compute_coverage(y_coverage, orig_height, scaled_height);

    assert(width + start_column <= scaled_width);

    /* skip the rows at the beginning */
    for (dest_y = 0; dest_y < start_row; dest_y++) {
        int box = 1 << FIXED_SHIFT;
        int start_coverage_y = y_coverage[dest_y];
        box -= start_coverage_y;
        src_y++;
        while (box >= pixel_coverage_y) {
            box -= pixel_coverage_y;
            src_y++;
        }
    }

    for (; dest_y < start_row + height; dest_y++) {
        int columns = 0;
        int box = 1 << FIXED_SHIFT;
        int start_coverage_y = y_coverage[dest_y];

        getRow(src_y, scanline);
        downsample_row_box_filter(start_column, width, scanline, scanline + orig_width, temp_buf + width * columns, x_coverage, pixel_coverage_x);
        columns++;
        src_y++;
        box -= start_coverage_y;

        while (box >= pixel_coverage_y) {
            getRow(src_y, scanline);
            downsample_row_box_filter(start_column, width, scanline, scanline + orig_width, temp_buf + width * columns, x_coverage, pixel_coverage_x);
            columns++;
            src_y++;
            box -= pixel_coverage_y;
        }

        /* downsample any leftovers */
        if (box > 0) {
            getRow(src_y, scanline);
            downsample_row_box_filter(start_column, width, scanline, scanline + orig_width, temp_buf + width * columns, x_coverage, pixel_coverage_x);
            columns++;
        }

        /* now scale the rows we just downsampled in the y direction */
        downsample_columns_box_filter(width, start_coverage_y, pixel_coverage_y, temp_buf, dest);
        dest += dst_stride / 4;

        //        assert(width*columns <= ((orig_height + scaled_height-1)/scaled_height+1) * width);
    }
    //    assert (src_y<=orig_height);

    retval = true;

cleanup:
    free(x_coverage);
    free(y_coverage);
    free(temp_buf);
    free(scanline);

    return retval;
}

/**
 * Downscales an image using a box filter algorithm.
 * Source image must be in 8bit format (grayscale) and
 * destination image is returned in Cairo 8bit format (CAIRO_FORMAT_A8)
 *
 * @param orig_width: Original image width (source image)
 * @param orig_height: Original image height (source image)
 * @param scaled_width: Target downscaled width (dest image)
 * @param scaled_height: Target downscaled height (dest image)
 * @param start_column: Start column (1-indexed, 0 means use entire width)
 * @param end_column: End column (1-indexed, this column is included in the range)
 * @param start_row: Start row (1-indexed, 0 means use entire height)
 * @param end_row: End row (1-indexed, this row is included in the range)
 *
 * @return: A CAIRO_FORMAT_A8 surface with the downscaled image, or nullptr on error
 */
cairo_surface_t *CairoRescaleBox::downScaleImageA8(unsigned short int orig_width, unsigned short int orig_height, unsigned short int scaled_width, unsigned short int scaled_height, unsigned short int start_column,
                                                   unsigned short int end_column, unsigned short int start_row, unsigned short int end_row)
{
    cairo_surface_t *dest_surface;
    unsigned char *dest_data;
    uint8_t *source_row;
    unsigned long *accum;
    unsigned int *count;
    int dest_stride;
    unsigned short int start_col0, end_col0, start_row0, end_row0;
    unsigned short int source_width, source_height;
    double x_ratio, y_ratio;

    if (orig_width == 0 || orig_height == 0 || scaled_width >= orig_width || scaled_height >= orig_height) {
        printf("downScaleImageA8(): Bad parameters\n");
        return nullptr;
    }

    // Convert from 1-index to 0-index and set defaults
    if (start_column == 0) {
        start_col0 = 0;
        end_col0 = orig_width - 1; // set full width
    } else {
        start_col0 = start_column - 1;
        end_col0 = end_column - 1;
    }
    if (start_row == 0) {
        start_row0 = 0;
        end_row0 = orig_height - 1; // set full height
    } else {
        start_row0 = start_row - 1;
        end_row0 = end_row - 1;
    }

    if (start_col0 >= orig_width || end_col0 >= orig_width || start_row0 >= orig_height || end_row0 >= orig_height || start_col0 > end_col0 || start_row0 > end_row0) {
        printf("downScaleImageA8(): Bad cropping parameters\n");
        return nullptr;
    }

    source_width = end_col0 - start_col0 + 1;
    source_height = end_row0 - start_row0 + 1;

    dest_surface = cairo_image_surface_create(CAIRO_FORMAT_A8, scaled_width, scaled_height);

    if (cairo_surface_status(dest_surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(dest_surface);
        return nullptr;
    }

    dest_stride = cairo_image_surface_get_stride(dest_surface);
    dest_data = cairo_image_surface_get_data(dest_surface);

    // Allocate buffer for source row data (8-bit grayscale pixels)
    source_row = (uint8_t *)gmallocn(source_width, sizeof(uint8_t));
    if (!source_row) {
        cairo_surface_destroy(dest_surface);
        return nullptr;
    }

    // Use direct accumulator array - sized to match the output dimensions
    accum = (unsigned long *)calloc(scaled_width * scaled_height, sizeof(unsigned long));
    count = (unsigned int *)calloc(scaled_width * scaled_height, sizeof(unsigned int));

    if (!accum || !count) {
        free(source_row);
        gfree(accum);
        gfree(count);
        cairo_surface_destroy(dest_surface);
        return nullptr;
    }

    x_ratio = (double)source_width / scaled_width;
    y_ratio = (double)source_height / scaled_height;

    for (unsigned int src_y = start_row0; src_y <= end_row0; src_y++) {
        getRowA8(src_y, source_row, start_col0, end_col0);

        // Determine which destination rows this source row contributes to
        int dest_y_min = (int)((src_y - start_row0) / y_ratio);
        int dest_y_max = (int)((src_y + 1 - start_row0) / y_ratio);

        dest_y_min = (dest_y_min < 0) ? 0 : (dest_y_min >= scaled_height ? scaled_height - 1 : dest_y_min);
        dest_y_max = (dest_y_max < 0) ? 0 : (dest_y_max >= scaled_height ? scaled_height - 1 : dest_y_max);

        for (unsigned int src_x = 0; src_x < source_width; src_x++) {
            uint8_t gray_value = source_row[src_x];

            // Determine which destination columns this source column contributes to
            int dest_x_min = (int)(src_x / x_ratio);
            int dest_x_max = (int)((src_x + 1) / x_ratio);

            dest_x_min = (dest_x_min < 0) ? 0 : (dest_x_min >= scaled_width ? scaled_width - 1 : dest_x_min);
            dest_x_max = (dest_x_max < 0) ? 0 : (dest_x_max >= scaled_width ? scaled_width - 1 : dest_x_max);

            // Add this pixel's contribution to all affected destination pixels
            for (int dest_y = dest_y_min; dest_y <= dest_y_max; dest_y++) {
                for (int dest_x = dest_x_min; dest_x <= dest_x_max; dest_x++) {
                    int dest_idx = dest_y * scaled_width + dest_x;
                    accum[dest_idx] += gray_value;
                    count[dest_idx]++;
                }
            }
        }
    }

    // Calculate final pixel values and set them in the destination surface
    for (int dest_y = 0; dest_y < scaled_height; dest_y++) {
        unsigned char *dest_row = dest_data + dest_y * dest_stride;

        for (int dest_x = 0; dest_x < scaled_width; dest_x++) {
            int dest_idx = dest_y * scaled_width + dest_x;
            unsigned int pixel_count = count[dest_idx];

            // Calculate average, avoiding division by zero
            dest_row[dest_x] = (pixel_count > 0) ? (unsigned char)(accum[dest_idx] / pixel_count) : 0;
        }
    }

    free(source_row);
    free(accum);
    free(count);

    cairo_surface_mark_dirty(dest_surface);

    return dest_surface;
}
