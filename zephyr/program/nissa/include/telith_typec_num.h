/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Telith typec-num declarations */

#ifndef __CROS_EC_NISSA_NISSA_SUB_BOARD_H__
#define __CROS_EC_NISSA_NISSA_SUB_BOARD_H__

enum telith_typec_num {
	TELITH_TYPEC_UNKNOWN = -1, /* Uninitialised */
	TELITH_TYPEC_NONE = 0,
	TELITH_TYPEC_ONE = 1, /* USB type C */
};

enum telith_typec_num telith_get_typec_num(void);

#endif /* __CROS_EC_NISSA_NISSA_SUB_BOARD_H__ */
