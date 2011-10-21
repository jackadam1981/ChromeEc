/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is the main function of Chrome OS EC core.
 * Called by platform-dependent main function.
 *
 */
#include "cros_ec/include/ec_common.h"
#include "cros_ec/include/core.h"

EcError CoreMain() {
  EcError ret;

  ret = CoreKeyboardInit();
  if (ret != EC_SUCCESS) {
    printf("CoreKeyboardInit() failed: %d\n", ret);
    return ret;
  }

  return EC_SUCCESS;
}
