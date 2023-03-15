/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EMUL_AMD_FP6_USB_MUX_H
#define EMUL_AMD_FP6_USB_MUX_H

#include <zephyr/drivers/emul.h>

void amd_fp6_emul_set_xbar(const struct emul *emul, bool ready);
void amd_fp6_emul_set_delay(const struct emul *emul, int delay_ms);

#endif
