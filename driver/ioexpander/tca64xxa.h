/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DRIVER_IOEXPANDER_TCA64XXA_H_
#define __CROS_EC_DRIVER_IOEXPANDER_TCA64XXA_H_

#define TCA64XXA_FLAG_VER_TCA6416A	2
#define TCA64XXA_FLAG_VER_TCA6424A	4
#define TCA64XXA_FLAG_VER_MASK		0b110
#define TCA64XXA_FLAG_VER_OFFSET	0

extern const struct ioexpander_drv tca64xxa_ioexpander_drv;

#endif /* __CROS_EC_DRIVER_IOEXPANDER_TCA64XXA_H_ */
