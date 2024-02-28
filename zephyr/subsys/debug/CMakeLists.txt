# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(include)

zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_GDBSTUB_BACKEND_CONSOLE src/backend_console.c)