/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Regulator control module for Chrome EC */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "regulator.h"

#define CPRINTS(format, args...) cprints(/* TODO */ CC_HOSTCMD, format, ## args)

/**
 * TODO(pihsun): Documentation
 */
static enum ec_status
hc_regulator_enable(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_enable *p = args->params;
	int rv;

	CPRINTS("SET ENABLE %u %u", p->index, p->enable);

	rv = board_regulator_enable(p->index, p->enable);

	/*
	 * TODO(pihsun): Should we return EC_RES_INVALID_PARAM when rv is
	 * EC_ERROR_INVAL?
	 */
	/* TODO(pihsun): Log error */
	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_ENABLE, hc_regulator_enable,
		     EC_VER_MASK(0));

/**
 * TODO(pihsun): Documentation
 */
static enum ec_status
hc_regulator_is_enabled(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_is_enabled *p = args->params;
	struct ec_response_regulator_is_enabled *r = args->response;
	int rv;

	CPRINTS("QUERY ENABLED %u", p->index);

	rv = board_regulator_is_enabled(p->index, &r->enabled);

	if (rv)
		return EC_RES_ERROR;

	CPRINTS("RETURNS %u", r->enabled);
	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_IS_ENABLED, hc_regulator_is_enabled,
		     EC_VER_MASK(0));

/**
 * TODO(pihsun): Documentation
 */
static enum ec_status
hc_regulator_get_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_get_voltage *p = args->params;
	struct ec_response_regulator_get_voltage *r = args->response;
	int rv;

	CPRINTS("GET REGULATOR %u", p->index);

	rv = board_regulator_get_voltage(p->index, &r->selector);

	if (rv)
		return EC_RES_ERROR;

	CPRINTS("RETURNS %u", r->selector);
	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_GET_VOLTAGE, hc_regulator_get_voltage,
		     EC_VER_MASK(0));

/**
 * TODO(pihsun): Documentation
 */
static enum ec_status
hc_regulator_set_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_set_voltage *p = args->params;
	int rv;

	CPRINTS("SET REGULATOR %u %u", p->index, p->selector);

	rv = board_regulator_set_voltage(p->index, p->selector);

	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_SET_VOLTAGE, hc_regulator_set_voltage,
		     EC_VER_MASK(0));
