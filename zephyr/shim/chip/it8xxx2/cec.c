/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/cec/it83xx.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/i2c/it8xxx2-i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <chip_chipregs.h>

LOG_MODULE_REGISTER(shim_cros_cec, LOG_LEVEL_ERR);

#define DT_DRV_COMPAT ite_it8xxx2_cec_raw
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
	     "Exactly one instance of ite,it8xxx2-cec should be defined.");

#define IT8XXX2_CEC_NODE DT_INST(0, DT_DRV_COMPAT)
PINCTRL_DT_DEFINE(IT8XXX2_CEC_NODE);

#define CGC_OFFSET_CEC ((IT8XXX2_ECPM_CGCTRL4R_OFF << 8) | 0x01)

void cros_cec_enable(int enable)
{
	/* Enable clock to specified peripheral */
	volatile uint8_t *reg =
		(volatile uint8_t *)(IT8XXX2_ECPM_BASE + (CGC_OFFSET_CEC >> 8));
	uint8_t reg_mask = CGC_OFFSET_CEC & 0xff;
	const struct pinctrl_dev_config *pcfg =
		PINCTRL_DT_DEV_CONFIG_GET(IT8XXX2_CEC_NODE);

	if (enable) {
		/* Enable CEC clock */
		*reg &= ~reg_mask;
		/* Enable alternate function */
		pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
	} else {
		/* Configure pin back to GPIO */
		pinctrl_apply_state(pcfg, PINCTRL_STATE_SLEEP);
		/* Disable CEC clock */
		*reg |= reg_mask;
	}
}
