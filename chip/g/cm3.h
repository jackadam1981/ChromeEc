/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Cortex-M3 core functions
 * @note In the future this should be replaced with CMSIS-CORE
 */

#ifndef INC_CM3_H_
#define INC_CM3_H_

#include "common.h"
#include "registers.h"

void cm3_systick_config(uint32_t reload_val, uint32_t int_en);
uint32_t cm3_systick_read(void);

#endif /* INC_CM3_H_ */
