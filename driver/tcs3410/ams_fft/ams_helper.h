/* Copyright © 2022 ams-OSRAM AG
 * All rights are reserved.
 *
 * Use of this source code is governed by a license that can be
 * found in the LICENSE.txt file.
 *
 */

#ifndef __AMS_HELPER_H__
#define __AMS_HELPER_H__

#include <stdio.h>
#include <stdint.h>

int ams_normalize_fft_data(uint16_t *input, uint16_t *output, uint32_t size);

#endif // __AMS_HELPER_H__

