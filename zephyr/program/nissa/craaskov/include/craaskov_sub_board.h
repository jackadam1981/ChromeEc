/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Craaskov sub-board declarations */

#ifndef __CROS_EC_NISSA_NISSA_SUB_BOARD_H__
#define __CROS_EC_NISSA_NISSA_SUB_BOARD_H__

enum craaskov_sub_board_type {
	CRAASKOV_SB_UNKNOWN = -1, /* Uninitialised */
	CRAASKOV_SB_NONE = 0, /* No board defined */
	CRAASKOV_SB_HDMI_A = 1, /* HDMI, USB type A */
};

enum craaskov_sub_board_type craaskov_get_sb_type(void);

#endif /* __CROS_EC_NISSA_NISSA_SUB_BOARD_H__ */
