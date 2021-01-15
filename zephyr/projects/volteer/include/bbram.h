/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_PROJECTS_VOLTEER_INCLUDE_BBRAM_H_
#define ZEPHYR_PROJECTS_VOLTEER_INCLUDE_BBRAM_H_

enum bbram_data_index {
	BBRM_DATA_INDEX_SCRATCHPAD = 0, /* General-purpose scratchpad */
	BBRM_DATA_INDEX_SAVED_RESET_FLAGS = 4, /* Saved reset flags */
	BBRM_DATA_INDEX_WAKE = 8, /* Wake reasons for hibernate */
	BBRM_DATA_INDEX_PD0 = 12, /* USB-PD saved port0 state */
	BBRM_DATA_INDEX_PD1 = 13, /* USB-PD saved port1 state */
	BBRM_DATA_INDEX_TRY_SLOT = 14, /* Vboot EC try slot */
	BBRM_DATA_INDEX_PD2 = 15, /* USB-PD saved port2 state */
	BBRM_DATA_INDEX_VBNVCNTXT = 16, /* VbNvContext for ARM arch */
	BBRM_DATA_INDEX_RAMLOG = 32, /* RAM log for Booter */
	BBRM_DATA_INDEX_PANIC_FLAGS = 35, /* Flag to indicate validity of
					   * panic data starting at index
					   * 36.
					   */
	BBRM_DATA_INDEX_PANIC_BKUP = 36, /* Panic data (index 35-63)*/
	BBRM_DATA_INDEX_LCT_TIME = 64, /* The start time of LCT(4 bytes)
					*/
};

#endif /* ZEPHYR_PROJECTS_VOLTEER_INCLUDE_BBRAM_H_ */
