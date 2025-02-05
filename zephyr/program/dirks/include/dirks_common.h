/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Dirks shared common functionality */

#ifndef __CROS_EC_DIRKS_DIRKS_COMMON_H__
#define __CROS_EC_DIRKS_DIRKS_COMMON_H__

#include <ap_power/ap_power.h>

/**
 * Functions executed when AP power change.
 *
 * This function is called from shared common code of dirks.
 *
 * A board should override this function if it has different functions
 * need to be executed when AP power change.
 */
__override_proto void board_power_change(struct ap_power_ev_callback *,
					 struct ap_power_ev_data);

#endif /* __CROS_EC_DIRKS_DIRKS_COMMON_H__ */
