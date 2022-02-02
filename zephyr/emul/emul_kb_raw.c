/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_kb_raw_emul

#include <logging/log.h>
LOG_MODULE_REGISTER(emul_kb_raw);

#include <device.h>
#include <drivers/emul.h>
#include <drivers/cros_kb_raw.h>

struct kb_raw_emul_data {
};
struct kb_raw_emul_cfg {
	/** Label of the I2C device being emulated */
	const char *dev_label;
	/** Pointer to run-time data */
	struct kb_raw_emul_data *data;
};

/**
 * @brief Set up a new kn_raw emulator
 *
 * @param device Device node.
 *
 * @return 0 indicating success (always)
 */
static int kb_raw_emul_init(const struct device *device)
{
	return 0;
}

static int emul_kb_raw_init(const struct device *dev)
{
	return 0;
}

static int emul_kb_raw_enable_interrupt(const struct device *dev, int enable)
{
	return 0;
}

static int emul_kb_raw_read_row(const struct device *dev)
{
	return 0;
}

static int emul_kb_raw_drive_column(const struct device *dev, int col)
{
	return 0;
}

static const struct cros_kb_raw_driver_api emul_kb_raw_driver_api = {
	.init = emul_kb_raw_init,
	.drive_colum = emul_kb_raw_drive_column,
	.read_rows = emul_kb_raw_read_row,
	.enable_interrupt = emul_kb_raw_enable_interrupt,
};

#define KB_RAW_EMUL(n)							\
	static struct kb_raw_emul_data kb_raw_emul_data_##n = {};	\
									\
	static const struct kb_raw_emul_cfg kb_raw_emul_cfg_##n = {	\
		.dev_label = DT_INST_LABEL(n),				\
		.data = &kb_raw_emul_data_##n,				\
	};								\
	DEVICE_DT_INST_DEFINE(n, kb_raw_emul_init, NULL,		\
				&kb_raw_emul_data_##n,			\
				&kb_raw_emul_cfg_##n,			\
				PRE_KERNEL_1,				\
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
				&emul_kb_raw_driver_api)
DT_INST_FOREACH_STATUS_OKAY(KB_RAW_EMUL);
