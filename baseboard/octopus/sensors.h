/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common Octopus sensor functions */

#ifdef OCTOPUS_SENSOR_NCP15WB_51_47
int get_ncp15wb_51_47_temp(int adc, int *temp_ptr);
#endif

#ifdef OCTOPUS_SENSOR_NCP15WB_13_47
int get_ncp15wb_13_47_temp(int adc, int *temp_ptr);
#endif
