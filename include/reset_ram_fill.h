/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __RESET_RAM_FILL_H
#define __RESET_RAM_FILL_H

#include <stdint.h>

extern volatile uint32_t *const reset_ram_fill_cond0_reg;
extern volatile uint32_t *const reset_ram_fill_cond1_reg;
extern const uint32_t reset_ram_fill_cond0_mask;
extern const uint32_t reset_ram_fill_cond1_mask;

#endif /* __RESET_RAM_FILL_H */