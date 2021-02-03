/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "power/icelake.h"
#include "power/intel_x86.h"

/*
 * Brya uses a power sequencer chip to drive DSW_PWROK to the AP.
 */
__override void intel_x86_dsw_pwrok_pass_thru(void)
{
}


const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);
