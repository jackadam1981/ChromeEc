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
#if DT_NODE_HAS_STATUS(I2C0_0_NODE, okay)
#define I2C0_0 DT_LABEL(I2C0_0_NODE)
#else
#define I2C0_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C1_0_NODE, okay)
#define I2C1_0 DT_LABEL(I2C1_0_NODE)
#else
#define I2C1_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C2_0_NODE, okay)
#define I2C2_0 DT_LABEL(I2C2_0_NODE)
#else
#define I2C2_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C3_0_NODE, okay)
#define I2C3_0 DT_LABEL(I2C3_0_NODE)
#else
#define I2C3_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C4_0_NODE, okay)
#define I2C4_0 DT_LABEL(I2C4_0_NODE)
#else
#define I2C4_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C4_1_NODE, okay)
#define I2C4_1 DT_LABEL(I2C4_1_NODE)
#else
#define I2C4_1 ""
#endif

#if DT_NODE_HAS_STATUS(I2C5_0_NODE, okay)
#define I2C5_0 DT_LABEL(I2C5_0_NODE)
#else
#define I2C5_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C5_1_NODE, okay)
#define I2C5_1 DT_LABEL(I2C5_1_NODE)
#else
#define I2C5_1 ""
#endif

#if DT_NODE_HAS_STATUS(I2C6_0_NODE, okay)
#define I2C6_0 DT_LABEL(I2C6_0_NODE)
#else
#define I2C6_0 ""
#endif

#if DT_NODE_HAS_STATUS(I2C6_1_NODE, okay)
#define I2C6_1 DT_LABEL(I2C6_1_NODE)
#else
#define I2C6_1 ""
#endif

#if DT_NODE_HAS_STATUS(I2C7_0_NODE, okay)
#define I2C7_0 DT_LABEL(I2C7_0_NODE)
#else
#define I2C7_0 ""
#endif

const struct device *i2c_get_device_for_port(const int port)
{
	/* TODO Explore memoizing these values. */
	switch (port) {
	case NPCX_I2C_PORT0_0:
		return device_get_binding(I2C0_0);
	case NPCX_I2C_PORT1_0:
		return device_get_binding(I2C1_0);
	case NPCX_I2C_PORT2_0:
		return device_get_binding(I2C2_0);
	case NPCX_I2C_PORT3_0:
		return device_get_binding(I2C3_0);
#ifdef CHIP_VARIANT_NPCX7M6G
	case NPCX_I2C_PORT4_0:
		return device_get_binding(I2C4_0);
#endif
	case NPCX_I2C_PORT4_1:
		return device_get_binding(I2C4_1);
	case NPCX_I2C_PORT5_0:
		return device_get_binding(I2C5_0);
	case NPCX_I2C_PORT5_1:
		return device_get_binding(I2C5_1);
	case NPCX_I2C_PORT6_0:
		return device_get_binding(I2C6_0);
	case NPCX_I2C_PORT6_1:
		return device_get_binding(I2C6_1);
	case NPCX_I2C_PORT7_0:
		return device_get_binding(I2C7_0);
	default:
		return NULL;
	}
}
