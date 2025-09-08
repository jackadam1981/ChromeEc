/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Driver for tuning IMVP9 - IMVP9.3 conroller */

#include "chipset.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "i2c.h"
#include "power_button.h"
#include "rt3645.h"

#include <zephyr/logging/log.h>

#include <ap_power/ap_power.h>
#include <power_signals.h>

#define CPRINTS(format, args...) cprints(CC_I2C, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ##args)

static int imvp_write(uint8_t reg, uint8_t val)
{
	int rv = 1;
	uint8_t data = val;
	int retry = 0;

	while ((rv != EC_SUCCESS) && (retry < 3)) {
		rv = i2c_write8(I2C_PORT_IMVP, I2C_ADDR_IMVP, reg, data);
		retry++;
	}

	CPRINTF("writing reg=0x%x val=0x%x, rv=%d\n", reg, val, rv);
	return rv;
}

static int imvp_read(uint8_t reg, uint8_t *val)
{
	int rv = 1;
	int data = 0;

	rv = i2c_read8(I2C_PORT_IMVP, I2C_ADDR_IMVP, reg, &data);

	*val = (uint8_t)data;
	CPRINTF("Read reg=0x%x val=0x%x, rv=%d\n", reg, *val, rv);
	return rv;
}

static int imvp_select_page(uint8_t page)
{
	int rv = 1;
	if ((page < IMVP_PAGE_LIMIT) && (page >= IMVP_PAGE_GLOBAL))
		rv = imvp_write(PAGE_SET_REG, page);
	return rv;
}

static int imvp_read_crc()
{
	uint8_t value = 0;

	imvp_select_page(IMVP_PAGE_RAIL_GEN);
	imvp_read((uint8_t)CRC_REG, &value);

	if (value == GOOD_CRC) {
		ccprintf("CRC is Good match - 0x%X \n", value);
		return EC_SUCCESS;
	} else {
		ccprintf("CRC is Bad match - 0x%X \n", value);
		return EC_ERROR_INVAL;
	}
}

static int imvp_check_nvm_status(int pos)
{
	uint8_t value = 0;
	int retry = 0;
	int retry_cnt;
	if ((pos == NVM_RELOAD_STAT_BIT) || (pos == NVM_STAT))
		retry_cnt = 2;
	if (pos == NVM_PRGRM_FINISH_STAT_BIT)
		retry_cnt = 20;
	while (retry < retry_cnt) {
		imvp_select_page(IMVP_PAGE_GLOBAL);
		imvp_read((uint8_t)NVM_STAT_REG, &value);
		if (pos == NVM_STAT) {
			if (value == 0xE0) {
				ccprintf("Match to NVM_STAT \n");
				break;
			}
		} else {
			if ((value >> pos) & 0x1) {
				ccprintf("Match to NVM_RELOAD/Program \n");
				break;
			}
		}

		retry++;

		k_msleep(1);
	}

	return 0;
}

static void imvp_unlock_nvm()
{
	ccprintf("imvp unlocking.. \n");
	imvp_select_page(IMVP_PAGE_GLOBAL);
	imvp_write((uint8_t)CONFIG_MODE_REG, 0x24);
	imvp_write((uint8_t)CONFIG_MODE_REG, 0x54);
	imvp_write((uint8_t)CONFIG_MODE_REG, 0x2);
	k_msleep(1);
}

static void imvp_lock_nvm()
{
	ccprintf("imvp locking.. \n");
	imvp_select_page(IMVP_PAGE_GLOBAL);
	imvp_write((uint8_t)CONFIG_MODE_REG, 0x00);
	imvp_write((uint8_t)CONFIG_MODE_REG, 0xFF);
}

