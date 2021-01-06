/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_version.h"
#include "system.h"

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	return EC_ERROR_UNIMPLEMENTED;
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
}

const char *system_get_chip_vendor(void)
{
	return "chromeos";
}

const char *system_get_chip_name(void)
{
	return "emu";
}

const char *system_get_chip_revision(void)
{
	return "";
}

const void *__image_size;
