/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "power/icelake.h"

const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);
