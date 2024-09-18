// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __GSC_UTILS_DICE_DICE_H
#define __GSC_UTILS_DICE_DICE_H

#include "dice_types.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dice_context_s {
    uds_t uds;
    uint32_t aprov_status;
    uint32_t sec_ver;
    uint8_t hidden_digest[DIGEST_BYTES];
    uint8_t code_digest[DIGEST_BYTES];
    uint8_t pcr0[DIGEST_BYTES];
    bool initialized;
} dice_context_t;

// Resets dice context on initial boot or AP reboot
void reset_dice_context(
    dice_context_t *dice_ctx // [OUT] dice context
);

// Gets attestation CDI
bool get_cdi_attest(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    digest_mut_t cdi // [OUT] CDI
);

// Gets sealing CDI
bool get_cdi_seal(
    platform_context_t ctx, // [IN] opaque platform context
    dice_context_t *dice_ctx, // [IN/OUT] dice context
    digest_mut_t cdi // [OUT] CDI
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __GSC_UTILS_DICE_DICE_H */