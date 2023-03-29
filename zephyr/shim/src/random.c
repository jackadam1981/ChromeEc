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
 */
void trng_init(void)
{
}
void trng_exit(void)
{
}

void trng_rand_bytes(void *buffer, size_t len)
{
	int ret;

	if (!device_is_ready(rng) || len > UINT16_MAX)
		k_oops();

	ret = entropy_get_entropy(rng, (uint8_t *)buffer, (uint16_t)len);

	if (ret < 0)
		k_oops();
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
static int command_rand(int argc, const char **argv)
{
	uint8_t data[32];
	char str_buf[hex_str_buf_size(sizeof(data))];

	trng_init();
	trng_rand_bytes(data, sizeof(data));
	trng_exit();

	snprintf_hex_buffer(str_buf, sizeof(str_buf),
			    HEX_BUF(data, sizeof(data)));
	ccprintf("rand %s\n", str_buf);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rand, command_rand, NULL,
			"Output random bytes to console.");
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

	trng_init();
	trng_rand_bytes(r->rand, num_rand_bytes);
	trng_exit();

	args->response_size = num_rand_bytes;

	return EC_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_RAND_NUM, host_command_rand,
		     EC_VER_MASK(EC_VER_RAND_NUM));
#endif
