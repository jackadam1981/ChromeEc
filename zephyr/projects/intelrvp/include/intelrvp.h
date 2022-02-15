/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __INTELRVP_BOARD_H
#define __INTELRVP_BOARD_H

#include <devicetree.h>

/* RVP ID read retry count */
#define RVP_VERSION_READ_RETRY_CNT	2

#if DT_NODE_EXISTS(DT_PATH(intelrvp_board_id))
#define I2C_PORT_BOARD_ID_GPIO \
	(I2C_PORT(DT_PHANDLE(DT_NODELABEL(intelrvp_board_id), i2c_port)))
#define I2C_ADDR_BOARD_ID_GPIO \
	(DT_PROP(DT_NODELABEL(intelrvp_board_id), i2c_addr))
#endif

#endif /* __INTELRVP_BOARD_H */
