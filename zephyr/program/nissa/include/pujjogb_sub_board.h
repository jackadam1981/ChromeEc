/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Pujjoga sub-board declarations */

#ifndef __CROS_EC_NISSA_PUJJOGB_SUB_BOARD_H__
#define __CROS_EC_NISSA_PUJJOGB_SUB_BOARD_H__

enum pujjogb_sub_board_type {
	PUJJOGB_SB_UNKNOWN = -1, /* Uninitialised */
	PUJJOGB_SB_NONE = 0, /* No board defined */
	PUJJOGB_SB_HDMI_A = 1, /* HDMI, USB type A */
};

enum pujjoga_sub_board_type pujjogb_get_sb_type(void);

#endif /* __CROS_EC_NISSA_PUJJOGB_SUB_BOARD_H__ */
