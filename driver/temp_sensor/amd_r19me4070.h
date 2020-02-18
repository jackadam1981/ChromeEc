/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPU R19ME4070 configuration */

#ifndef __CROS_EC_R19ME4070_H
#define __CROS_EC_R19ME4070_H

/* Baseboard features */
#include "baseboard.h"

/* GPU features */
#define R19M14017_LOCAL                 0
/* If you want to use I2C_PORT_GPU , must assign I2C channel in board.h*/
#ifndef I2C_PORT_GPU
#define I2C_PORT_GPU
#endif

/*
 * get GPU temperature value and move to *temp_ptr
 * One second trigger ,Use I2C read GPU's Die temperature.
 */
int get_temp_R19M14017(int idx, int *temp_ptr);

/*
 * Use I2C Write GPU register,do this function before
 * read GPU temperature .
 */
void gpu_init_temp_sensor(void);

#endif /* __CROS_EC_AMD_R19ME4070_H */
