# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(NOT DEFINED CONFIG_ZTEST)
    target_sources(app PRIVATE main.c)
    zephyr_sources_ifdef(CONFIG_PM_POLICY_APP         power_policy.c)
endif()
