/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A CROS_EC_COMMAND macro must be defined before including this file
 * with the following signature:
 *
 * int CROS_EC_COMMAND(int command, int version,
 *		    const void *outdata, int outsize,
 *		    void *indata, int insize)
 */

#include "ec_cmd_api-generated.h"
