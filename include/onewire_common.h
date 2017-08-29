/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Standard speed; all timings padded by 2 usec for safety.
 *
 * Note that these timing are actually _longer_ than legacy 1-wire standard
 * speed because we're running the 1-wire bus at 3.3V instead of 5V.
 */
#define T_RSTL 602  /* Reset low pulse; 600-960 us */
#define T_MSP   72  /* Presence detect sample time; 70-75 us */
#define T_RSTH (68 + 260 + 5 + 2) /* Reset high; tPDHmax + tPDLmax + tRECmin */
#define T_SLOT  70  /* Timeslot; >67 us */
#define T_W0L   63  /* Write 0 low; 62-120 us */
#define T_W1L    7  /* Write 1 low; 5-15 us */
#define T_RL     7  /* Read low; 5-15 us */
#define T_MSR   15  /* Read sample time; <15 us.  Must be at least 200 ns after
                    * T_RL since that's how long the signal takes to be pulled
                    * up on our board.  */
