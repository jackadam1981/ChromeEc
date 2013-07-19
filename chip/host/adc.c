/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ADC module for emulator */

#include "adc.h"
#include "common.h"

test_mockable int adc_read_channel(enum adc_channel ch)
{
	return 0;
}
