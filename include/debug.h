/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __DEBUG_H__

#include "console.h"

#ifndef CC_SYSTEM_my
#define CC_SYSTEM_my CC_SYSTEM
#endif

extern int cputs(enum console_channel channel, const char *outstr);
extern int cprintf(enum console_channel channel, const char *format, ...);
extern int cprints(enum console_channel channel, const char *format, ...);

/* #define CONFIG_DEBUG_CLANG */

#ifdef CONFIG_DEBUG_CLANG

#ifndef CPUTS
#define CPUTS(outstr) cputs(CC_SYSTEM_my, outstr)
#endif

#ifndef CPRINTF
#define CPRINTF(format, args...) cprintf(CC_SYSTEM_my, format, ## args)
#endif

#define CFLUSH() cflush()
#define CCPUTS(channel, format, args...) cprintf(channel, format, ## args)
#else

#ifndef CPUTS
#define CPUTS(outstr)
#endif

#ifndef CPRINTF
#define CPRINTF(format, args...)
#endif

#define CFLUSH()
#define CCPUTS(channel, format, args...)
#endif

#endif
