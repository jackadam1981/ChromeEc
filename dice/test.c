// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "dice.h"
#include "platform.h"
#include "cbor_dice.h"

#define DBGDUMP(name) \
    printf("%s: ", #name); \
    hexdump(name, sizeof(name)); \
    printf("\n");

static void hexdump(const uint8_t *buf, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        printf("%02x ", buf[i]);
    }
}

// static void print_digest(digest_t digest) {
//     hexdump(digest, DIGEST_BYTES);
// }

int main() {
    // uds_t uds = {0, };
    // digest_t inputs_digest = {0, };
    // slice_ref_t label = kZeroDigestSlice;
    // digest_mut_t cdi;
    uint8_t cbor_hdr[] = CFG_DESCR_LABEL_RESETTABLE;

    // if (!cdi_from_uds(uds, inputs_digest, label, cdi)) {
    //     printf("cdi_from_uds failed\n");
    //     return 1;
    // }
    // DBGDUMP(cdi);
    DBGDUMP(cbor_hdr);

    return 0;
}

