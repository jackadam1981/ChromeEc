# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable this as a separate library when upstreaming
#zephyr_library()

zephyr_library_sources_ifdef(CONFIG_USBC_BC12_PI3USB9201 pi3usb9201.c)
