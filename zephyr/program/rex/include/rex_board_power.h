/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_REX_REX_BOARD_POWER_H__
#define __CROS_EC_REX_REX_BOARD_POWER_H__

#include "common.h"

#include <ap_power/ap_power.h>

/*
 * Functions executed when AP resume change.
 *
 * A board should override this function if it has different functions
 * need to be executed when AP resume change.
 */
__override_proto void board_resume_change(struct ap_power_ev_callback *,
					  struct ap_power_ev_data);

#endif /* __CROS_EC_REX_REX_BOARD_POWER_H__ */
