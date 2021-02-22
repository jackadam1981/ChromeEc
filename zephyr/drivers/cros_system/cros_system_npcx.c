/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_system.h>
#include <logging/log.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

/* Driver config */
struct cros_system_npcx_config {
	/* hardware module base address */
	uintptr_t base_scfg;
	uintptr_t base_twd;
};

/* Driver data */
struct cros_system_npcx_data {
	int reset; /* reset cause */
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct cros_system_npcx_config *)(dev)->config)

#define HAL_SCFG_INST(dev) (struct scfg_reg *)(DRV_CONFIG(dev)->base_scfg)
#define HAL_TWD_INST(dev) (struct twd_reg *)(DRV_CONFIG(dev)->base_twd)

#define DRV_DATA(dev) ((struct cros_system_npcx_data *)(dev)->data)

static int cros_system_npcx_get_reset_cause(const struct device *dev)
{
	struct cros_system_npcx_data *data = DRV_DATA(dev);

	return data->reset;
}

static int cros_system_npcx_init(const struct device *dev)
{
	struct scfg_reg *const inst_scfg = HAL_SCFG_INST(dev);
	struct twd_reg *const inst_twd = HAL_TWD_INST(dev);
	struct cros_system_npcx_data *data = DRV_DATA(dev);

	/* check reset cause */
	if (IS_BIT_SET(inst_twd->T0CSR, NPCX_T0CSR_WDRST_STS)) {
		data->reset = WATCHDOG_RST;
		inst_twd->T0CSR |= BIT(NPCX_T0CSR_WDRST_STS);
	} else if (IS_BIT_SET(inst_scfg->RSTCTL, NPCX_RSTCTL_DBGRST_STS)) {
		data->reset = DEBUG_RST;
		inst_scfg->RSTCTL |= BIT(NPCX_RSTCTL_DBGRST_STS);
	} else if (IS_BIT_SET(inst_scfg->RSTCTL, NPCX_RSTCTL_VCC1_RST_STS)) {
		data->reset = VCC1_RST_PIN;
	} else {
		data->reset = POWERUP;
	}

	return 0;
}

static struct cros_system_npcx_data cros_system_npcx_dev_data;

static const struct cros_system_npcx_config cros_system_dev_cfg = {
	.base_scfg = DT_REG_ADDR(DT_INST(0, nuvoton_npcx_pinctrl)),
	.base_twd = DT_REG_ADDR(DT_INST(0, nuvoton_npcx_watchdog)),
};

static const struct cros_system_driver_api cros_system_driver_npcx_api = {
	.get_reset_cause = cros_system_npcx_get_reset_cause,
};

/*
 * The priority of cros_system_npcx_init() should be lower than watchdog init
 * for reset cause check.
 */
DEVICE_DEFINE(cros_system_npcx_0, "CROS_SYSTEM", cros_system_npcx_init, NULL,
	      &cros_system_npcx_dev_data, &cros_system_dev_cfg, PRE_KERNEL_1,
	      30, &cros_system_driver_npcx_api);

#define HAL_DBG_REG_BASE_ADDR \
	(struct dbg_reg *)DT_REG_ADDR(DT_INST(0, nuvoton_npcx_cros_dbg))

#define DBG_NODE           DT_NODELABEL(dbg)
#define DBG_PINCTRL_PH     DT_PHANDLE_BY_IDX(DBG_NODE, pinctrl_0, 0)
#define DBG_ALT_FILED(f)   DT_PHA_BY_IDX(DBG_PINCTRL_PH, alts, 0, f)

static int jtag_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	struct dbg_reg *const dbg_reg_base = HAL_DBG_REG_BASE_ADDR;
#if DT_NODE_HAS_STATUS(DT_NODELABEL(dbg), okay)
	const struct npcx_alt jtag_alts[] =
	{
		{
			.group = DBG_ALT_FILED(group),
			.bit = DBG_ALT_FILED(bit),
			.inverted = DBG_ALT_FILED(inv)
		}
	};
#endif

	dbg_reg_base->DBGCTRL = 0x04;
	dbg_reg_base->DBGFRZEN3 &= ~BIT(NPCX_DBGFRZEN3_GLBL_FRZ_DIS);
#if DT_NODE_HAS_STATUS(DT_NODELABEL(dbg), okay)
	npcx_pinctrl_mux_configure(jtag_alts, 1, 1);
#endif

	return 0;
}
SYS_INIT(jtag_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT + 1);
