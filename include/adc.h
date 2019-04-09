/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ADC interface for Chrome EC */

#ifndef __CROS_EC_ADC_H
#define __CROS_EC_ADC_H

#include "common.h"

#define ADC_READ_ERROR -1  /* Value returned by adc_read_channel() on error */

/*
 * Boards which use the ADC interface must provide enum adc_channel in the
 * board.h file.  See chip/$CHIP/adc_chip.h for additional chip-specific
 * requirements.
 */

/**
 * Read an ADC channel.
 *
 * @param ch		Channel to read
 *
 * @return The scaled ADC value, or ADC_READ_ERROR if error.
 */
int adc_read_channel(enum adc_channel ch);

/**
 * Enable ADC watchdog. Note that interrupts might come in repeatedly very
 * quickly when ADC output goes out of the accepted range.
 *
 * @param ain_id	The AIN to be watched by the watchdog.
 * @param high		The high threshold that the watchdog would trigger
 *			an interrupt when exceeded.
 * @param low		The low threshold.
 *
 * @return		EC_SUCCESS, or non-zero if any error.
 */
int adc_enable_watchdog(int ain_id, int high, int low);

/**
 * Disable ADC watchdog.
 *
 * @return		EC_SUCCESS, or non-zero if any error.
 */
int adc_disable_watchdog(void);

/**
 * Set the delay between ADC watchdog samples. This can be used as a trade-off
 * of power consumption and performance.
 *
 * @param delay_ms      The delay in milliseconds between two ADC watchdog
 *                      samples.
 *
 * @return              EC_SUCCESS, or non-zero if any error or not supported.
 */
int adc_set_watchdog_delay(int delay_ms);

/*
 * Used to define Voltage-ID conversion table, which is used by adc_read_id.
 * For example, if you have the following table
 *
 * 	{
 * 		{ id1, th1 },
 * 		{ id2, th2 },
 * 		...
 * 	}
 *
 * adc_read_id returns:
 * 	0   <= reading < th1 --> id1
 * 	th1 <= reading < th2 --> id2
 * 	...
 *
 * Note that thresholds must increase monotonically: th1 < th2 < ... and
 * thresholds should be the highest readings of the corresponding ID.
 *
 * Thresholds are typically defined as 'median + margin' or 'median * margin':
 * 	576 + 56 OR 900 * 1.03
 *
 * If you want a strict matching, you need to define a dummy entry:
 *
 * 	{
 *		{ BATTERY_1, 576 + 56 },
 *		{ BATTERY_UNKNOWN, 900 - 56 },
 *		{ BATTERY_2, 900 + 56 },
 *	}
 *
 * Another member (e.g. margin_mv) is deliberately avoided to save ROM space.
 * If you have 16 entries, margin_mv would cost 16*4 bytes while you pay only 8
 * bytes per dummy.
 */
struct adc_to_id {
	int id;
	int threshold_mv;
};

/**
 * Read ADC and convert it to ID
 *
 * @param ch	Channel to read
 * @param table	Conversion table
 * @param size	Number of entries in <table> (Pass ARRAY_SIZE(table))
 * @return	ID or ADC_READ_ERROR.
 */
int adc_read_id(enum adc_channel ch, const struct adc_to_id *table, int size);

#endif  /* __CROS_EC_ADC_H */
