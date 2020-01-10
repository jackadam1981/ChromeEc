/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A simple abstraction for the hardware write protect line */

#ifndef __CROS_EC_HWWP_H
#define __CROS_EC_HWWP_H

#include <stdbool.h>

/**
 * @brief Indicate hardware write protect pin status
 *
 * This takes into account if the GPIO is active low or active high.
 *
 * @return true HW WP pin is asserted
 * @return false HW WP pin is asserted
 */
bool hwwp_isasserted(void);

#endif /* __CROS_EC_HWWP_H */