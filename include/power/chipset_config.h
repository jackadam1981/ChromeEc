/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chipset configuration selection for Chrome EC */

/* Chipset specific header files */
#if defined(CONFIG_CHIPSET_ALDERLAKE_SLG4BD44540)
#include "alderlake_slg4bd44540.h"
/* Geminilake and apollolake use same power sequencing. */
#elif defined(CONFIG_CHIPSET_APL_GLK)
#include "apollolake.h"
#elif defined(CONFIG_CHIPSET_CANNONLAKE)
#include "cannonlake.h"
#elif defined(CONFIG_CHIPSET_COMETLAKE)
#include "cometlake.h"
#elif defined(CONFIG_CHIPSET_COMETLAKE_DISCRETE)
#include "cometlake-discrete.h"
#elif defined(CONFIG_CHIPSET_ICELAKE)
#include "icelake.h"
#elif defined(CONFIG_CHIPSET_SKYLAKE)
#include "skylake.h"
#elif defined(CONFIG_CHIPSET_SC7180) || defined(CONFIG_CHIPSET_SC7280)
#include "qcom.h"
#endif
