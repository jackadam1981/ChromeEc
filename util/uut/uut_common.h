/*
 * Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This file contains npcx generic types, macros, constants,
 * utilities and error codes.
 */

#ifndef __UUT_COMMON_H__
#define __UUT_COMMON_H__

/*--------------------------------------------------------------------------
 * UTILITY MACROS
 *-------------------------------------------------------------------------
 */
 /* Extracting Byte - 8 bit: MSB, LSB */
#define MSB(u16) ((uint8_t)((uint16_t)(u16) >> 8))
#define LSB(u16) ((uint8_t)(u16))

/*--------------------------------------------------------------------------
 * GENERIC TYPES DEFINITIONS
 *--------------------------------------------------------------------------
 */
#define BOOLEAN int
#define FP64 double

/*--------------------------------------------------------------------------
 * CONSTANTS
 *--------------------------------------------------------------------------
 */
#ifndef FALSE
#define FALSE (BOOLEAN)0
#endif

#ifndef TRUE
#define TRUE (BOOLEAN)1
#endif

#ifndef NULL
#define NULL 0
#endif

#define ENABLE 1
#define DISABLE 0

#endif /* __UUT_COMMON_H__ */
