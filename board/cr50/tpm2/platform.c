/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Platform.h"
#include "TPM_Types.h"

#include "trng.h"

UINT16 _cpri__GenerateRandom(INT32 randomSize,
			     BYTE *buffer)
{
	rand_bytes(buffer, randomSize);
	return randomSize;
}
