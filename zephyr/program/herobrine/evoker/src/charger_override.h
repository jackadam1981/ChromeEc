/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_EVOKER_CHARGER_OVERRIDE_H
#define __CROS_EC_EVOKER_CHARGER_OVERRIDE_H

#ifdef CONFIG_ZTEST
__override int board_charger_profile_override(struct charge_state_data *curr);
#endif

#endif /* __CROS_EC_EVOKER_CHARGER_OVERRIDE_H */
