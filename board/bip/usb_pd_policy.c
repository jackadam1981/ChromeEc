/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "usbc_ppc.h"

int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}
