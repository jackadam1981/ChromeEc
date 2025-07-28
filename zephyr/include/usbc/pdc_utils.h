/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_PDC_UTILS_H_
#define ZEPHYR_INCLUDE_DRIVERS_PDC_UTILS_H_

#define __TALLY_CCD_PROP(node) COND_CODE_1(DT_PROP(node, ccd), (1), (0)) +

/**
 * @brief Count the number of nodes with the `ccd` property true under the
 *        provided compat string.
 */
#define COUNT_CCD_PORTS_BY_COMPAT(compat) \
	(DT_FOREACH_STATUS_OKAY(compat, __TALLY_CCD_PROP) 0)

#endif /* ZEPHYR_INCLUDE_DRIVERS_PDC_UTILS_H_ */
