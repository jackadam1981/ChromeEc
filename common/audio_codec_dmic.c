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

static int dmic_get_max_gain(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_dmic_get_max_gain *r = args->response;

	if (!priv.driver.get_max_gain)
		return EC_RES_ERROR;

	if (priv.driver.get_max_gain(&r->max_gain) != EC_SUCCESS)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int dmic_set_gain_idx(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;
	struct ec_param_ec_codec_dmic_set_gain_idx *pp = &p->set_gain_idx_param;

	if (!priv.driver.set_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.set_gain_idx(pp->channel, pp->gain) != EC_SUCCESS)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int dmic_get_gain_idx(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;
	struct ec_param_ec_codec_dmic_get_gain_idx *pp = &p->get_gain_idx_param;
	struct ec_response_ec_codec_dmic_get_gain_idx *r = args->response;

	if (!priv.driver.get_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.get_gain_idx(pp->channel, &r->gain) != EC_SUCCESS)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int dmic_set_gain_mono(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;
	struct ec_param_ec_codec_dmic_set_gain_mono *pp =
		&p->set_gain_mono_param;

	if (!priv.driver.set_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.set_gain_idx(0, pp->gain) != EC_SUCCESS)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int dmic_get_gain_mono(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_dmic_get_gain_mono *r = args->response;

	if (!priv.driver.get_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.get_gain_idx(0, &r->gain) != EC_SUCCESS)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int dmic_set_gain_dual(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;
	struct ec_param_ec_codec_dmic_set_gain_dual *pp =
		&p->set_gain_dual_param;

	if (!priv.driver.set_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.set_gain_idx(0, pp->left) != EC_SUCCESS)
		return EC_RES_ERROR;
	if (priv.driver.set_gain_idx(1, pp->right) != EC_SUCCESS)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

static int dmic_get_gain_dual(struct host_cmd_handler_args *args)
{
	struct ec_response_ec_codec_dmic_get_gain_dual *r = args->response;

	if (!priv.driver.get_gain_idx)
		return EC_RES_ERROR;

	if (priv.driver.get_gain_idx(0, &r->left) != EC_SUCCESS)
		return EC_RES_ERROR;
	if (priv.driver.get_gain_idx(1, &r->right) != EC_SUCCESS)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

static int (*sub_cmds[])(struct host_cmd_handler_args *) = {
	[EC_CODEC_DMIC_GET_MAX_GAIN] = dmic_get_max_gain,
	[EC_CODEC_DMIC_SET_GAIN_IDX] = dmic_set_gain_idx,
	[EC_CODEC_DMIC_GET_GAIN_IDX] = dmic_get_gain_idx,
	[EC_CODEC_DMIC_SET_GAIN_MONO] = dmic_set_gain_mono,
	[EC_CODEC_DMIC_GET_GAIN_MONO] = dmic_get_gain_mono,
	[EC_CODEC_DMIC_SET_GAIN_DUAL] = dmic_set_gain_dual,
	[EC_CODEC_DMIC_GET_GAIN_DUAL] = dmic_get_gain_dual,
};

#ifdef DEBUG_AUDIO_CODEC
static char *strcmd[EC_CODEC_DMIC_SUBCMD_COUNT] = {
	[EC_CODEC_DMIC_GET_MAX_GAIN] = "EC_CODEC_DMIC_GET_MAX_GAIN",
	[EC_CODEC_DMIC_SET_GAIN_IDX] = "EC_CODEC_DMIC_SET_GAIN_IDX",
	[EC_CODEC_DMIC_GET_GAIN_IDX] = "EC_CODEC_DMIC_GET_GAIN_IDX",
	[EC_CODEC_DMIC_SET_GAIN_MONO] = "EC_CODEC_DMIC_SET_GAIN_MONO",
	[EC_CODEC_DMIC_GET_GAIN_MONO] = "EC_CODEC_DMIC_GET_GAIN_MONO",
	[EC_CODEC_DMIC_SET_GAIN_DUAL] = "EC_CODEC_DMIC_SET_GAIN_DUAL",
	[EC_CODEC_DMIC_GET_GAIN_DUAL] = "EC_CODEC_DMIC_GET_GAIN_DUAL",
};
#endif

static int dmic_host_command(struct host_cmd_handler_args *args)
{
	struct ec_param_ec_codec_dmic *p =
		(struct ec_param_ec_codec_dmic *)args->params;

	DBG("DMIC subcommand: %s", strcmd[p->cmd]);

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

	if (driver->get_max_gain)
		priv.driver.get_max_gain = driver->get_max_gain;
	if (driver->set_gain_idx)
		priv.driver.set_gain_idx = driver->set_gain_idx;
	if (driver->get_gain_idx)
		priv.driver.get_gain_idx = driver->get_gain_idx;

	return EC_SUCCESS;
}
