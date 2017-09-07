/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
*/

#ifndef __POWER_MGT_H
#define __POWER_MGT_H

#include "common.h"

#ifdef CONFIG_LOW_POWER_IDLE

void pm_init(void);

void pm_execute_idle_flow(int32_t length_of_idle);
void pm_return_from_idle(void);


#endif /* CONFIG_LOW_POWER_IDLE */

#endif /* __POWER_MGT_H */
