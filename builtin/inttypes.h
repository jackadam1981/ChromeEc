/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_INTTYPES_H__
#define __CROS_EC_INTTYPES_H__

#include <stdint.h>

#define PRId8     "hhd"
#define PRId16    "hd"
#define PRId32    "d"
#define PRId64    "lld"

#define PRIi8     "hhi"
#define PRIi16    "hi"
#define PRIi32    "i"
#define PRIi64    "lli"

#define PRIo8     "hho"
#define PRIo16    "ho"
#define PRIo32    "o"
#define PRIo64    "llo"

#define PRIu8     "hhu"
#define PRIu16    "hu"
#define PRIu32    "u"
#define PRIu64    "llu"

#define PRIx8     "hhx"
#define PRIx16    "hx"
#define PRIx32    "x"
#define PRIx64    "llx"

#endif /* __CROS_EC_INTTYPES_H__ */
