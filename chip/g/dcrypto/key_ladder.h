/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_CHIP_G_DCRYPTO_KEY_LADDER_H
#define __EC_CHIP_G_DCRYPTO_KEY_LADDER_H

#define KEYMGR_CERT_8 8

uint32_t dcrypto_key_ladder_step(uint32_t cert);

#endif /* ! __EC_CHIP_G_DCRYPTO_KEY_LADDER_H */
