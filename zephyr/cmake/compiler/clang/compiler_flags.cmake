# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include("${ZEPHYR_BASE}/cmake/compiler/clang/compiler_flags.cmake")

# Disable -fno-freestanding.
set_compiler_property(PROPERTY hosted)

# Disable position independent code.
add_compile_options(-fno-PIC)

# Conditionally enable debugging and disable prelink optimization
# TODO(b/) Create add_compile_options_ifdef (multiple options) upstream
add_compile_option_ifdef(CONFIG_PLATFORM_EC_DEBUG_SYMBOLS -g)
add_compile_option_ifdef(CONFIG_PLATFORM_EC_DEBUG_SYMBOLS
  --include=${ZEPHYR_EC_MODULE_DIR}/common/disable_clang_optimizations.h)

check_set_compiler_property(APPEND PROPERTY warning_extended -Wunused-variable
	-Werror=unused-variable -Werror=missing-braces
	-Werror=sometimes-uninitialized -Werror=unused-function
	-Werror=array-bounds)
