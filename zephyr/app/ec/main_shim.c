/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include "ec_app_main.h"
#include "host_command.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>

#define I2C_DEV_NAME	DT_LABEL(DT_ALIAS(i2c_4))


uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER;

static int test_i2c(void)
{
	unsigned char datas[6];
	const struct device *i2c_dev = device_get_binding(I2C_DEV_NAME);
	uint32_t i2c_cfg_tmp;

	if (!i2c_dev) {
		printk("Cannot get I2C device\n");
		return -1;
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_dev, i2c_cfg)) {
		printk("I2C config failed\n");
		return -1;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_dev, &i2c_cfg_tmp)) {
		printk("I2C get_config failed\n");
		return -1;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		printk("I2C get_config returned invalid config\n");
		return -1;
	}

	datas[0] = 0x01;
	datas[1] = 0x20;

	/* 3. verify i2c_write() */
	if (i2c_write(i2c_dev, datas, 2, 0x52)) {
		printk("Fail to configure\n");
		return -1;
	}

	datas[0] = 0x02;
	datas[1] = 0x00;
	if (i2c_write(i2c_dev, datas, 2, 0x52)) {
		printk("Fail to configure\n");
		return -1;
	}

	k_sleep(K_MSEC(1));

	datas[0] = 0x03;
	if (i2c_write(i2c_dev, datas, 1, 0x52)) {
		printk("Fail to write\n");
		return -1;
	}

	(void)memset(datas, 0, sizeof(datas));

	/* 4. verify i2c_read() */
	if (i2c_read(i2c_dev, datas, 6, 0x52)) {
		printk("Fail to fetch sample\n");
		return -1;
	}

	//printk("[i2c]axis raw data: %d %d %d %d %d %d\n",
	//			datas[0], datas[1], datas[2],
	//			datas[3], datas[4], datas[5]);

	return 0;
}

static int test_i2c_burst(void)
{
	unsigned char datas[6];
	const struct device *i2c_dev = device_get_binding(I2C_DEV_NAME);
	uint32_t i2c_cfg_tmp;

	if (!i2c_dev) {
		printk("Cannot get I2C device\n");
		return -1;
	}

	/* 1. verify i2c_configure() */
	if (i2c_configure(i2c_dev, i2c_cfg)) {
		printk("I2C config failed\n");
		return -1;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_dev, &i2c_cfg_tmp)) {
		printk("I2C get_config failed\n");
		return -1;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		printk("I2C get_config returned invalid config\n");
		return -1;
	}

	datas[0] = 0x01;
	datas[1] = 0x20;
	datas[2] = 0x02;
	datas[3] = 0x00;

	/* 3. verify i2c_burst_write() */
	if (i2c_burst_write(i2c_dev, 0x52, 0x00, datas, 4)) {
		printk("Fail to write\n");
		return -1;
	}

	k_sleep(K_MSEC(1));

	(void)memset(datas, 0, sizeof(datas));

	/* 4. verify i2c_burst_read() */
	if (i2c_burst_read(i2c_dev, 0x52, 0x03, datas, 6)) {
		printk("Fail to fetch sample\n");
		return -1;
	}

	//printk("[i2c]axis raw data: %d %d %d %d %d %d\n",
	//			datas[0], datas[1], datas[2],
	//			datas[3], datas[4], datas[5]);

	return 0;
}

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
void main(void)
{
	ec_app_main();

	int ret,i;

	printk("[i2c]start test...\n");

	for(i=0;i<=30;i++) {
		ret = test_i2c();
		if(ret)
			break;

		ret = test_i2c_burst();
		if(ret)
			break;

	}
	if(ret)
		printk("[i2c]fail to access\n");
	else
		printk("[i2c]sucess!\n\n\n");



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
