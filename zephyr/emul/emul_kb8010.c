/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/retimer/kb8010.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_kb8010.h"
#include "emul/emul_stub_device.h"

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT kandou_kb8010

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
LOG_MODULE_REGISTER(emul_kb8010);

/** Static configuration for the emulator */
struct kb8010_emul_cfg {
	/** I2C address of emulator */
	uint16_t addr;
};

/** Run-time data used by the emulator */
struct kb8010_emul_data {
	/** Current state of all emulated KB8010 retimer registers */
	uint8_t protocol_reg;
	uint8_t orientation_reg;
	uint8_t reset_reg;
	uint8_t xbar_ovr_reg;
	uint8_t xbar_eb1sel_reg;
	uint8_t xbar_eb2sel_reg;
	uint8_t xbar_eb4sel_reg;
	uint8_t xbar_eb5sel_reg;
	uint8_t xbar_txasel_reg;
	uint8_t xbar_txbsel_reg;
	uint8_t xbar_txcsel_reg;
	uint8_t xbar_tsdsel_reg;
	uint8_t xbar_sbu_cfg_reg;
	uint8_t dp_d_ieee_oui_reg;
	uint8_t dp_d_func_1_reg;
	uint8_t dp_d_func_2_reg;
	uint8_t dp_l_eq_cfg_reg;
	uint8_t dfp_reply_timeout_reg;
	uint8_t cio_cfg_wakeup_ign_ls_det_reg;
	uint8_t sbbr_comrx_ch_0_link_ctrl_run3_reg;
	uint8_t sbbr_comrx_ch_1_link_ctrl_run3_reg;
	uint8_t sbbr_comrx_ch_shared_link_ctrl_run_offset_reg;
	uint8_t sbbr_comrx_ch_shared_link_ctrl_run_post_cdr_offset_reg;
	uint8_t sbbr_comrx_azc_ctrl_ctle_oc_bw_stg1_reg;
	uint8_t sbbr_comrx_azc_ctrl_ctle_oc_bw_stg2_reg;
	uint8_t sbbr_comrx_azc_ctrl_ctle_oc_bw_stg3_reg;
	uint8_t sbbr_comrx_lfps_lfps_ctrl_reg;
	uint8_t sbbr_comtx_output_driver_misc_ovr_en_reg;
	uint8_t sbbr_br_rx_cal_offset_eye_bg_sat_ovf_reg;
	uint8_t sbbr_br_rx_cal_vga2_gxr_reg;
};

