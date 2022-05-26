/*
 * Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef DT_BINDINGS_BATTERY_H_
#define DT_BINDINGS_BATTERY_H_

/*
 * These values are taken from ec/include/battery.h
 * They are used by LED devicetree files (led.dts) to define battery-level
 * range. UI change is triggered when EC sends a host command based on
 * battery-levels. LED behavior should be in sync with the UI behavior, so
 * these values must match the values defined in ec/include/battery.h.
 */
#define BATT_LEVEL_EMPTY	0
#define BATT_LEVEL_SHUTDOWN	3
#define BATT_LEVEL_CRITICAL	5
#define BATT_LEVEL_LOW		10
#define BATT_LEVEL_NEAR_FULL	97
#define BATT_LEVEL_FULL		100

#endif /* DT_BINDINGS_BATTERY_H_ */
