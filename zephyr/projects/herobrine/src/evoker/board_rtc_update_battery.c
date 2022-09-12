/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Update system first used date to smart battery register
 * OptionalMfgFunction3-1 (0x3f).
 */

#include "battery.h"
#include "battery_smart.h"
#include "console.h"
#include "rtc.h"
#include <zephyr/drivers/gpio.h>

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)


enum battery_present battery_hw_present(void)
{
	const struct gpio_dt_spec *batt_pres;

	batt_pres = GPIO_DT_FROM_NODELABEL(gpio_ec_batt_pres_odl);

	/* The GPIO is low when the battery is physically present */
	return gpio_pin_get_dt(batt_pres) ? BP_NO : BP_YES;
}

int is_battery_date_need_update(void)
{
	int rv;
	int first_used_date;

	/* Should not do anything if battery not present */
	if (battery_hw_present() != BP_YES)
		return 0;

        CPRINTS("%s: get battery first used date", __func__);

        /* get battery first used date from 0x3f */
	rv = sb_read(SB_OPTIONAL_MFG_FUNC1, &first_used_date);

	if (rv) {
                CPRINTS("%s: get battery first used date fail (%d)", __func__, rv);
                return rv;
        }

        CPRINTS("%s: first used date: 0x%04x", __func__, first_used_date);

	/* check 0x3f is 0x0000 */
	return !first_used_date;
}

__override void board_system_rtc_set_value(uint32_t seconds)
{
        struct calendar_date time;
        int first_used_date;
        int rv;

        if(!is_battery_date_need_update())
                return;

        time = sec_to_date(seconds);

        CPRINTS("%s: year: %d, month: %d, day: %d, ", __func__,
                                time.year, time.month, time.day);

        /**
         *  ChromeOS defined rtc counting start from 2000,
         *  battery want a date that counting start from 1980.
         */
        first_used_date =  ((time.year + 20) << 9) | 
                        (time.month << 5) | time.day;

        CPRINTS("%s: set first used date to: 0x%04x", __func__, first_used_date);

        rv = sb_write(SB_OPTIONAL_MFG_FUNC1, first_used_date);

	if (rv) {
                CPRINTS("%s: set battery first used date fail (%d)",
                                                        __func__, rv);
                return;
        }

}
