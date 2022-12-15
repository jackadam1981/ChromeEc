/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "task.h"
#include "i2c.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

//#define I2C_DEV_NODE	DT_ALIAS(i2c_1)
#define I2C_DEV_NODE	DT_NODELABEL(i2c4)

uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;
uint8_t w_datas[255];
uint8_t r_datas[255];
uint8_t w_datas1[59];
uint8_t r_datas1[64];

static int test_i2c_host(const struct device *const i2c_dev)
{
#if 0
	printf("\n[main]1.===read datas===\n");

	if (i2c_read(i2c_dev, r_datas, sizeof(r_datas), 0x52)) {
		printf("Fail to read\n");
		return -1;
	}

	printf("\n[main]===read second===\n");

	if (i2c_read(i2c_dev, r_datas, sizeof(r_datas), 0x62)) {
		printf("Fail to read\n");
		return -1;
	}


	printf("[main]1.===write datas(<=255bytes FIFO mode allowed)===\n");

	for (int i = 0; i < sizeof(w_datas); i++) {
		w_datas[i] = i;
	}
	//w_datas[256]=0x77;w_datas[257]=0x78;w_datas[258]=0x79;

	if (i2c_write(i2c_dev, w_datas, sizeof(w_datas), 0x52)) {
		printf("Fail to write\n");
		return -1;
	}


#endif
	//printf("\n[main]1-1.===write datas(<=59bytes CQ mode allowed)===\n");
#if 1
	if (i2c_write(i2c_dev, w_datas1, sizeof(w_datas1), 0x52)) {
		printf("Fail to write\n");
		return -1;
	}

	//printf("\n[main]2.===read datas(<=255bytes FIFO mode allowed)===\n");

	if (i2c_read(i2c_dev, r_datas1, sizeof(r_datas1), 0x52)) {
		printf("Fail to read\n");
		return -1;
	}

	w_datas1[0] = 0x22;
	w_datas1[1] = 0x00;

	int ret2 = i2c_write(i2c_dev, w_datas, sizeof(w_datas), 0x52);
	if (ret2) {
		printf("Fail to write = ret=%d\n",ret2);
		return -1;
	}

	ret2 = i2c_read(i2c_dev, r_datas, sizeof(r_datas), 0x52);
	if (ret2) {
		printf("Fail to read ret = %d\n",ret2);
		return -1;
	}
	printf("raw data: %x %x %x %x %x %x %x %x\n",
				r_datas[0], r_datas[1], r_datas[2],
				r_datas[3], r_datas[4], r_datas[5], r_datas[6], r_datas[6]);


	printf("\n[main]3.===i2c_burst_read(<=255bytes FIFO mode allowed)===\n");

	(void)memset(&r_datas, 0, sizeof(r_datas));

	if (i2c_burst_read(i2c_dev, 0x52, 0xaa, r_datas, sizeof(r_datas))) {
		printf("Fail to read\n");
		return -1;
	}
	printf("axis raw data: %x %x %x %x %x %x\n",
				r_datas[0], r_datas[1], r_datas[2],
				r_datas[3], r_datas[4], r_datas[5]);
#endif
#if 0
	//printf("\n[main]3-1.===i2c_burst_read(<=64bytes CQ mode allowed)===\n");

	if (i2c_burst_read(i2c_dev, 0x52, 0xaa, r_datas1, sizeof(r_datas1))) {
		printf("Fail to read\n");
		return -1;
	}
	//printf("axis raw data: %x %x %x %x %x %x\n",
	//			r_datas1[0], r_datas1[1], r_datas1[2],
	//			r_datas1[3], r_datas1[4], r_datas1[5]);

#endif
#if 0
	printf("\n[main]4.===i2c_burst_write(CQ and FIFO mode don't allow)===\n");

	if (i2c_burst_write(i2c_dev, 0x52, 0xbb, w_datas, sizeof(w_datas)-1)) {
		printk("Fail to write\n");
		return -1;
	}

	printf("\n[main]5.===Cros I2C: i2c_read_block(<=255bytes FIFO mode allowed)===\n");

	(void)memset(&r_datas, 0, sizeof(r_datas));

	if (i2c_read_block(1,0x52,0xcc,r_datas,sizeof(r_datas))) { //i2c_1-->port=1, i2c_4-->port=3
		printf("Fail to read\n");
		return -1;
	}
	printk("[i2c]axis raw data: %x %x %x %x %x %x\n",
				r_datas[0], r_datas[1], r_datas[2],
				r_datas[3], r_datas[4], r_datas[5]);


	printf("\n[main]6.===Cros I2C: i2c_write_block(CQ and FIFO mode don't allow)===\n");

	if (i2c_write_block(1,0x52,0xdd,w_datas,sizeof(w_datas)-1)) {
		printk("Fail to write\n");
		return -1;
	}
#endif

#ifdef CONFIG_PLATFORM_EC_SMBUS_PEC
	/* I2C4 = number of enable port = 3th */
	if (i2c_write16(3, 0x52, 0xbb, 0xccdd)) {
		printf("Fail to write\n");
		return -1;
	}
#endif

	return 0;
}

static void board_init(void)
{
	int ret;
	timestamp_t t_i2chost_start, t_i2chost_end;
	uint64_t duration_us;
	const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
	uint32_t i2c_cfg_tmp;

	printf("[main]===test_i2c_host to target===\n");

	if (!device_is_ready(i2c_dev)) {
		printf("I2C device is not ready\n");
		return;
	}

	/* 1. Verify i2c_configure() */
	if (i2c_configure(i2c_dev, i2c_cfg)) {
		printf("I2C config failed\n");
		return;
	}

	/* 2. Verify i2c_get_config() */
	if (i2c_get_config(i2c_dev, &i2c_cfg_tmp)) {
		printf("I2C get_config failed\n");
		return;
	}
	if (i2c_cfg != i2c_cfg_tmp) {
		printf("I2C get_config returned invalid config\n");
		return;
	}

	for (int i = 0; i < sizeof(w_datas); i++) {
		w_datas[i] = i;
	}
	for (int i = 0; i < sizeof(w_datas1); i++) {
		w_datas1[i] = i;
	}

	t_i2chost_start = get_time();
	for(int i=0; i<1; i++) {
		ret = test_i2c_host(i2c_dev);
		//k_sleep(K_MSEC(1));
	}
	t_i2chost_end = get_time();

	duration_us = (t_i2chost_end.val - t_i2chost_start.val);
	printk("i2c host total duration_us %llu us\n", duration_us);

	if(ret)
		printf("[main]fail to access\n");
	else
		printf("[main]i2c transfer sucess!\n\n\n");

}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_POST_I2C);
