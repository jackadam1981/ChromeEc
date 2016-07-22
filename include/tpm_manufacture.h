/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This header declares the TPM manufacture related interface.
 * Individual boards are expected to provide implementations.
 */

#ifndef __CROS_EC_TPM_MANUFACTURE_H
#define __CROS_EC_TPM_MANUFACTURE_H

#define ENDORSE_RSA_CERT_OK    0x00000001
#define ENDORSE_ECC_CERT_OK    0x00000002
#define ENDORSE_SUCCESS        \
		(ENDORSE_RSA_CERT_OK | ENDORSE_ECC_CERT_OK)

/* Returns an int with bits set according to the ENDORSE #defines
 * above.  Callers may compare with ENDORSE_SUCCESS to ensure endorse
 * completion.
 */
int tpm_manufactured(void);
/* Returns non-zero if TPM endorsement initialization succeeds. */
int tpm_endorse(void);

#endif	/* __CROS_EC_TPM_MANUFACTURE_H */
