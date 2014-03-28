/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Drive a GPIB power supply */

#ifndef __GPIB_H
#define __GPIB_H

void gpib_set_voltage(int mv);

void gpib_set_current(int ma);

void gpib_set_output(int en);

#endif /* __GPIB_H */
