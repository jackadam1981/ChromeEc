/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * STM32F412 + FPC 1025 Fingerprint MCU configuration
 *
 * Alternate names that share this same board file:
 *   hatch_fp
 *   bloonchipper
 *   dragonclaw
 */

#ifndef __BOARD_H
#define __BOARD_H

#include "base-board.h"

#ifdef SECTION_IS_RW
#define CONFIG_FP_SENSOR_FPC1025
#endif /* SECTION_IS_RW */

#endif /* __BOARD_H */
