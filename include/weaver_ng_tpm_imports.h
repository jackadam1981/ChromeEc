/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Compatibility layer between the TPM code and weaver_ng.
 *
 * This is needed because the headers for the TPM are not compatible with the
 * headers used by weaver_ng.c. It also makes it easier to mock the
 * functionality derived from the TPM code.
 */

#ifndef __CROS_EC_WEAVER_NG_TPM_IMPORTS_H
#define __CROS_EC_WEAVER_NG_TPM_IMPORTS_H

#include <stdint.h>

uint32_t get_restart_count(void);

#endif  /* __CROS_EC_WEAVER_NG_TPM_IMPORTS_H */
