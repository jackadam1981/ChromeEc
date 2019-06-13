/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NCT38XX registers are defined in the driver/tcpm/nct38xx.h.
 * No matter they are used by TCPC or IO Expander driver.
 */
#include "nct38xx.h"

#ifndef __CROS_EC_IOEXPANDER_NCT38XX_H
#define __CROS_EC_IOEXPANDER_NCT38XX_H

extern const struct ioexpander_drv nct38xx_ioexpander_drv;

#endif /* defined(__CROS_EC_IOEXPANDER_NCT38XX_H) */
