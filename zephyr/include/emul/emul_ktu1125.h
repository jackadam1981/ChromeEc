/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_KTU1125_H
#define EMUL_KTU1125_H

#include <zephyr/drivers/emul.h>
#include <zephyr/sys/slist.h>

struct ktu1125_set_reg_entry_t {
	struct _snode node;
	int reg;
	uint8_t val;
	int64_t access_time;
};

/**
 * @brief Peeking each byte of register from ktu1125 emulator
 *
 * @param emul Pointer to I2C ktu1125 emulator
 * @param reg First byte of last write message
 * @param val Pointer where byte to read should be stored
 *
 * @return 0 on success
 * @return -EINVAL when register is out of range defined in ktu1125 private
 *                 register or val is NULL
 */
int ktu1125_emul_peek_reg(const struct emul *emul, int reg, uint8_t *val);

/**
 * @brief Setting each byte of register from ktu1125 emulator
 *
 * @param emul Pointer to I2C ktu1125 emulator
 * @param reg First byte of last write message
 * @param val byte to write to the reg in emulator
 *
 * @return 0 on success
 * @return -EINVAL when register is out of range defined in ktu1125 private
 *                 register or val is NULL
 */
int ktu1125_emul_write_reg(const struct emul *emul, int reg, int val);

/**
 * @brief Getting the set register history list head from a ktu1125 emulator.
 *
 * @param emul Pointer to I2C ktu1125 emulator
 *
 * @return pointer to the head node of the set private register history
 */
struct _snode *ktu1125_emul_get_reg_set_history_head(const struct emul *emul);

/**
 * @brief Reset the register setting history on a ktu1125 emulator.
 *
 * @param emul Pointer to I2C ktu1125 emulator
 */
void ktu1125_emul_reset_set_reg_history(const struct emul *emul);

#endif /* EMUL_KTU1125_H */
