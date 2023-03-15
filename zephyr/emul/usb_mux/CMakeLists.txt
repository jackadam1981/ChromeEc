# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_EMUL_USB_MUX_AMD_FP6	emul_amd_fp6.c)
