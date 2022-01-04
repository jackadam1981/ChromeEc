/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT microchip_xec_cros_kb_raw

#include <assert.h>
#include <drivers/cros_kb_raw.h>
#include <drivers/clock_control.h>
#include <drivers/gpio.h>
#include <drivers/pinctrl.h>
#include <kernel.h>
#include <soc.h>
#include <soc/microchip_xec/reg_def_cros.h>

#include "ec_tasks.h"
#include "keyboard_raw.h"
#include "task.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_kb_raw, LOG_LEVEL_ERR);

#define XEC_MAX_KEY_COLS 18 /* Maximum rows of keyboard matrix */
#define XEC_MAX_KEY_ROWS 8 /* Maximum columns of keyboard matrix */
#define XEC_KB_ROW_MASK (BIT(XEC_MAX_KEY_ROWS) - 1)

/* Device config */
struct cros_kb_raw_xec_config {
	/* keyboard scan controller base address */
	uintptr_t base;
	/* Keyboard scan input (KSI) wake-up irq */
	int irq;
	const struct pinctrl_dev_config *pcfg;
};

#define KB_RAW_XEC_CONFIG(dev)						\
	((struct cros_kb_raw_xec_config const *)(dev)->config)

static int kb_raw_xec_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Clock default is on */
	return 0;
}

/* Cros ec keyboard raw api functions */
static int cros_kb_raw_xec_enable_interrupt(const struct device *dev,
					    int enable)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(enable);

	/* TODO */

	return 0;
}

static int cros_kb_raw_xec_read_row(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* TODO */

	return 0;
}

static int cros_kb_raw_xec_drive_column(const struct device *dev, int col)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(col);

	/* TODO */

	return 0;
}

static void cros_kb_raw_xec_ksi_isr(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* TODO */
}

/* TODO */
static int cros_kb_raw_xec_init(const struct device *dev)
{
	struct cros_kb_raw_xec_config const *cfg = KB_RAW_XEC_CONFIG(dev);

	/* Use zephyr pinctrl to initialize pins */
	int ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);

	if (ret)
		return ret;

	IRQ_CONNECT(DT_INST_IRQN(0), 0, cros_kb_raw_xec_ksi_isr, NULL, 0);

	return 0;
}

static const struct cros_kb_raw_driver_api cros_kb_raw_xec_driver_api = {
	.init = cros_kb_raw_xec_init,
	.drive_colum = cros_kb_raw_xec_drive_column,
	.read_rows = cros_kb_raw_xec_read_row,
	.enable_interrupt = cros_kb_raw_xec_enable_interrupt,
};

/* instantiate zephyr pinctrl constant info */
PINCTRL_DT_INST_DEFINE(0)

static const struct cros_kb_raw_xec_config cros_kb_raw_cfg = {
	.base = DT_INST_REG_ADDR(0),
	.irq = DT_INST_IRQN(0),
	.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
};

DEVICE_DT_INST_DEFINE(0, kb_raw_xec_init, NULL, NULL, &cros_kb_raw_cfg,
		      PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		      &cros_kb_raw_xec_driver_api);
