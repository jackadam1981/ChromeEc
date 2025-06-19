/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_RVP_BOARD_ID_H_
#define ZEPHYR_INCLUDE_DRIVERS_RVP_BOARD_ID_H_

#define BOARD_ID 0
#define BOM_ID 1
#define FAB_ID 2

int get_rvp_id_config(int);

#endif /* ZEPHYR_INCLUDE_DRIVERS_RVP_BOARD_ID_H_ */
