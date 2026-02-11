/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	ec_app_main();

	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}

	return 0;
}
/* LCOV_EXCL_STOP */

#include <zephyr/drivers/i2c.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c_test));

static uint8_t buf[129];
static uint8_t inbuf[128];

// i2c read i2c@4000d200 51 0 10

static int cmd_i2c_stress(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	uint8_t i = 0;

	while (true) {
		LOG_INF("i=%d", i);

		memset(buf, i, sizeof(buf));
		buf[0] = 0;

		ret = i2c_write(i2c_dev, buf, sizeof(buf), 0x51);
		if (ret) {
			LOG_ERR("i2c_write error: %d", ret);
			return 0;
		}

		k_sleep(K_MSEC(10));

		uint8_t off = 0;
		ret = i2c_write_read(i2c_dev, 0x51, &off, sizeof(off), inbuf, sizeof(inbuf));
		if (ret) {
			LOG_ERR("i2c_read error: %d", ret);
			return 0;
		}


		if (memcmp(buf+1, inbuf, sizeof(inbuf)) != 0) {
			LOG_ERR("data mismatch");

			LOG_HEXDUMP_INF(buf, sizeof(buf), "buf");
			LOG_HEXDUMP_INF(inbuf, sizeof(inbuf), "inbuf");
		}

		k_sleep(K_MSEC(10));

		i++;
	}

	return 0;
}

SHELL_CMD_REGISTER(i2c_stress, NULL, "i2c stress", cmd_i2c_stress);

static void thread1_main(void)
{
	int ret;
	uint8_t buf[10];

	int i = 0;

	while (true) {
		uint8_t off = 0;
		ret = i2c_write_read(i2c_dev, 0x28, &off, sizeof(off), buf, sizeof(buf));
		if (ret) {
			LOG_ERR("i2c_read error: %d", ret);
			return;
		}

		//LOG_HEXDUMP_INF(buf, 10, "buf");
		LOG_INF("thread1 step %d", i);

		k_sleep(K_MSEC(100));

		i++;
	}
}

K_THREAD_DEFINE(thread1, 1024, thread1_main, NULL, NULL, NULL, 0, 0, 0);
