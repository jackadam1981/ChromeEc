/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor_auth_secrets.h"

/* The GSC pairing key. */
static PairingKey pairing_key;

PairingKey &get_pairing_key()
{
	return pairing_key;
}
