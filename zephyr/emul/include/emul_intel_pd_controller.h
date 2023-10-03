/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_INTEL_PD_CONTROLLER_H
#define __EMUL_INTEL_PD_CONTROLLER_H

#include <zephyr/drivers/emul.h>
#include <drivers/intel_altmode.h>

/**
 * @brief Modify the value status register of emulated Intel PD Controller.
 *
 * @param emul The device emulator instance.
 * @param status The value of status register.
 *
 * @return 0 on success
 * @return -EINVAL if an invalid argument is provided
 */
int intel_pd_controller_emul_set_status(const struct emul *emul,
					const union data_status_reg status);

/**
 * @brief Trigger emulated Intel PD Controller interruption.
 *
 * @param emul The device emulator instance.
 *
 * @return 0 on success
 * @return -EINVAL if an invalid argument is provided
 */
int intel_pd_controller_emul_trigger_irq(const struct emul *emul);

#endif /*__EMUL_INTEL_PD_CONTROLLER_H */
