/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_INFO_H__
#define __CROS_EC_STARFISH_SIM_INFO_H__

#include <stdint.h>

struct slot_name {
	char name[64];
};

void sim_setting_set_slot_id(int slot_number, int32_t *in);
bool sim_setting_get_slot_id(int slot_number, int32_t *out);

void sim_setting_set_slot_name(int slot_number, struct slot_name *in);
bool sim_setting_get_slot_name(int slot_number, struct slot_name *out);


#endif /* __CROS_EC_STARFISH_SIM_INFO_H__ */
