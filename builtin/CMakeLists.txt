# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC
    # TODO(b/237712836): Remove once Zephyr's libc has strcasecmp.
    stdlib.c
)