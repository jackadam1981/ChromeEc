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
#include "util.h"

static enum ec_status
hc_regulator_get_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_get_info *p = args->params;
	struct ec_response_regulator_get_info *r = args->response;
	int rv;

	rv = board_regulator_get_info(p->index, r->name, &r->num_voltages,
				      r->voltages_mv);

	if (rv)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_GET_INFO, hc_regulator_get_info,
		     EC_VER_MASK(0));

static int command_regulator_get_info(int argc, char **argv)
{
	int id;
	char *e;
	char name[EC_REGULATOR_NAME_MAX_LEN];
	uint16_t num_voltages;
	uint16_t voltages_mv[EC_REGULATOR_VOLTAGE_MAX_COUNT] = {0};
	int rv, i;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	id = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	rv = board_regulator_get_info(id, name, &num_voltages, voltages_mv);

	if (rv)
		return rv;


	ccprintf("id: %d\n", id);
	ccprintf("name: %s\n", name);
	ccprintf("num_voltages: %d\n", num_voltages);
	for (i = 0; i < EC_REGULATOR_VOLTAGE_MAX_COUNT; i++) {
		if (voltages_mv[i])
			ccprintf("voltages_mv: %d\n", voltages_mv[i]);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rgt_getinfo, command_regulator_get_info,
                        "regulator_id", "Get regulator info");

static enum ec_status
hc_regulator_enable(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_enable *p = args->params;
	int rv;

	rv = board_regulator_enable(p->index, p->enable);

	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_ENABLE, hc_regulator_enable,
		     EC_VER_MASK(0));

static int command_regulator_enable(int argc, char **argv)
{
	int id;
	char *e;
	int enable;
	int rv;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	id = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argv[2][0] == 'o' && argv[2][1] == 'n')
		enable = 1;
	else if (argv[2][0] == 'o' && argv[2][1] == 'f')
		enable = 0;
	else
		return EC_ERROR_PARAM2;

	rv = board_regulator_enable(id, enable);

	if (rv)
		return rv;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rgt_enable, command_regulator_enable,
                        "regulator_id [on | off]",
                        "Enable | Disable regulator");

static enum ec_status
hc_regulator_is_enabled(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_is_enabled *p = args->params;
	struct ec_response_regulator_is_enabled *r = args->response;
	int rv;

	rv = board_regulator_is_enabled(p->index, &r->enabled);

	if (rv)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_IS_ENABLED, hc_regulator_is_enabled,
		     EC_VER_MASK(0));

static int command_regulator_is_enabled(int argc, char **argv)
{
	int id;
	char *e;
	uint8_t enabled;
	int rv;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	id = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	rv = board_regulator_is_enabled(id, &enabled);

	if (rv)
		return rv;

	ccprintf("regulator_id%d : %sabled\n", id, enabled ? "en" : "dis");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rgt_is_enabled, command_regulator_is_enabled,
                        "regulator_id", "Show if regulator is enabled");

static enum ec_status
hc_regulator_get_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_get_voltage *p = args->params;
	struct ec_response_regulator_get_voltage *r = args->response;
	int rv;

	rv = board_regulator_get_voltage(p->index, &r->voltage_mv);

	if (rv)
		return EC_RES_ERROR;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_GET_VOLTAGE, hc_regulator_get_voltage,
		     EC_VER_MASK(0));

static enum ec_status
hc_regulator_set_voltage(struct host_cmd_handler_args *args)
{
	const struct ec_params_regulator_set_voltage *p = args->params;
	int rv;

	rv = board_regulator_set_voltage(p->index, p->min_mv, p->max_mv);

	if (rv)
		return EC_RES_ERROR;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_REGULATOR_SET_VOLTAGE, hc_regulator_set_voltage,
		     EC_VER_MASK(0));
