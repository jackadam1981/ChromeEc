/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Gladios board-specific USB-C configuration */

#ifndef __CROS_EC_USBC_CONFIG_H
#define __CROS_EC_USBC_CONFIG_H

#define CONFIG_USB_PD_PORT_MAX_COUNT 1

enum usbc_port { USBC_PORT_C0 = 0, USBC_PORT_COUNT };

/* USB-A ports */
enum usba_port { USBA_PORT_A0 = 0,USBA_PORT_A1,
		USBA_PORT_A2, USBA_PORT_A3, USBA_PORT_COUNT };

#endif /* __CROS_EC_USBC_CONFIG_H */
