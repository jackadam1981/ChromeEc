/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include "system.h"

#define I2C_DEV_NODE	DT_NODELABEL(i2c0)
#define I2C_DEV_TARGET_NODE DT_NODELABEL(i2c5_target)
#define I2C_DEV_TARGET_HOST DT_NODELABEL(i2c5)

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

static int test_i2c_host(void)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct device *const i2c_target_dev = DEVICE_DT_GET(I2C_DEV_TARGET_NODE);
	const struct device *const i2c_target_host = DEVICE_DT_GET(I2C_DEV_TARGET_HOST);
	uint32_t i2c_cfg_tmp, ret;

	ret = i2c_target_driver_register(i2c_target_dev);
	if (ret) {
		printf("I2C target register failed\n");
	}

	if (!device_is_ready(i2c_dev)) {
		printf("I2C device is not ready\n");
		return -1;
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_dev, i2c_cfg)) {
		printf("I2C config failed\n");
		return -1;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_dev, &i2c_cfg_tmp)) {
		printf("I2C get_config failed\n");
		return -1;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		printf("I2C get_config returned invalid config\n");
		return -1;
	}

	printf("[main]===read request datas===\n");
	uint8_t r_datas[256];
	for (int i = 0; i < 256; i++) {
		r_datas[i] = 0xcc;
	}
	if (i2c_target_dma_read(i2c_target_host, r_datas, 256)) {
		printf("[main]Fail to i2c_read_request_data\n");

		return -1;
	}

	printf("[main]===write datas===\n");
	uint8_t w_datas[256];
	for (int i = 0; i < 256; i++) {
		w_datas[i] = i;
	}

	if (i2c_write(i2c_dev, w_datas, 256, 0x52)) {
		printf("Fail to write\n");
		return -1;
	}

	printf("[main]===read datas===\n");
	uint8_t datas1[6];
	/* 4. verify i2c_read() */
	if (i2c_read(i2c_dev, datas1, 6, 0x52)) {
		printf("Fail to read\n");
		return -1;
	}

	printf("axis raw data: %x %x %x %x %x %x\n",
				datas1[0], datas1[1], datas1[2],
				datas1[3], datas1[4], datas1[5]);


	ret = i2c_target_driver_unregister(i2c_target_dev);
	if (ret) {
		printf("I2C target unregister failed\n");
	}

	return 0;
}

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
void main(void)
{
	ec_app_main();
	int ret;

	printf("[main]===test_i2c_host to target===\n");
	ret = test_i2c_host();
	if(ret)
		printf("[main]fail to access\n");
	else
		printf("[main]i2c transfer sucess!\n\n\n");


	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}
}
/* LCOV_EXCL_STOP */
