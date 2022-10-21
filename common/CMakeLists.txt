# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

add_subdirectory(spi)

# CONFIG_PLATFORM_EC files that don't relate to something below should be
# included here, sorted by filename. This is common functionality which is
# supported by all boards and emulators (including unit tests) using the shim
# layer.
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC
    base32.c
    console_output.c
    ec_features.c
    gpio_commands.c
    peripheral.c
    printf.c
    queue.c
    shared_mem.c
    system.c
    uart_printf.c
    util.c
    version.c
)
