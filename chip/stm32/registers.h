/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for STM32 processor
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"
#include "compile_time_macros.h"

#if defined(CHIP_FAMILY_STM32H7)
#include "registers-stm32h7.h"
#else
#include "registers-all.h"
#endif

#endif /* __CROS_EC_REGISTERS_H */
