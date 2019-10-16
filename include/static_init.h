/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef STATIC_INIT_VALUE
#error "'STATIC_INIT_VALUE' required"
#endif

#ifndef STATIC_INIT_COUNT
#error "'STATIC_INIT_COUNT' required"
#endif

#define INIT_1 STATIC_INIT_VALUE,
#if STATIC_INIT_COUNT & 1
INIT_1
#endif
#define INIT_2 INIT_1 INIT_1
#if STATIC_INIT_COUNT & 2
INIT_2
#endif
#define INIT_4 INIT_2 INIT_2
#if STATIC_INIT_COUNT & 4
INIT_4
#endif
#define INIT_8 INIT_4 INIT_4
#if STATIC_INIT_COUNT & 8
INIT_8
#endif
#define INIT_16 INIT_8 INIT_8
#if STATIC_INIT_COUNT & 16
INIT_16
#endif
#define INIT_32 INIT_16 INIT_16
#if STATIC_INIT_COUNT & 32
INIT_32
#endif
#define INIT_64 INIT_32 INIT_32
#if STATIC_INIT_COUNT & 64
INIT_64
#endif
#define INIT_128 INIT_64 INIT_64
#if STATIC_INIT_COUNT & 128
INIT_128
#endif
#define INIT_256 INIT_128 INIT_128
#if STATIC_INIT_COUNT & 256
INIT_256
#endif
#define INIT_512 INIT_256 INIT_256
#if STATIC_INIT_COUNT & 512
INIT_512
#endif
#define INIT_1024 INIT_512 INIT_512
#if STATIC_INIT_COUNT & 1024
INIT_1024
#endif

#if STATIC_INIT_COUNT >= 65536
#error "Init list too long, add more init blocks"
#endif
