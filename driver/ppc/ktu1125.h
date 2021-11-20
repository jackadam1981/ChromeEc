/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kinetic KTU1125 Type-C Power Path Controller */

#ifndef __CROS_EC_KTU1125_H
#define __CROS_EC_KTU1125_H

#include "common.h"

#include "driver/ppc/ktu1125_public.h"

struct ktu1125_config {
	uint8_t i2c_port;
	uint8_t i2c_addr_flags;
};

extern const struct ktu1125_config ktu1125_chips[];
extern const unsigned int ktu1125_cnt;


#define KTU1125_ID           0x0
#define KTU1125_CTRL_SW_CFG  0x1
#define KTU1125_SET_SW_CFG   0x2
#define KTU1125_SET_SW2_CFG  0x3
#define KTU1125_MONITOR_SNK  0x4
#define KTU1125_MONITOR_SRC  0x5
#define KTU1125_MONITOR_DATA 0x6
#define KTU1125_INTMASK_SNK  0x7
#define KTU1125_INTMASK_SRC  0x8
#define KTU1125_INTMASK_DATA 0x9
#define KTU1125_INT_SNK      0xA
#define KTU1125_INT_SRC      0xB
#define KTU1125_INT_DATA     0xC


#endif /* defined(__CROS_EC_KTU1125_H) */