/* Workhorse for mapping i2c reg to internal emulator data access */
static uint8_t *kb8010_emul_get_reg_ptr(struct kb8010_emul_data *data, int reg)
{
	switch (reg) {
	case KB8010_REG_PROTOCOL:
		return &(data->protocol_reg);
	case KB8010_REG_ORIENTATION:
		return &(data->orientation_reg);
	case KB8010_REG_RESET:
		return &(data->reset_reg);
	case KB8010_REG_XBAR_OVR:
		return &(data->xbar_ovr_reg);
	case KB8010_REG_XBAR_EB1SEL:
		return &(data->xbar_eb1sel_reg);
	case KB8010_REG_XBAR_EB2SEL:
		return &(data->xbar_eb2sel_reg);
	case KB8010_REG_XBAR_EB4SEL:
		return &(data->xbar_eb4sel_reg);
	case KB8010_REG_XBAR_EB5SEL:
		return &(data->xbar_eb5sel_reg);
	case KB8010_REG_XBAR_TXASEL:
		return &(data->xbar_txasel_reg);
	case KB8010_REG_XBAR_TXBSEL:
		return &(data->xbar_txbsel_reg);
	case KB8010_REG_XBAR_TXCSEL:
		return &(data->xbar_txcsel_reg);
	case KB8010_REG_XBAR_TXDSEL:
		return &(data->xbar_tsdsel_reg);
	case KB8010_REG_XBAR_SBU_CFG:
		return &(data->xbar_sbu_cfg_reg);
	case KB8010_REG_DP_D_IEEE_OUI:
		return &(data->dp_d_ieee_oui_reg);
	case KB8010_REG_DP_D_FUNC_1:
		return &(data->dp_d_func_1_reg);
	case KB8010_REG_DP_D_FUNC_2:
		return &(data->dp_d_func_2_reg);
	case KB8010_REG_DP_L_EQ_CFG:
		return &(data->dp_l_eq_cfg_reg);
	case KB8010_REG_DFP_REPLY_TIMEOUT:
		return &(data->dfp_reply_timeout_reg);
	case KB8010_REG_CIO_CFG_WAKEUP_IGN_LS_DET:
		return &(data->cio_cfg_wakeup_ign_ls_det_reg);
	case KB8010_REG_SBBR_COMRX_CH_0_LINK_CTRL_RUN3:
		return &(data->sbbr_comrx_ch_0_link_ctrl_run3_reg);
	case KB8010_REG_SBBR_COMRX_CH_1_LINK_CTRL_RUN3:
		return &(data->sbbr_comrx_ch_1_link_ctrl_run3_reg);
	case KB8010_REG_SBBR_COMRX_CH_SHARED_LINK_CTRL_RUN_OFFSET:
		return &(data->sbbr_comrx_ch_shared_link_ctrl_run_offset_reg);
	/* clang-format off */
	case KB8010_REG_SBBR_COMRX_CH_SHARED_LINK_CTRL_RUN_POST_CDR_OFFSET:
		return &(
			data->sbbr_comrx_ch_shared_link_ctrl_run_post_cdr_offset_reg);
	/* clang-format on */
	case KB8010_REG_SBBR_COMRX_AZC_CTRL_CTLE_OC_BW_STG1:
		return &(data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg1_reg);
	case KB8010_REG_SBBR_COMRX_AZC_CTRL_CTLE_OC_BW_STG2:
		return &(data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg2_reg);
	case KB8010_REG_SBBR_COMRX_AZC_CTRL_CTLE_OC_BW_STG3:
		return &(data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg3_reg);
	case KB8010_REG_SBBR_COMRX_LFPS_LFPS_CTRL:
		return &(data->sbbr_comrx_lfps_lfps_ctrl_reg);
	case KB8010_REG_SBBR_COMTX_OUTPUT_DRIVER_MISC_OVR_EN:
		return &(data->sbbr_comtx_output_driver_misc_ovr_en_reg);
	case KB8010_REG_SBBR_BR_RX_CAL_OFFSET_EYE_BG_SAT_OVF:
		return &(data->sbbr_br_rx_cal_offset_eye_bg_sat_ovf_reg);
	case KB8010_REG_SBBR_BR_RX_CAL_VGA2_GXR:
		return &(data->sbbr_br_rx_cal_vga2_gxr_reg);

	default:
		__ASSERT(false, "Unimplemented Register Access Error on 0x%x",
			 reg);
		/* Statement never reached, required for compiler warnings */
		return NULL;
	}
}

/** Check description in emul_kb8010.h */
void kb8010_emul_set_reg(const struct emul *emul, int reg, uint8_t val)
{
	struct kb8010_emul_data *data = emul->data;

	uint8_t *reg_to_write = kb8010_emul_get_reg_ptr(data, reg);
	*reg_to_write = val;
}

/** Check description in emul_kb8010.h */
uint8_t kb8010_emul_get_reg(const struct emul *emul, int reg)
{
	struct kb8010_emul_data *data = emul->data;
	uint8_t *reg_to_read = kb8010_emul_get_reg_ptr(data, reg);

	return *reg_to_read;
}

static int kb8010_emul_write_byte(const struct emul *emul, int reg, uint8_t val)
{
	struct kb8010_emul_data *data = emul->data;

	uint8_t *reg_to_write = kb8010_emul_get_reg_ptr(data, reg);
	*reg_to_write = val;

	return 0;
}

static int kb8010_emul_read_byte(const struct emul *emul, int reg, uint8_t *val)
{
	struct kb8010_emul_data *data = emul->data;
	uint8_t *reg_to_read = kb8010_emul_get_reg_ptr(data, reg);

	*val = *reg_to_read;

	return 0;
}

