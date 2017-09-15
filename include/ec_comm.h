/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EC communication protocol.
 */

#ifndef EC_COMM_H_
#define EC_COMM_H_

#include <stdint.h>
#include "ec_commands.h"

#if defined(CONFIG_EC_COMM_MASTER) && defined(CONFIG_EC_COMM_BATTERY)
#define CONFIG_EC_COMM_BATTERY_MASTER
#endif

#if defined(CONFIG_EC_COMM_SLAVE) && defined(CONFIG_EC_COMM_BATTERY)
#define CONFIG_EC_COMM_BATTERY_SLAVE
#endif

extern struct ec_response_battery_static_info base_battery_static;
extern struct ec_response_battery_dynamic_info base_battery_dynamic;

#endif /* EC_COMM_H_ */
