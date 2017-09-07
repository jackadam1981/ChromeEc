/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hardware timer driver API extenstion for ISH */

#ifndef __CROS_EC_HWTIMER_ISH_H
#define __CROS_EC_HWTIMER_ISH_H

#ifdef CONFIG_LOW_POWER_IDLE
void __hw_clock_wake_set(uint32_t deadline);
void __hw_clock_wake_clear(void);
#endif


#endif /* __CROS_EC_HWTIMER_ISH_H */
