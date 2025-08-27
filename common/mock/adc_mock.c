/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"

/* LCOV_EXCL_START - These mocks just avoid linker errors with stubs. */

int adc_read_channel(enum adc_channel ch)
{
	return 0;
}

/* LCOV_EXCL_STOP */
