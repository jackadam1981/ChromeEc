/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file is include by startup_MEC172x.S and must not contain
 * complex preprocessor directives.
 * We define various startup flags.
 */

/* Default stack size is 2044Bytes */
#define __STACK_SIZE 0x000007FC

/* Default heap size is 2048 */
/* #define __HEAP_SIZE 0x0000800 */

/*
 * Stack sentinel
 * If defined startup code will fill the default stack with the
 * define 32-bit value.
 */
/* #define STACK_SENTINEL */
#define STACK_SENTINEL_VAL 0x57AC5E91

/*
 * If mutliple initialized data sections are implemented then define
 * this.
 */
/* #define __STARTUP_COPY_MULTIPLE */

/*
 * If multiple BSS sections are implemented and startup code should
 * zero fill them Undefine __STARTUP_CLEAR_BSS and define this.
 */
/* #define __STARTUP_CLEAR_BSS_MULTIPLE */

/*
 * If a single BSS section is implemented and startup code should zero
 * fill it undefine __STARTUP_CLEAR_BSS_MULTIPLE and defint this.
 */
#define __STARTUP_CLEAR_BSS

/*
 * Skip calling CMSIS SystemInit before C startup code.
 */
#define __NO_SYSTEM_INIT

/*
 * The last action in startup is to call C library init which then
 * calls C main. Override the call to C library init by defining this
 * to the alternate function.
 * For example, if no C library is used one can define this to be main.
 */
#define __START main

/*
 * Application implements a default interrupt handler
 */
/* #define __DEFAULT_ISR Default_ISR */
