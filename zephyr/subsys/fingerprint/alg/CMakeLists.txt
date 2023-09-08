# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if(DEFINED CONFIG_CROS_FP_FPC_BEP_ALGORITHM)
  zephyr_library_sources(fpc_bep_bio_alg.c)
  zephyr_library_sources_ifndef(CONFIG_HAVE_BEP_PRIVATE_DRIVER	fpc_bep_mock.c)
endif()
