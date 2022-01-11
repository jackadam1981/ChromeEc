/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_CHARGER_ENUM_H_
#define ZEPHYR_SHIM_INCLUDE_CHARGER_ENUM_H_

/*
 * Theoretically, this should enumerate all the chargers
 * by checking and processing each type, but practically
 * if OCPC is enabled, there are only 2 chargers.
 */
enum chg_id {
	CHARGER_PRIMARY,
	CHARGER_SECONDARY,
	CHARGER_NUM,
};

#endif /* ZEPHYR_SHIM_INCLUDE_CHARGER_ENUM_H_ */
