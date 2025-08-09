/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_RT3645_H
#define EMUL_RT3645_H

#include <zephyr/drivers/emul.h>
#include <zephyr/sys/slist.h>

/**
 * @brief Peeking each byte of register from rt3645 emulator
 *
 * @param emul Pointer to I2C rt3645 emulator
 * @param reg First byte of last write message
 * @param val Pointer where byte to read should be stored
 *
 * @return 0 on success
 * @return -EINVAL when register is out of range defined in rt3645 private
 *                 register or val is NULL
 */
int rt3645_emul_read_reg(const struct emul *emul, int reg, uint8_t *val);

/**
 * @brief Setting each byte of register from rt3645 emulator
 *
 * @param emul Pointer to I2C rt3645 emulator
 * @param reg First byte of last write message
 * @param val byte to write to the reg in emulator
 *
 * @return 0 on success
 * @return -EINVAL when register is out of range defined in rt3645 private
 *                 register or val is NULL
 */
int rt3645_emul_write_reg(const struct emul *emul, int reg, int val);

void rt3645_emul_reset_regs(const struct emul *emul);

bool rt3645_emul_in_config_mode(const struct emul *emul);

#endif /* EMUL_RT3645_H */
