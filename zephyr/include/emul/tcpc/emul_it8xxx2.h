/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_IT8XXX2_H
#define __EMUL_IT8XXX2_H

#include <zephyr/drivers/emul.h>

#define IT8XXX2_REG_CTRL_OUT_EN_DEFAULT 0x00
#define IT8XXX2_REG_CTRL_OUT_EN_RESERVED_MASK GENMASK(7, 7)

#define IT8XXX2_REG_VBC_FAULT_CTL_DEFAULT 0x01
#define IT8XXX2_REG_VBC_FAULT_CTL_RESERVED_MASK (GENMASK(7, 6) | GENMASK(2, 2))

int it8xxx2_emul_get_reg(const struct emul *emul, int r, uint16_t *val);
int it8xxx2_emul_set_reg(const struct emul *emul, int r, uint16_t val);

void it8xxx2_emul_reset(const struct emul *emul);

#endif /* __EMUL_IT8XXX2_H */
