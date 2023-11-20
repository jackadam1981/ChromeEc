/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TCPMv3
 */
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define NODE_ID	DT_NODELABEL(pdc_power_p0p1)
#if DT_NODE_HAS_STATUS(NODE_ID, okay)
#define PDC	NODE_ID
#else
#error "Can't find PDC Node"
#endif

struct tcpm_port_t {
	const struct device *pdc;
	int port_num;
};

struct tcpm_data_t {
	struct tcpm_port_t port[2];
};

struct tcpm_config_t {
	int tmp;
};

K_THREAD_STACK_DEFINE(my_stack_area, 2000);

static struct tcpm_data_t tcpm_data = {
	.port[0].pdc = DEVICE_DT_GET(PDC),
	.port[0].port_num = 0,
	.port[1].pdc = DEVICE_DT_GET(PDC),
	.port[1].port_num = 1,
};

static const struct tcpm_config_t tcpm_config = {
	.tmp = 0;
};

/**
 * @brief Initialize the USB-C Subsystem
 */
static int tcpm_subsys_init(const struct device *dev)
{
	LOG_INF("TCPMv3 Started");

	return 0;
}

DEVICE_DEFINE(pdc_power_p0p1,"pdc_power_p0p1", &tcpm_subsys_init, NULL, &tcpm_data,
			&tcpm_config, POST_KERNEL,
			CONFIG_APPLICATION_INIT_PRIORITY, NULL);
