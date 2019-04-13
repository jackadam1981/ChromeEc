/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"

#include "audio_codec.h"

static struct {
	struct audio_codec_dmic_driver driver;
} priv;

static int dmic_set_gain(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;
	struct ec_param_ec_codec_dmic_set_gain *pp = &p->set_gain_param;

	if (!priv.driver.set_gain)
		return EC_RES_ERROR;

	if (priv.driver.set_gain(pp->left, pp->right) != EC_SUCCESS)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int dmic_get_gain(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_dmic_get_gain *r = args->response;

	if (!priv.driver.get_gain)
		return EC_RES_ERROR;

	if (priv.driver.get_gain(&r->left, &r->right) != EC_SUCCESS)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
	[EC_CODEC_DMIC_SET_GAIN] = dmic_set_gain,
	[EC_CODEC_DMIC_GET_GAIN] = dmic_get_gain,
};

static int dmic_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;

	DBG("DMIC subcommand: %d", p->cmd);

	if (p->cmd < EC_CODEC_DMIC_SUBCMD_COUNT)
		return sub_cmds[p->cmd](args);

	WARN("invalid DMIC subcommand: %d", p->cmd);
	return EC_RES_INVALID_PARAM;
}
DECLARE_HOST_COMMAND(EC_CMD_EC_CODEC_DMIC, dmic_host_command, EC_VER_MASK(0));

/*
 * Exported interfaces.
 */
int audio_codec_register_dmic_driver(struct audio_codec_dmic_driver *driver)
{
	if (!driver)
		return EC_SUCCESS;

	if (driver->set_gain)
		priv.driver.set_gain = driver->set_gain;
	if (driver->get_gain)
		priv.driver.get_gain = driver->get_gain;

	return EC_SUCCESS;
}
