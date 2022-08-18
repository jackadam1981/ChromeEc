/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <gpio.h>
#include <zephyr/shell/shell_dummy.h>
#include <console.h>

#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "ec_commands.h"
#include "host_command.h"

struct gpio_commands_fixture {
	const enum gpio_signal signal;
	char name[32];
	int level;
	gpio_flags_t flags;
};

static void *gpio_commands_setup(void)
{
	static struct gpio_commands_fixture test_fixture = {
		.signal = GPIO_SIGNAL(DT_NODELABEL(gpio_test)),
		.level = 0,
	};

	strcpy(test_fixture.name, gpio_get_name(test_fixture.signal));
	gpio_set_level(test_fixture.signal, test_fixture.level);
	test_fixture.flags = gpio_get_default_flags(test_fixture.signal);

	return &test_fixture;
}

ZTEST_F(gpio_commands, test_hc_gpio_get_v0)
{
	struct ec_response_gpio_get response;
	struct ec_params_gpio_get params;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 0, response, params);

	strcpy(params.name, fixture->name);
	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);
	zassert_equal(sizeof(response), args.response_size, NULL);
	zassert_equal(fixture->level, response.val, NULL);
}

ZTEST(gpio_commands, test_hc_gpio_get_v0_invalid_name)
{
	struct ec_response_gpio_get response;
	struct ec_params_gpio_get params = { .name = "INVALID_GPIO_NAME" };
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 0, response, params);

	zassert_equal(EC_RES_ERROR, host_command_process(&args), NULL);
}

ZTEST_F(gpio_commands, test_hc_gpio_get_v1_get_by_name)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_BY_NAME,
	};
	strcpy(params.get_value_by_name.name, fixture->name);
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);
	zassert_equal(sizeof(response.get_value_by_name), args.response_size,
		      NULL);
	zassert_equal(fixture->level, response.get_value_by_name.val, NULL);
}

ZTEST(gpio_commands, test_hc_gpio_get_v1_get_by_name_invalid_name)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_BY_NAME,
		.get_value_by_name.name = "INVALID_GPIO_NAME",
	};
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_ERROR, host_command_process(&args), NULL);
}

ZTEST(gpio_commands, test_hc_gpio_get_v1_get_count)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_COUNT,
	};
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);
	zassert_equal(sizeof(response.get_count), args.response_size, NULL);
	zassert_equal(GPIO_COUNT, response.get_count.val, NULL);
}

ZTEST_F(gpio_commands, test_hc_gpio_get_v1_get_info)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_INFO,
		.get_info.index = fixture->signal,
	};
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);
	zassert_equal(sizeof(response.get_info), args.response_size, NULL);
	zassert_equal(fixture->flags, response.get_info.flags, NULL);
	zassert_equal(fixture->level, response.get_info.val, NULL);
	zassert_mem_equal(fixture->name, response.get_info.name,
			  strlen(response.get_info.name), NULL);
}

ZTEST(gpio_commands, test_hc_gpio_get_v1_get_info_invalid_index)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_GPIO_GET_INFO,
		.get_info.index = GPIO_COUNT,
	};
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_ERROR, host_command_process(&args), NULL);
}

ZTEST(gpio_commands, test_hc_gpio_get_v1_invalid_subcmd)
{
	struct ec_response_gpio_get_v1 response;
	struct ec_params_gpio_get_v1 params = {
		.subcmd = EC_CMD_GPIO_GET,
	};
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_GPIO_GET, 1, response, params);

	zassert_equal(EC_RES_INVALID_PARAM, host_command_process(&args), NULL);
}

ZTEST_SUITE(gpio_commands, drivers_predicate_post_main, gpio_commands_setup,
	    NULL, NULL, NULL);
