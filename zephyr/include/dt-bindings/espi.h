/*
 * Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef DT_BINDINGS_ESPI_H_
#define DT_BINDINGS_ESPI_H_

/*
 * Defines for ESPI device configuration.
 * Copied from zephyr/main/include/drivers/espi.h
 */
#define ESPI_CHANNEL_PERIPHERAL		(1 << 0)
#define ESPI_CHANNEL_VWIRE		(1 << 1)
#define ESPI_CHANNEL_OOB		(1 << 2)
#define ESPI_CHANNEL_FLASH		(1 << 3)

#define ESPI_IO_MODE_SINGLE_LINE	(1 << 0)
#define ESPI_IO_MODE_DUAL_LINES		(1 << 1)
#define ESPI_IO_MODE_QUAD_LINES		(1 << 2)

#endif /* DT_BINDINGS_ESPI_H_ */
