/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for LN9310 emulator
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_LN9310_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_LN9310_H_

#include <emul.h>
#include "driver/ln9310.h"

void ln9310_set_battery_cell_type(const struct emul *emul,
				  enum battery_cell_type type);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_LN9310_H_ */
