/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief API to pass virtual wire signals to power signal handler.
 */

#ifndef __AP_POWER_AP_POWER_VW_H__
#define __AP_POWER_AP_POWER_VW_H__

#include <zephyr/drivers/espi.h>

/**
 * @brief Handle a virtual wire signal.
 *
 * @param sig The enum of the signal received.
 * @param data The value of the signal
 */
void power_signal_vw_received(enum espi_vwire_signal sig, uint32_t data);

#endif /* __AP_POWER_AP_POWER_VW_H__ */
