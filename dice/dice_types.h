// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __GSC_UTILS_DICE_DICE_TYPES_H
#define __GSC_UTILS_DICE_DICE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DIGEST_BYTES        32
#define UDS_BYTES           DIGEST_BYTES
#define CDI_BYTES           DIGEST_BYTES
#define ECDSA_POINT_BYTES   32
#define ECDSA_SIG_BYTES     (2 * ECDSA_POINT_BYTES) /* 32 byte R + 32 byte S */

// UDS_ID and CDI_ID sizes
#define DICE_ID_BYTES       20
#define DICE_ID_HEX_BYTES   (DICE_ID_BYTES * 2)

typedef const uint8_t digest_t[DIGEST_BYTES];
typedef digest_t uds_t;
typedef digest_t cdi_t;

typedef uint8_t digest_mut_t[DIGEST_BYTES];
typedef digest_mut_t uds_mut_t;
typedef digest_mut_t cdi_mut_t;

typedef const uint8_t dice_id_t[DICE_ID_BYTES];
typedef uint8_t dice_id_mut_t[DICE_ID_BYTES];
typedef struct {
    size_t size;
    uint8_t *data;
} slice_mut_t;
typedef struct {
    const size_t size;
    const uint8_t *data;
} slice_ref_t;

#define uds_as_slice(uds) { UDS_BYTES, uds }
#define digest_as_slice(digest) { DIGEST_BYTES, digest }
#define digest_as_slice_mut(digest) { DIGEST_BYTES, digest }

typedef struct {
    uint8_t x[ECDSA_POINT_BYTES];
    uint8_t y[ECDSA_POINT_BYTES];
} ecdsa_public_t;

typedef const uint8_t ecdsa_sig_t[ECDSA_SIG_BYTES];
typedef uint8_t ecdsa_sig_mut_t[ECDSA_SIG_BYTES];

// typedef struct {
//     const size_t qty_segments;
//     slice_ref_t segments[];
// } sgl_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __GSC_UTILS_DICE_DICE_TYPES_H */