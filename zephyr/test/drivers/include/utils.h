/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_
#define ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_

#include "charger.h"

/** @brief Set chipset to S0 state. Call all necessary hooks. */
void test_set_chipset_to_s0(void);

/** @brief Set chipset to G3 state. Call all necessary hooks. */
void test_set_chipset_to_g3(void);

/*
 * TODO(b/217755888): Implement ztest assume API upstream
 */

/**
 * @brief Assume that this function call won't be reached
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_unreachable(msg, ...) zassert_unreachable(msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a cond is true
 * @param cond Condition to check
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_true(cond, msg, ...) zassert_true(cond, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a cond is false
 * @param cond Condition to check
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_false(cond, msg, ...) zassert_false(cond, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a cond is 0 (success)
 * @param cond Condition to check
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_ok(cond, msg, ...) zassert_ok(cond, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a ptr is NULL
 * @param ptr Pointer to compare
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_is_null(ptr, msg, ...) zassert_is_null(ptr, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a ptr is not NULL
 * @param ptr Pointer to compare
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_not_null(ptr, msg, ...) \
	zassert_not_null(ptr, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a a equals @a b
 *
 * @a a and @a b won't be converted and will be compared directly.
 *
 * @param a Value to compare
 * @param b Value to compare
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_equal(a, b, msg, ...) zassert_equal(a, b, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a a does not equal @a b
 *
 * @a a and @a b won't be converted and will be compared directly.
 *
 * @param a Value to compare
 * @param b Value to compare
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_not_equal(a, b, msg, ...) \
	zassert_not_equal(a, b, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a a equals @a b
 *
 * @a a and @a b will be converted to `void *` before comparing.
 *
 * @param a Value to compare
 * @param b Value to compare
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_equal_ptr(a, b, msg, ...) \
	zassert_equal_ptr(a, b, msg, ##__VA_ARGS__)

/**
 * @brief Assume that @a a is within @a b with delta @a d
 *
 * @param a Value to compare
 * @param b Value to compare
 * @param d Delta
 * @param msg Optional message to print if the assumption fails
 */
#define zassume_within(a, b, d, msg, ...) \
	zassert_within(a, b, d, msg, ##__VA_ARGS__)

/**
 * @brief Assume that 2 memory buffers have the same contents
 *
 * This macro calls the final memory comparison assumption macro.
 * Using double expansion allows providing some arguments by macros that
 * would expand to more than one values (ANSI-C99 defines that all the macro
 * arguments have to be expanded before macro call).
 *
 * @param ... Arguments, see @ref zassume_mem_equal__
 *            for real arguments accepted.
 */
#define zassume_mem_equal(...) zassert_mem_equal(##__VA_ARGS__)

/**
 * Make the host command to get the charge state for a given charger number.
 *
 * This function assumes a successful host command processing and will make a
 * call to the zassume_* API. A failure here will abort the calling test.
 *
 * @param chgnum The charger number to query.
 * @return The result of the query.
 */
static inline struct ec_response_charge_state host_cmd_charge_state(int chgnum)
{
	struct ec_params_charge_state params = {
		.chgnum = chgnum,
		.cmd = CHARGE_STATE_CMD_GET_STATE,
	};
	struct ec_response_charge_state response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_CHARGE_STATE, 0, response, params);

	zassume_ok(host_command_process(&args),
		   "Failed to get charge state for chgnum %d", chgnum);
	return response;
}

static inline struct ec_response_usb_pd_power_info host_cmd_power_info(int port)
{
	struct ec_params_usb_pd_power_info params = { .port = port };
	struct ec_response_usb_pd_power_info response;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_USB_PD_POWER_INFO, 0, response, params);

	zassume_ok(host_command_process(&args),
		   "Failed to get power info for port %d", port);
	return response;
}

static inline struct ec_response_typec_status host_cmd_typec_status(int port)
{
	struct ec_params_typec_status params = { .port = port };
	struct ec_response_typec_status response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_TYPEC_STATUS, 0, response, params);

	zassume_ok(host_command_process(&args),
		   "Failed to get Type-C state for port %d", port);
	return response;
}

#endif /* ZEPHYR_TEST_DRIVERS_INCLUDE_UTILS_H_ */
