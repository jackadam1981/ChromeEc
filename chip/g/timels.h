/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INC_TIMELS_H_
#define INC_TIMELS_H_

#include "common.h"
#include "registers.h"

extern void timels_xtl_en(uint32_t enable);
extern void timels_rc_en(uint32_t enable);

#endif /* INC_TIMELS_H_ */
