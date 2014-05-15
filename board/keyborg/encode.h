/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Touch data encoding/decoding */

#ifndef __KEYBORG_ENCODE_H
#define __KEYBORG_ENCODE_H

void encode_reset(void);

void encode_add_column(const uint8_t *dptr);

void encode_dump_matrix(void);

#endif  /* __KEYBORG_ENCODE_H */
