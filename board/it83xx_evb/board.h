/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx development board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* NOTE: 0->ec evb, non-zero->pd evb */
#define IT83XX_PD_EVB  0
/* Select Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_DAC

#endif /* __CROS_EC_BOARD_H */
