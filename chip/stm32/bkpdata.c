/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bkpdata.h"
#include "registers.h"
#include "task.h"

/**
 * Read backup register at specified index.
 *
 * @return The value of the register or 0 if invalid index.
 */
uint16_t bkpdata_read(enum bkpdata_index index)
{
	if (index < 0 || index >= STM32_BKP_ENTRIES)
		return 0;

	if (index & 1)
		return STM32_BKP_DATA(index >> 1) >> 16;
	else
		return STM32_BKP_DATA(index >> 1) & 0xFFFF;
}

/**
 * Write hibernate register at specified index.
 *
 * @return nonzero if error.
 */
int bkpdata_write(enum bkpdata_index index, uint16_t value)
{
	static struct mutex bkpdata_write_mutex;

	if (index < 0 || index >= STM32_BKP_ENTRIES)
		return EC_ERROR_INVAL;

	/*
	 * Two entries share a single 32-bit register, lock mutex to prevent
	 * read/mask/write races.
	 */
	mutex_lock(&bkpdata_write_mutex);
	if (index & 1) {
		uint32_t val = STM32_BKP_DATA(index >> 1);
		val = (val & 0x0000FFFF) | (value << 16);
		STM32_BKP_DATA(index >> 1) = val;
	} else {
		uint32_t val = STM32_BKP_DATA(index >> 1);
		val = (val & 0xFFFF0000) | value;
		STM32_BKP_DATA(index >> 1) = val;
	}
	mutex_unlock(&bkpdata_write_mutex);

	return EC_SUCCESS;
}