/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"

#include "audio_codec.h"

static struct {
	struct audio_codec_i2s_rx_driver driver;

	uint8_t i2s_rx_enabled;
} priv;

static int i2s_rx_enable(struct host_cmd_handler_args *args)
{
	if (priv.i2s_rx_enabled)
		return EC_RES_ACCESS_DENIED;

	if (!priv.driver.enable)
		return EC_RES_ERROR;

	if (priv.driver.enable() != EC_SUCCESS)
		return EC_RES_ERROR;

	priv.i2s_rx_enabled = 1;

	return EC_RES_SUCCESS;
}

static int i2s_rx_disable(struct host_cmd_handler_args *args)
{
	if (!priv.i2s_rx_enabled)
		return EC_RES_ACCESS_DENIED;

	if (!priv.driver.disable)
		return EC_RES_ERROR;

	if (priv.driver.disable() != EC_SUCCESS)
		return EC_RES_ERROR;

	priv.i2s_rx_enabled = 0;

	return EC_RES_SUCCESS;
}

static int i2s_rx_set_sample_depth(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_i2s_rx *p =
		(struct ec_param_ec_codec_i2s_rx *)args->params;
	struct ec_param_ec_codec_i2s_rx_set_sample_depth *pp =
		&p->set_sample_depth_param;

	if (!priv.driver.set_sample_depth)
		return EC_RES_ERROR;

	switch (pp->depth) {
	case EC_CODEC_I2S_RX_SAMPLE_DEPTH_16:
	case EC_CODEC_I2S_RX_SAMPLE_DEPTH_24:
		break;
	}

	return EC_RES_SUCCESS;
}

static int i2s_rx_set_daifmt(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_i2s_rx *p =
		(struct ec_param_ec_codec_i2s_rx *)args->params;
	struct ec_param_ec_codec_i2s_rx_set_daifmt *pp = &p->set_daifmt_param;

	if (!priv.driver.set_daifmt)
		return EC_RES_ERROR;

	switch (pp->daifmt) {
	case EC_CODEC_I2S_RX_DAIFMT_I2S:
	case EC_CODEC_I2S_RX_DAIFMT_RIGHT_J:
	case EC_CODEC_I2S_RX_DAIFMT_LEFT_J:
	case EC_CODEC_I2S_RX_DAIFMT_DSP_A:
	case EC_CODEC_I2S_RX_DAIFMT_DSP_B:
		break;
	}

	return EC_RES_SUCCESS;
}

static int i2s_rx_set_bclk(struct host_cmd_handler_args *args)
{
	if (!priv.driver.set_bclk)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
	[EC_CODEC_I2S_RX_ENABLE] = i2s_rx_enable,
	[EC_CODEC_I2S_RX_DISABLE] = i2s_rx_disable,
	[EC_CODEC_I2S_RX_SET_SAMPLE_DEPTH] = i2s_rx_set_sample_depth,
	[EC_CODEC_I2S_RX_SET_DAIFMT] = i2s_rx_set_daifmt,
	[EC_CODEC_I2S_RX_SET_BCLK] = i2s_rx_set_bclk,
};

#ifdef DEBUG_AUDIO_CODEC
static char *strcmd[EC_CODEC_I2S_RX_SUBCMD_COUNT] = {
	[EC_CODEC_I2S_RX_ENABLE] = "EC_CODEC_I2S_RX_ENABLE",
	[EC_CODEC_I2S_RX_DISABLE] = "EC_CODEC_I2S_RX_DISABLE",
	[EC_CODEC_I2S_RX_SET_SAMPLE_DEPTH] = "EC_CODEC_I2S_RX_SET_SAMPLE_DEPTH",
	[EC_CODEC_I2S_RX_SET_DAIFMT] = "EC_CODEC_I2S_RX_SET_DAIFMT",
	[EC_CODEC_I2S_RX_SET_BCLK] = "EC_CODEC_I2S_RX_SET_BCLK",
};
#endif

static int i2s_rx_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_i2s_rx *p =
		(struct ec_param_ec_codec_i2s_rx *)args->params;

	DBG("I2S RX subcommand: %s", strcmd[p->cmd]);

	if (p->cmd < EC_CODEC_I2S_RX_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);

	WARN("invalid I2S RX subcommand: %d", p->cmd);
	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_I2S_RX,
	i2s_rx_host_command, EC_VER_MASK(0));

/*
 * Exported interfaces.
 */
int audio_codec_register_i2s_rx_driver(
	struct audio_codec_i2s_rx_driver *driver)
{
	if (!driver)
		return EC_SUCCESS;

	if (driver->enable)
		priv.driver.enable = driver->enable;
	if (driver->disable)
		priv.driver.disable = driver->disable;

	if (driver->set_sample_depth)
		priv.driver.set_sample_depth = driver->set_sample_depth;
	if (driver->set_daifmt)
		priv.driver.set_daifmt = driver->set_daifmt;
	if (driver->set_bclk)
		priv.driver.set_bclk = driver->set_bclk;

	return EC_SUCCESS;
}
