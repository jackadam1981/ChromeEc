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

#define I2C_DEV_NODE	DT_NODELABEL(i2c1)
#define I2C_DEV_TARGET_NODE DT_NODELABEL(i2c2_target)

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
uint8_t w_datas[256];
uint8_t r_datas[256];
uint8_t r_datas2[20];
uint8_t r_datas3[256];
uint8_t w_datas1[256];

static int test_i2c_host(void)
{
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct device *const i2c_target_dev = DEVICE_DT_GET(I2C_DEV_TARGET_NODE);
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
#if 1
	printf("[main]1.===write datas===\n");

	for (int i = 0; i < sizeof(w_datas); i++) {
		w_datas[i] = i;
	}
	//w_datas[256]=0x77;w_datas[257]=0x78;w_datas[258]=0x79;

	if (i2c_write(i2c_dev, w_datas, sizeof(w_datas), 0x52)) {
		printf("Fail to write\n");
		return -1;
	}


	printf("[main]2.===read datas===\n");


	/* 4. verify i2c_read() */
	if (i2c_read(i2c_dev, r_datas, sizeof(r_datas), 0x52)) {
		printf("Fail to read\n");
		return -1;
	}

	printf("axis raw data: %x %x %x %x %x %x\n",
				r_datas[0], r_datas[1], r_datas[2],
				r_datas[3], r_datas[4], r_datas[5]);

#endif
	printf("[main]3.===write to read datas===\n");

	/* 4. verify i2c_burst_read() */
	if (i2c_burst_read(i2c_dev, 0x52, 0x22, r_datas2, sizeof(r_datas2))) {
		printf("Fail to fetch sample\n");
		return -1;
	}

	printf("axis raw data: %x %x %x %x %x %x\n",
				r_datas2[0], r_datas2[1], r_datas2[2],
				r_datas2[3], r_datas2[4], r_datas2[5]);
#if 0
	printf("[main]4.===read datas===\n");

	/* 5. verify i2c_read() */
	if (i2c_read(i2c_dev, r_datas3, sizeof(r_datas3), 0x52)) {
		printf("Fail to read\n");
		return -1;
	}

	printf("[main]5.===write datas===\n");

	for (int i = 0; i < sizeof(w_datas1); i++) {
		w_datas1[i] = 0xbb;
	}

	if (i2c_write(i2c_dev, w_datas1, sizeof(w_datas1), 0x52)) {
		printf("Fail to write\n");
		return -1;
	}
#endif
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
