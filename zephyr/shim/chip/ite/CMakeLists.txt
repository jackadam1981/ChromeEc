# Copyright 2021 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#zephyr_include_directories(include)

zephyr_sources_ifdef(CONFIG_CROS_EC           system.c)
zephyr_sources_ifdef(CONFIG_CROS_EC           pinmux.c)
