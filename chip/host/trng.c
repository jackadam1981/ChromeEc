
/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dummy TRNG driver for unit test.
 * The intent is to make this implementation as determinist as
 * possible for reproducibility.
 */

#include <stdint.h>
#include <stdlib.h>
// #include "common.h"

static unsigned int seed;

void init_trng(void)
{
    /* Make for repeatable tests */
    seed = 0;
    srand(seed);
}

void exit_trng(void)
{
}

void rand_bytes(void *buffer, size_t len)
{
    uint8_t *b = (uint8_t *)buffer;
    for (; len > 0; len--) {
        *b++ = (uint8_t)rand_r(&seed);
    }
}