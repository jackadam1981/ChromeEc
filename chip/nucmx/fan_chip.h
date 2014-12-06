/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NUCMX-specific MFT module for Chrome EC */

#ifndef __CROS_EC_NUCMX_FAN_H
#define __CROS_EC_NUCMX_FAN_H

/* MFT module select */
enum nucmx_mft_module {
	NUCMX_MFT_MODULE_1 = 0,
	NUCMX_MFT_MODULE_2 = 0,
	NUCMX_MFT_MODULE_3 = 0,
	/* Number of MFT modules */
	NUCMX_MFT_MODULE_COUNT
};

/* MFT module port */
enum nucmx_mft_module_port {
	NUCMX_MFT_MODULE_PORT_TA,
	NUCMX_MFT_MODULE_PORT_TB,
	/* Number of MFT module ports */
	NUCMX_MFT_MODULE_PORT_COUNT
};

/* Data structure to define MFT channels. */
struct mft_t {
	/* MFT module ID */
	enum nucmx_mft_module module;
	/* MFT port */
	enum nucmx_mft_module_port port;
	/* MFT TCNT default count */
	uint32_t default_count;
	/* MFT freq */
	uint32_t freq;
};

extern const struct mft_t mft_channels[];

#endif /* __CROS_EC_NUCMX_FAN_H */
