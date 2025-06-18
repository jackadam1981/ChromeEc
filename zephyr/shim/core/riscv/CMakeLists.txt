# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_include_directories("${ZEPHYR_BASE}/arch/riscv/include")

zephyr_library_sources_ifdef(CONFIG_MPU mpu.c)