# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# By default CONFIG_COMMON_LIBC_MALLOC allocates all unused ram at runtime.
# This will corrupt EC panic and jump data (b/394299886).
# If CONFIG_COMMON_LIBC_MALLOC is enabled,
# ensure CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE is set to a positve value.
if(DEFINED CONFIG_COMMON_LIBC_MALLOC)
  # CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE < 0 means allocate all unused ram.
  if(CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE LESS 0)
    message(FATAL_ERROR "Invalid configuration, "
      "disable CONFIG_COMMON_LIBC_MALLOC or "
      "set CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE to a positve value.")
  endif()
endif()
