/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_HOST_CMD_TEST_USB_PD_HOST_CMD_H_
#define ZEPHYR_TEST_DRIVERS_HOST_CMD_TEST_USB_PD_HOST_CMD_H_

#include <zephyr/fff.h>
#include <zephyr/types.h>

#define TEST_PORT USBC_PORT_C0

DECLARE_FAKE_VOID_FUNC(pd_send_vdm, int, uint32_t, int, const uint32_t *, int);
DECLARE_FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);

#endif /* ZEPHYR_TEST_DRIVERS_HOST_CMD_TEST_USB_PD_HOST_CMD_H_ */
