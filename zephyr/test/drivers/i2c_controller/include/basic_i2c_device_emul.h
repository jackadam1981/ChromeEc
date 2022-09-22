/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "emul/emul_common_i2c.h"

/**
 * @brief Emulator state struct
 *
 */
struct basic_i2c_device_data {
	struct i2c_common_emul_data common;
	uint8_t regs[256];
	uint8_t extended_regs[256];
};

/**
 * @brief Resets the internal register store to default
 *
 * @param emul Pointer to the emulator object
 */
void basic_i2c_device_reset(const struct emul *emul);

/**
 * @brief Gets a pointer to the internal register store
 *
 * @param emul Pointer to the emulator object
 * @return uint8_t* Pointer to start of register store
 */
uint8_t *basic_i2c_device_get_regs(const struct emul *emul);
