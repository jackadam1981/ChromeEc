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

// Get (part of) DICE handover structure: [offset .. offset + size)
size_t get_dice_handover_bytes(
    uint8_t *dest, // [OUT] destination buffer to fill
    size_t offset, // [IN] starting offset in the DICE handover structure
    size_t size // [IN] size of the DICE handover structure to copy
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __GSC_UTILS_DICE_DICE_H */