/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "intel_rvp_board_id.h"

#define DT_DRV_COMPAT intel_rvp_board_id


#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
const struct gpio_dt_spec bom_id_config[] = {
	RVP_ID_CONFIG_LIST(DT_NODELABEL(rvp_board_id), bom_gpios)
};

const struct gpio_dt_spec fab_id_config[] = {
	RVP_ID_CONFIG_LIST(DT_NODELABEL(rvp_board_id), fab_gpios)
};

const struct gpio_dt_spec board_id_config[] = {
	RVP_ID_CONFIG_LIST(DT_NODELABEL(rvp_board_id), board_gpios)
};
#endif /* #if DT_HAS_COMPAT_STATUS_OKAY */
