/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Regulator control module for Chrome EC */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"

#define CPRINTS(format, args...) cprints(/* TODO */ CC_HOSTCMD, format, ## args)

/**
 * TODO
 */
static enum ec_status
hc_regulator_set_enable(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_set_enable *p = args->params;

	CPRINTS("SET ENABLE %u %u", p->index, p->enabled);

	/*
	 * TODO: this is only a mock for now, figure out how to handle
	 * customization for boards.
	 */
	if (p->index >= 1)
		return EC_RES_INVALID_PARAM;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_SET_ENABLE, hc_regulator_set_enable,
		     EC_VER_MASK(0));

/**
 * TODO
 */
static enum ec_status
hc_regulator_is_enabled(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_is_enabled *p = args->params;
	struct ec_response_regulator_is_enabled *r = args->response;

	CPRINTS("QUERY ENABLED %u", p->index);

	/*
	 * TODO: this is only a mock for now, figure out how to handle
	 * customization for boards.
	 */
	if (p->index >= 1)
		return EC_RES_INVALID_PARAM;

	r->enabled = 1;
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_IS_ENABLED, hc_regulator_is_enabled,
		     EC_VER_MASK(0));

/**
 * TODO
 */
static enum ec_status
hc_regulator_get_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_get_voltage *p = args->params;
	struct ec_response_regulator_get_voltage *r = args->response;

	CPRINTS("GET REGULATOR %u", p->index);

	/*
	 * TODO: this is only a mock for now, figure out how to handle
	 * customization for boards.
	 */
	if (p->index >= 1)
		return EC_RES_INVALID_PARAM;

	r->selector = 0;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_GET_VOLTAGE, hc_regulator_get_voltage,
		     EC_VER_MASK(0));

/**
 * TODO
 */
static enum ec_status
hc_regulator_set_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_set_voltage *p = args->params;

	CPRINTS("SET REGULATOR %u %u", p->index, p->selector);

	/*
	 * TODO: this is only a mock for now, figure out how to handle
	 * customization for boards.
	 */
	if (p->index >= 1 || p->selector >= 2)
		return EC_RES_INVALID_PARAM;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_SET_VOLTAGE, hc_regulator_set_voltage,
		     EC_VER_MASK(0));
