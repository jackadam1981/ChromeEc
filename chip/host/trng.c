
/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dummy TRNG driver for unit test.
 *
 * Although a TRNG is designed to be anything but predictable,
 * this implementation strives to be as predictable and defined
 * as possible to allow reproducing unit tests and fuzzer crashes.
 */

#include <stdint.h>
#include <stdlib.h>

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
    uint8_t *b, *end;
    for (b = buffer, end = b+len; b != end; b++) {
        *b = (uint8_t)rand_r(&seed);
    }
}