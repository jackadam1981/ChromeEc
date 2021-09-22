/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_RTC_COMMON_H
#define __CROS_EC_RTC_COMMON_H

enum bcd_mask {
	SECONDS_MASK = 0x70,
	MINUTES_MASK = 0x70,
	HOURS24_MASK = 0x30,
	DAYS_MASK    = 0x00,
	MONTHS_MASK  = 0x10,
	YEARS_MASK   = 0xf0
};

/**
 * @brief Convert a BCD value to Decimal
 *
 * @param bcd   BCD value to convert. val has the following form:
 *              bcd bits 7 to 4 - tens place
 *              bcd bits 3 to 0 - ones place
 * @param mask  BCD mask of the tens place
 * @return      Decimal value
 */
int bcd_to_dec(uint8_t bcd, enum bcd_mask mask);

/**
 * @brief Convert a Decimal to BCD
 *
 * @param val   Decimal value to convert
 * @param mask  BCD mask of the tens place
 * @return      BCD value. BCD is the following form:
 *              bcd bits 7 to 4 - tens place
 *              bcd bits 3 to 0 - ones place
 */
uint8_t dec_to_bcd(uint32_t val, enum bcd_mask mask);
#endif /* __CROS_EC_RTC_COMMON_H */
