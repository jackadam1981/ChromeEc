/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Raw touch data recording */

#include "common.h"
#include "debug.h"
#include "touch_scan.h"
#include "util.h"

#include "registers.h"

int usb_write_raw(const uint8_t *data, int size);

void encode_reset(void)
{
}

void encode_add_column(const uint8_t *dptr)
{
	/* Pad to 64 byte for bulk transfer */
	usb_write_raw(dptr, 64);
	usb_write_raw(dptr + ROW_COUNT * 2 - 64, 64);
}

void encode_dump_matrix(void)
{
}
