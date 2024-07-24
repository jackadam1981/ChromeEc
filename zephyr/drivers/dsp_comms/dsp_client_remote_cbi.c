/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/dsp/client.h"
#include "cros_board_info.h"

#include <zephyr/kernel.h>

int cbi_get_board_info(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
    return cbi_remote_get_board_info(tag, buf, size);
}
