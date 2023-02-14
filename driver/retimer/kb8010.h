/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for Kandou KB8001 USB-C 40 Gb/s multiprotocol switch.
 */

#ifndef __CROS_EC_KB8010_H
#define __CROS_EC_KB8010_H

#include "compile_time_macros.h"
#include "gpio_signal.h"
#include "usb_mux.h"

#define KB8010_I2C_ADDR0_FLAGS 0x08
#define KB8010_I2C_ADDR1_FLAGS 0x0C

/* Set the protocol */
#define KB8010_REG_PROTOCOL 0x0001
#define KB8010_PROTOCOL_USB3 0x0
#define KB8010_PROTOCOL_DPMF 0x1
#define KB8010_PROTOCOL_DP 0x2
#define KB8010_PROTOCOL_USB4 0x3

/* Configure the lane orientaitons */
#define KB8010_REG_ORIENTATION 0x0002
#define KB8010_CABLE_TYPE_PASSIVE 0x10
#define KB8010_CABLE_TYPE_ACTIVE_UNIDIR 0x20
#define KB8010_CABLE_TYPE_ACTIVE_BIDIR 0x30

#define KB8010_REG_RESET 0x0006
#define KB8010_RESET_FSM BIT(0)
#define KB8010_RESET_MM BIT(1)
#define KB8010_RESET_SERDES BIT(2)
#define KB8010_RESET_COM BIT(3)
#define KB8010_RESET_MASK GENMASK(3, 0)

#endif /* __CROS_EC_KB8010_H  */
