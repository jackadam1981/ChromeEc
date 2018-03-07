/*
 * Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This file contains npcx generic types, macros, constants,
 * utilities and error codes.
 */

#ifndef __DEFS_LINUX_H__
#define __DEFS_LINUX_H__

/*--------------------------------------------------------------------------
 * GENERIC TYPES DEFINITIONS
 *--------------------------------------------------------------------------
 */

#define BOOLEAN int
#define UINT8 unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
#define INT32 signed int
#define WORD unsigned short
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

#endif /* __DEFS_LINUX_H__ */
