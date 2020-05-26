/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

#ifdef SECTION_IS_RO
#include "ccd_measure_sbu.c"
#include "chg_control.c"
#include "ina231s.c"
#include "pathsel.c"
#endif /* SECTION_IS_RO */