static int imvp_read_product_id()
{
	uint8_t value = 0;
	imvp_select_page(IMVP_PAGE_GLOBAL);
	imvp_read((uint8_t)PRODUCT_ID_REG, &value);
	if (value == 0x45) {
		ccprintf("imvp product id read - 0x%X \n", value);
		k_msleep(1);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}

/* Write to registers in Page 5 */
static void imvp_update_VCC_VT()
{
	imvp_write(0xEF, 0x05);
	imvp_write(RLL_REG, RLL_PAGE5_VAL);
	imvp_write(DEM_SHRINK_TON_REG, DEM_SHRINK_TON_PAGE5_VAL);
	k_msleep(1);
	ccprintf("imvp updated VCC_VT \n");
}

static void imvp_update_page_RAILA_VCC_CORE()
{
	imvp_select_page(IMVP_PAGE_RAILA);
	imvp_write(ICC_MAX_REG, ICC_MAX_RAILA_VAL);
	imvp_write(ICCMAX_HC_SR_KTON_REG, ICCMAX_HC_SR_KTON_RAILA_VAL);
	imvp_write(RIMON_REG, RIMON_RAILA_VAL);
	imvp_write(RLL_REG, RLL_RAILA_VAL);
	imvp_write(DVID_ENHANCE_SPM_EN_REG, DVID_ENHANCE_SPM_EN_RAILA_VAL);
	k_msleep(1);
	ccprintf("imvp updated VCC_CORE \n");
}

static void imvp_update_page_RAILB_VCCSA()
{
	imvp_select_page(IMVP_PAGE_RAILB);
	imvp_write(ICCMAX_HC_SR_KTON_REG, ICCMAX_HC_SR_KTON_RAILB_VAL);
	imvp_write(DVID_ENHANCE_SPM_EN_REG, DVID_ENHANCE_SPM_EN_RAILB_VAL);
	k_msleep(1);
	ccprintf("imvp updated page_RAILB_VCCSA \n");
}

static void imvp_update_page_RAILC_VCCGT()
{
	imvp_select_page(IMVP_PAGE_RAILC);
	imvp_write(ICC_MAX_REG, ICC_MAX_RAILC_VAL);
	imvp_write(ICCMAX_HC_SR_KTON_REG, ICCMAX_HC_SR_KTON_RAILC_VAL);
	imvp_write(RIMON_REG, RIMON_RAILC_VAL);
	imvp_write(RLL_REG, RLL_RAILC_VAL);
	imvp_write(VID_STEP_COMP_GAIN_REG, VID_STEP_COMP_GAIN_RAILC_VAL);
	imvp_write(COMP_MODE_PZ_REG, COMP_MODE_PZ_RAILC_VAL);
	imvp_write(VSEN_COMP_LPF_REG, VSEN_COMP_LPF_RAILC_VAL);
	imvp_write(DVID_ENHANCE_SPM_EN_REG, DVID_ENHANCE_SPM_EN_RAILC_VAL);
	imvp_write(DEM_SHRINK_TON_REG, DEM_SHRINK_TON_RAILC_VAL);
	imvp_write(ZCD_VID_R_TH_REG, ZCD_VID_R_TH_RAILC_VAL);
	imvp_write(ZCD_I_TH_HYS_REG, ZCD_I_TH_HYS_RAILC_VAL);
	imvp_write(DVID_TAU_AQR_TH_REG, DVID_TAU_AQR_TH_RAILC_VAL);
	imvp_write(RIPPLE_COMP_SVID_ADDR_REG, RIPPLE_COMP_SVID_ADDR_RAILC_VAL);
	imvp_write(SVID_DCLL_REG, SVID_DCLL_RAILC_VAL);
	k_msleep(1);
	ccprintf("imvp updated page_RAILC_VCCGT \n");
}

static void imvp_update_page_RAILD_VCC_ECORE()
{
	imvp_select_page(IMVP_PAGE_RAILD);
	imvp_write(ICCMAX_HC_SR_KTON_REG, ICCMAX_HC_SR_KTON_RAILD_VAL);
	imvp_write(VSEN_COMP_LPF_REG, VSEN_COMP_LPF_RAILD_VAL);
	imvp_write(DVID_ENHANCE_SPM_EN_REG, DVID_ENHANCE_SPM_EN_RAILD_VAL);
	imvp_write(RIPPLE_COMP_SVID_ADDR_REG, RIPPLE_COMP_SVID_ADDR_RAILD_VAL);
	k_msleep(1);
	ccprintf("imvp updated page_RAILD_VCC_ECORE \n");
}

static void imvp_update_page_RAIL_GEN_DVID_PH()
{
	imvp_select_page(IMVP_PAGE_RAIL_GEN);
	imvp_write(SPM_HYS_DVIDUP_PH_A_REG, SPM_HYS_DVIDUP_PH_A_REG_VAL);
	imvp_write(DVIDUP_PH_B_DVIDDN_PH_SET_REG,
		   DVIDUP_PH_B_DVIDDN_PH_SET_REG_VAL);
	ccprintf("imvp updated page_RAIL_GEN_DVID_PH \n");
}

static void imvp_store_nvm()
{
	imvp_select_page(IMVP_PAGE_GLOBAL);
	imvp_write(NVM_PRGRM_CTRL_REG, NVM_PRGRM_DAT);
	k_msleep(800);
	ccprintf("imvp stored nvm \n");
}

static void imvp_restore_nvm()
{
	imvp_select_page(IMVP_PAGE_GLOBAL);
	imvp_write(NVM_PRGRM_CTRL_REG, NVM_RESTORE_DAT);
	k_msleep(100);
	ccprintf("imvp restored nvm \n");
}

static int imvp_update()
{
	int rv;

	ccprintf("IMVP9.3 FW udate to start...\n");
	imvp_check_nvm_status(NVM_RELOAD_STAT_BIT);

	if (!imvp_read_crc()) {
		ccprintf("CRC Matched. IMVP9.3 FW is already updated!\n");
		return EC_SUCCESS;
	}

	imvp_unlock_nvm();

	if (imvp_read_product_id()) {
		ccprintf("IMVP9.3 controller is not found! \n");
		return EC_SUCCESS;
	}

	imvp_update_VCC_VT();
	imvp_update_page_RAILA_VCC_CORE();
	imvp_update_page_RAILB_VCCSA();
	imvp_update_page_RAILC_VCCGT();
	imvp_update_page_RAILD_VCC_ECORE();
	imvp_update_page_RAIL_GEN_DVID_PH();
	imvp_store_nvm();

	if (imvp_check_nvm_status(NVM_PRGRM_FINISH_STAT_BIT)) {
		ccprintf("IMVP9.3 NVM programming is not succes. \n");
		return EC_SUCCESS;
	}
	imvp_restore_nvm();

	if (imvp_check_nvm_status(NVM_STAT)) {
		ccprintf("IMVP9.3 programming is not success. \n");
		return EC_SUCCESS;
	}

	if ((rv = imvp_read_crc())) {
		ccprintf("IMVP update failed! \n");
		imvp_lock_nvm();
		return rv;
	}
	imvp_lock_nvm();
	ccprintf("IMVP FW is updated successfully and IMVP is locked!\n");

	return EC_SUCCESS;
}

static int command_imvp_update(int argc, const char **argv)
{
	ccprintf("Initiating to G3 for IMVP update!\n");
	chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);
	ccprintf("set Arail on\n");
	power_signal_set(PWR_EN_PP3300_A, 1);
	imvp_update();
	power_signal_set(PWR_EN_PP3300_A, 0);
	ccprintf("Press Powerbutton or use pwrbtn command to progress!\n");
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(imvp_update, command_imvp_update, NULL,
			"update imvp9.3 firmware");

#ifdef CONFIG_IMVP_FACTORY_UPDATE
static void imvp_update_handler(struct ap_power_ev_callback *cb,
				struct ap_power_ev_data data)
{
	static int imvp_updated;
	switch (data.event) {
	case AP_POWER_PRE_INIT:

		if (imvp_updated == 0) {
			ccprintf("Imvp update required\n");
			if (chipset_in_state(CHIPSET_STATE_SOFT_OFF)) {
				if (!imvp_update()) {
					imvp_updated = 1;
					ccprintf("Imvp_updated success! \n ");
				}
			}
		} else
			ccprintf("Imvp not required to update \n");
		break;
	default:
		ccprintf("unhandled power event %d\n", data.event);
		break;
	}
}

test_export_static void impv_cb_init()
{
	static struct ap_power_ev_callback cb;
	ap_power_ev_init_callback(&cb, imvp_update_handler, AP_POWER_PRE_INIT);
	ap_power_ev_add_callback(&cb);
	ccprintf("Imvp callback init\n");
}
DECLARE_HOOK(HOOK_INIT, impv_cb_init, HOOK_PRIO_LAST);
#endif
