/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_PD_CONTROLLER_H
#define __EMUL_INTEL_PD_CONTROLLER_H

#include <zephyr/drivers/emul.h>
#include <drivers/intel_altmode.h>

int intel_pd_controller_emul_set_status(const struct emul *emul,
					const union data_status_reg status);

int intel_pd_controller_emul_trigger_irq(const struct emul *emul);

#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
