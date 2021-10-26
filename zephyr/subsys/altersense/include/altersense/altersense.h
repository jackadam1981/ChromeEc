/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_ALTERSENSE_H_
#define ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_ALTERSENSE_H_

/**
 * @file Main altersense subsystem API
 *
 * Altersense is a subsystem used to select alternate sensors at runtime. It is
 * configured using devicetree phandles to existing sensor drivers. In the
 * following example, it is assumed that accel0 and accel1 are defined. Also,
 * each rule simply must adhere to the altersense/rule.h API so the system is
 * fully extensible to 3rd party rules when selecting a sensor at runtime.
 *
 * rule0: rule0 {
 *   compatible = "cros,altersense-rule-board-version";
 *   status = "okay";
 *   mask = <0xffffffff>;
 *   range = <0 10>; // Will select secondary if
 *                   // (0 <= (board_version & mask) <= 10).
 * };
 * rule1: rule1 {
 *   compatible = "cros,altersense-rule-ssfc";
 *   status = "okay";
 *   mask = <0x7>;
 *   range = <1 1>; // Will select secondary if (1 <= (ssfc & mask) <= 1).
 * };
 * accel {
 *   compatible = "cros,altersense";
 *   status = "okay";
 *   label = "ACCEL";
 *   primary = <&accel0>;
 *   secondary = <&accel1>;
 *   rules = <&rule0 &rule1>;
 * };
 */

/**
 * @brief Get the currently selected device.
 *
 * @param dev The altersense node
 * @return The currently selected device
 */
const struct device *altersense_get_selected(const struct device *dev);

/**
 *
 * @param dev The altersense node
 * @return negative value for error
 * @return 0 or more for the number of changed sensors.
 */
int altersense_refresh(const struct device *dev);

#endif /* ZEPHYR_SUBSYS_ALTERSENSE_INCLUDE_ALTERSENSE_ALTERSENSE_H_ */
