/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_IT8XXX2_H
#define EMUL_IT8XXX2_H

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

void it8xxx2_emul_reset(const struct emul *emul);

#endif /* EMUL_IT8XXX2_H */