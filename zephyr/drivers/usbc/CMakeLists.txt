# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_USBC_INTEL_ALTMODE intel_altmode.c)
zephyr_library_sources_ifdef(CONFIG_PDC_CCG8_PWR_MGNT pdc_ccg8_pwr_mngt.c)
zephyr_library_sources_ifdef(CONFIG_PDC_RTS54_PWR_MGNT pdc_rts54_pwr_mngt.c)
