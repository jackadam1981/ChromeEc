/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

#define EEPROM_PAGE_SIZE	8
#define EEPROM_TOTAL_SIZE	256

void flash_edid(void *u)
{
	int i, j, rv, data_is_different;
	uint8_t edid_tmp[EEPROM_PAGE_SIZE];

	while (1) {
		/* Write EDID into EEPROM */
		CPRINTS("Write EDID into EEPROM");
		for (i = 0; i < EEPROM_TOTAL_SIZE / EEPROM_PAGE_SIZE; i++) {
			do {
				rv = i2c_write_block(
					I2C_PORT_THERMAL,
					I2C_ADDR_EEPROM,
					i * EEPROM_PAGE_SIZE,
					(uint8_t *)&edid[i*EEPROM_PAGE_SIZE],
					EEPROM_PAGE_SIZE);

				/* Time interval between pages. */
				task_wait_event(5 * MSEC);
			} while (rv);
		}

		/* Verify EDID */
		CPRINTS("Verify EDID");
		data_is_different = 0;
		for (i = 0; i < EEPROM_TOTAL_SIZE / EEPROM_PAGE_SIZE; i++) {
			do {
				rv = i2c_read_block(
					I2C_PORT_THERMAL,
					I2C_ADDR_EEPROM,
					i * EEPROM_PAGE_SIZE,
					edid_tmp,
					EEPROM_PAGE_SIZE);

				/* Time interval between pages. */
				task_wait_event(5 * MSEC);

				if (rv)
					continue;

				for (j = 0; j < EEPROM_PAGE_SIZE; j++) {
					if (edid_tmp[j] !=
					    edid[i * EEPROM_PAGE_SIZE + j]) {
						data_is_different = 1;
						break;
					}
				}
			} while (rv);

			/* If verify failed, break then waiting for retry. */
			if (data_is_different)
				break;
		}

		if (data_is_different) {
			/* If verify failed, sleep 50 msec then retry. */
			CPRINTS("Verify FAILED! Retry...");
			task_wait_event(50 * MSEC);
		} else {
			/* If verify successfully. */
			CPRINTS("Verify EDID SUCCESS!");
			task_wait_event(-1);
		}
	}
}
