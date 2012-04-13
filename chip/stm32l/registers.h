/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for STM32 processor
 */

#ifndef __STM32_REGISTERS
#define __STM32_REGISTERS

#include "ec.h"

#if defined(EC_STM32F100R8)
#include "registers-stm32f.h"
#elif defined(EC_STM32L151R8)
#include "registers-stm32l.h"
#else
#error "must define exact EC part"
#endif

#endif /* __STM32_REGISTERS */
