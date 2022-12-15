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
#include <zephyr/sys/printk.h>
#include "system.h"
#include "i2c.h"

#define I2C_DEV_NODE	DT_ALIAS(i2c_4)
#define I2C_DEV_TARGET_NODE DT_NODELABEL(i2c5_target)

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

#if 1
static int test_i2c_target_write()
{
	struct i2c_target_config *config=(struct i2c_target_config *)0x800103184;
	struct i2c_msg msg[1];
	int ret;
	uint8_t len = 20;
	uint8_t *buf = "0123456789abcdefghij";

	config[0].address = 0x52;
	msg[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	config->callbacks->write_requested(config);
	buf++;
	len--;

	while (len) {
		ret = config->callbacks->write_received(config, *buf);
		//printf("[i2c]target_write *buf=%x\n",*buf);
		if (ret) {
			return -1;
		}
		buf++;
		len--;
	}

	if (!(msg->flags & I2C_MSG_RESTART) && msg->flags & I2C_MSG_STOP) {
		config->callbacks->stop(config);
	}

	return 0;

}

static int test_i2c_target_read()
{
	struct i2c_target_config *config=(struct i2c_target_config *)0x800103184;
	struct i2c_msg msg[1];
	uint8_t len = 20;
	uint8_t buf = 0;

	config[0].address = 0x52;
	msg[0].flags = I2C_MSG_READ | I2C_MSG_STOP;

	if (!len) {
		return 0;
	}

	config->callbacks->read_requested(config, &buf);
	buf++;
	len--;

	while (len) {
		config->callbacks->read_processed(config, &buf);
		printf("I2C target read buf =%x\n",buf);
		buf++;
		len--;
	}

	if (!(msg->flags & I2C_MSG_RESTART) && msg->flags & I2C_MSG_STOP) {
		config->callbacks->stop(config);
	}

	return 0;
}
#endif

static int test_i2c_host(void)
{

	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	const struct device *const i2c_target_dev = DEVICE_DT_GET(I2C_DEV_TARGET_NODE);

	uint32_t i2c_cfg_tmp, ret;

	printf("I2C target dev=%p\n",i2c_target_dev);

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
	uint8_t datas[20]="0123456789abcdefghij";

	if (i2c_write(i2c_dev, datas, 20, 0x52)) {
		printf("Fail to write to sensor GY271\n");
		return -1;
	}
#endif
#if 0
	uint8_t *datas1=(uint8_t *)0x80102f90;
	(void)memset(datas1, 0, 0x100);

	printf("===read datas address=== %p\n",datas1);

	/* 4. verify i2c_read() */
	if (i2c_read(i2c_dev, datas1, 6, 0x52)) {
		printf("Fail to fetch sample from sensor GY271\n");
		return -1;
	}

	printf("axis raw data: %x %x %x %x %x %x\n",
				datas1[0], datas1[1], datas1[2],
				datas1[3], datas1[4], datas1[5]);

#endif
#if 0
	uint8_t *datas1=(uint8_t *)0x80103084;
	(void)memset(datas1, 0, 0x100);
	printf("===burst datas address=== %p\n",datas1);

	/* 4. verify i2c_burst_read() */
	if (i2c_burst_read(i2c_dev, 0x52, 0x33, datas1, 64)) {
		printf("Fail to fetch sample\n");
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
	int ret, i;

	printf("[i2c]==========test_i2c_host to target==========\n");

	for(i=0;i<1;i++) {
		ret = test_i2c_host();
		if(ret)
			break;

		//ret = test_i2c_burst();
		//if(ret)
		//	break;

	}
	if(ret)
		printf("[i2c]fail to access\n");
	else
		printf("[i2c]i2c transfer sucess!\n\n\n");

	printf("[i2c]==========test_i2c_target write syscall==========\n");
	ret = test_i2c_target_write();

	if(ret)
		printf("[i2c]fail to target_write\n");
	else
		printf("[i2c]sucess target_write!\n\n\n");


	printf("[i2c]==========test_i2c_target read syscall==========\n");
	ret = test_i2c_target_read();

	if(ret)
		printf("[i2c]fail to target_write\n");
	else
		printf("[i2c]sucess target_read!\n\n\n");


	printf("[i2c]==========end test==========\n");

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
