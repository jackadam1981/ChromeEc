/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* #define CHIP_FAMILY_NPCX7 */
#include "registers.h"
#include "i2c/i2c.h"

/* Define all the nodes for i2c port/bus combinations. */
#define I2C0_0_NODE DT_NODELABEL(i2c0_0)
#define I2C1_0_NODE DT_NODELABEL(i2c1_0)
#define I2C2_0_NODE DT_NODELABEL(i2c2_0)
#define I2C3_0_NODE DT_NODELABEL(i2c3_0)
#define I2C4_0_NODE DT_NODELABEL(i2c4_0)
#define I2C4_1_NODE DT_NODELABEL(i2c4_1)
#define I2C5_0_NODE DT_NODELABEL(i2c5_0)
#define I2C5_1_NODE DT_NODELABEL(i2c5_1)
#define I2C6_0_NODE DT_NODELABEL(i2c6_0)
#define I2C6_1_NODE DT_NODELABEL(i2c6_1)
#define I2C7_0_NODE DT_NODELABEL(i2c7_0)

/* Define all the labels for valid i2c nodes. */
#define I2C0_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C0_0_NODE, okay), \
			   DT_LABEL(I2C0_0_NODE), "")
#define I2C1_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C1_0_NODE, okay), \
			   DT_LABEL(I2C1_0_NODE), "")
#define I2C2_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C2_0_NODE, okay), \
			   DT_LABEL(I2C2_0_NODE), "")
#define I2C3_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C3_0_NODE, okay), \
			   DT_LABEL(I2C3_0_NODE), "")
#define I2C4_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C4_0_NODE, okay), \
			   DT_LABEL(I2C4_0_NODE), "")
#define I2C4_1 COND_CODE_1(DT_NODE_HAS_STATUS(I2C4_1_NODE, okay), \
			   DT_LABEL(I2C4_1_NODE), "")
#define I2C5_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C5_0_NODE, okay), \
			   DT_LABEL(I2C5_0_NODE), "")
#define I2C5_1 COND_CODE_1(DT_NODE_HAS_STATUS(I2C5_1_NODE, okay), \
			   DT_LABEL(I2C5_1_NODE), "")
#define I2C6_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C6_0_NODE, okay), \
			   DT_LABEL(I2C6_0_NODE), "")
#define I2C6_1 COND_CODE_1(DT_NODE_HAS_STATUS(I2C6_1_NODE, okay), \
			   DT_LABEL(I2C6_1_NODE), "")
#define I2C7_0 COND_CODE_1(DT_NODE_HAS_STATUS(I2C7_0_NODE, okay), \
			   DT_LABEL(I2C7_0_NODE), "")

static const struct device *i2c_devices[NPCX_I2C_COUNT];

static int init_device_bindings(const struct device *device)
{
	ARG_UNUSED(device);
	i2c_devices[NPCX_I2C_PORT0_0] = device_get_binding(I2C0_0);
	i2c_devices[NPCX_I2C_PORT1_0] = device_get_binding(I2C1_0);
	i2c_devices[NPCX_I2C_PORT2_0] = device_get_binding(I2C2_0);
	i2c_devices[NPCX_I2C_PORT3_0] = device_get_binding(I2C3_0);
#ifdef CHIP_VARIANT_NPCX7M6G
	i2c_devices[NPCX_I2C_PORT4_0] = device_get_binding(I2C4_0);
#endif
	i2c_devices[NPCX_I2C_PORT4_1] = device_get_binding(I2C4_1);
	i2c_devices[NPCX_I2C_PORT5_0] = device_get_binding(I2C5_0);
	i2c_devices[NPCX_I2C_PORT5_1] = device_get_binding(I2C5_1);
	i2c_devices[NPCX_I2C_PORT6_0] = device_get_binding(I2C6_0);
	i2c_devices[NPCX_I2C_PORT6_1] = device_get_binding(I2C6_1);
	i2c_devices[NPCX_I2C_PORT7_0] = device_get_binding(I2C7_0);
	return 0;
}
SYS_INIT(init_device_bindings, POST_KERNEL, 51);

const struct device *i2c_get_device_for_port(const int port)
{
	if (port < 0 || port >= NPCX_I2C_COUNT)
		return NULL;
	return i2c_devices[port];
}