/** Check description in emul_kb8010.h */
void kb8010_emul_reset(const struct emul *emul)
{
	struct kb8010_emul_data *data;

	data = emul->data;

	data->protocol_reg = 0x00;
	data->orientation_reg = 0x00;
	data->reset_reg = 0x00;
	data->xbar_ovr_reg = 0x00;
	data->xbar_eb1sel_reg = 0x00;
	data->xbar_eb2sel_reg = 0x00;
	data->xbar_eb4sel_reg = 0x00;
	data->xbar_eb5sel_reg = 0x00;
	data->xbar_txasel_reg = 0x00;
	data->xbar_txbsel_reg = 0x00;
	data->xbar_txcsel_reg = 0x00;
	data->xbar_tsdsel_reg = 0x00;
	data->xbar_sbu_cfg_reg = 0x00;
	data->dp_d_ieee_oui_reg = 0x00;
	data->dp_d_func_1_reg = 0x00;
	data->dp_d_func_2_reg = 0x00;
	data->dp_l_eq_cfg_reg = 0x00;
	data->dfp_reply_timeout_reg = 0x00;
	data->cio_cfg_wakeup_ign_ls_det_reg = 0x00;
	data->sbbr_comrx_ch_shared_link_ctrl_run_offset_reg = 0x00;
	data->sbbr_comrx_ch_shared_link_ctrl_run_post_cdr_offset_reg = 0x00;
	data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg1_reg = 0x00;
	data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg2_reg = 0x00;
	data->sbbr_comrx_azc_ctrl_ctle_oc_bw_stg3_reg = 0x00;
	data->sbbr_comrx_lfps_lfps_ctrl_reg = 0x00;
	data->sbbr_comtx_output_driver_misc_ovr_en_reg = 0x00;
	data->sbbr_br_rx_cal_offset_eye_bg_sat_ovf_reg = 0x00;
	data->sbbr_br_rx_cal_vga2_gxr_reg = 0x00;
}

static int kb8010_emul_i2c_transfer(const struct emul *emul,
				    struct i2c_msg *msgs, int num_msgs,
				    int addr)
{
	__ASSERT_NO_MSG(msgs && num_msgs);

	i2c_dump_msgs("emul", msgs, num_msgs, addr);

	if (num_msgs == 1) {
		if (((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == 3)) {
			uint16_t reg = (msgs[0].buf[0] << 8) | msgs[0].buf[1];

			return kb8010_emul_write_byte(emul, reg,
						      msgs[0].buf[2]);
		}
		LOG_ERR("Unexpected write msgs");
		return -EIO;
	} else if (num_msgs == 2) {
		if (((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == 2) &&
		    ((msgs[1].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) &&
		    (msgs[1].len == 1)) {
			uint16_t reg = (msgs[0].buf[0] << 8) | msgs[0].buf[1];

			return kb8010_emul_read_byte(emul, reg, msgs[1].buf);
		}
		LOG_ERR("Unexpected read msgs");
		return -EIO;
	}

	LOG_ERR("Unexpected num_msgs");
	return -EIO;
}

static const struct i2c_emul_api kb8010_i2c_emul_api = {
	.transfer = kb8010_emul_i2c_transfer,
};

/* Device instantiation */

/**
 * @brief Set up a new KB8010 retimer emulator
 *
 * This should be called for each KB8010 retimer device that needs to be
 * emulated.
 *
 * @param emul Emulation information
 * @param parent Device to emulate
 *
 * @return 0 indicating success (always)
 */
static int kb8010_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	ARG_UNUSED(emul);
	ARG_UNUSED(parent);

	kb8010_emul_reset(emul);

	return 0;
}

struct i2c_common_emul_data *
emul_kb8010_get_i2c_common_data(const struct emul *emul)
{
	return emul->data;
}

#define KB8010_EMUL(n)                                                  \
	static struct kb8010_emul_data kb8010_emul_data_##n;            \
	static const struct kb8010_emul_cfg kb8010_emul_cfg_##n = {     \
		.addr = DT_INST_REG_ADDR(n),                            \
	};                                                              \
	EMUL_DT_INST_DEFINE(n, kb8010_emul_init, &kb8010_emul_data_##n, \
			    &kb8010_emul_cfg_##n, &kb8010_i2c_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(KB8010_EMUL);

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
