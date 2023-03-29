/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "host_command.h"
#include "printf.h"
#include "system.h"

#include <zephyr/drivers/entropy.h>
#include <zephyr/kernel.h>

#define rng DEVICE_DT_GET(DT_CHOSEN(zephyr_entropy))

/*
 * Zephyr driver is responsible for initializing, enabling and disabling
 * hardware. In this case, trng_init() and trng_exit() does nothing.
 *
 * LCOV_EXCL_START
 */
void trng_init(void)
{
}
void trng_exit(void)
{
}
/* LCOV_EXCL_STOP */

void trng_rand_bytes(void *buffer, size_t len)
{
	int ret;
	uint16_t to_copy;

	if (!device_is_ready(rng))
		k_oops();

	/*
	 * In EC, we use size_t to represent buffer size, but Zephyr uses
	 * uint16_t. Let's call entropy_get_entropy() multiple times to fill
	 * whole buffer.
	 */
	for (size_t copied = 0; copied < len; copied += to_copy) {
		to_copy = MIN(len - copied, UINT16_MAX);

		ret = entropy_get_entropy(rng, (uint8_t *)buffer + copied,
					  to_copy);
		if (ret < 0)
			k_oops();
	}
}

uint32_t trng_rand(void)
{
	uint32_t random;
	int ret;

	if (!device_is_ready(rng))
		k_oops();

	ret = entropy_get_entropy(rng, (uint8_t *)&random, sizeof(random));

	if (ret < 0)
		k_oops();

	return random;
}

#if defined(CONFIG_PLATFORM_EC_CONSOLE_CMD_RAND)
static int command_rand(const struct shell *shell, int argc, const char **argv)
{
	uint8_t data[32];
	char str_buf[hex_str_buf_size(sizeof(data))];

	trng_rand_bytes(data, sizeof(data));

	snprintf_hex_buffer(str_buf, sizeof(str_buf),
			    HEX_BUF(data, sizeof(data)));
	shell_fprintf(shell, SHELL_NORMAL, "rand %s\n", str_buf);

	return EC_SUCCESS;
}
SHELL_CMD_REGISTER(rand, NULL, "Output random bytes to console.", command_rand);
#endif

#if defined(CONFIG_PLATFORM_EC_HOSTCMD_RAND)
static enum ec_status host_command_rand(struct host_cmd_handler_args *args)
{
	const struct ec_params_rand_num *p = args->params;
	struct ec_response_rand_num *r = args->response;
	uint16_t num_rand_bytes = p->num_rand_bytes;

	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;

	if (num_rand_bytes > args->response_max)
		return EC_RES_OVERFLOW;

	trng_rand_bytes(r->rand, num_rand_bytes);

	args->response_size = num_rand_bytes;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_RAND_NUM, host_command_rand,
		     EC_VER_MASK(EC_VER_RAND_NUM));
#endif
