/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chipset configuration selection for Chrome EC */

/* Chipset specific header files */
#if defined(CONFIG_CHIPSET_ALDERLAKE_SLG4BD44540)
#include "alderlake_slg4bd44540.h"
#include "intel_x86.h"
/* Geminilake and apollolake use same power sequencing. */
#elif defined(CONFIG_CHIPSET_APL_GLK)
#include "apollolake.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_CANNONLAKE)
#include "cannonlake.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_COMETLAKE)
#include "cometlake.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_COMETLAKE_DISCRETE)
#include "cometlake-discrete.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_ICELAKE)
#include "icelake.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_SKYLAKE)
#include "skylake.h"
#include "intel_x86.h"
#elif defined(CONFIG_CHIPSET_SC7180) || defined(CONFIG_CHIPSET_SC7280)
#include "qcom.h"
#elif defined(CONFIG_CHIPSET_CEZANNE) || defined(CONFIG_CHIPSET_STONEY) || \
	defined(CONFIG_CHIPSET_AMD)
#include "amd_x86.h"
#endif
