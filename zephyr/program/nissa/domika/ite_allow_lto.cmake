# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(DEFINED CONFIG_BOARD_DOMIKA)
  # Define a function that returns the kernel LTO allow list
  function(kernel_lto_allow_list out_var)
    set(${out_var} timer.c PARENT_SCOPE)
  endfunction()
endif()
