/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * IDT P9221-R7 Wireless Power Receiver driver.
 */

#include "p9221_r7.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define P9221_TX_TIMEOUT_MS		(20 * 1000*1000)
#define P9221_DCIN_TIMEOUT_MS		(2 * 1000*1000)
#define P9221_VRECT_TIMEOUT_MS		(2 * 1000*1000)
#define P9221_NOTIFIER_DELAY_MS		80*1000
#define P9221r7_ILIM_MAX_UA		(1600 * 1000)
#define P9221r7_OVER_CHECK_NUM		3

#define OVC_LIMIT			1
#define OVC_THRESHOLD			1400000
#define OVC_BACKOFF_LIMIT		900000
#define OVC_BACKOFF_AMOUNT		100000

/* P9221  parameters */
static struct wpc_charger_info p9221_charger_info = {
	.online = false,
	.cust_id = 0,
	.i2c_port = I2C_PORT_WPC,
	.pp_buf_valid = false,
	.rx_len = 0,
	.rx_done = 0,
	.tx_id = 0,
	.tx_len = 0,
	.tx_done = 0,
	.tx_busy = 0,
	.p9221_check_vbus = 0,
	.p9221_check_det = 0,
	.last_capacity = 0,
	.vbus_status = 0,
};

struct wpc_charger_info *wpc_get_charger_info(void)
{
	return &p9221_charger_info;
}

static int p9221_is_r7(struct wpc_charger_info *wpc)
{
	return wpc->cust_id == P9221r7_CUSTOMER_ID_VAL;
}

static void p9221_set_offline(struct wpc_charger_info *wpc);

static const uint32_t p9221_ov_set_lut[] = {
	17000000, 20000000, 15000000, 13000000,
	11000000, 11000000, 11000000, 11000000
};

static int p9221_reg_is_8_bit(struct wpc_charger_info *wpc, uint16_t reg)
{
	if (p9221_is_r7(wpc)) {
		switch (reg) {
		case P9221_CHIP_REVISION_REG:
		case P9221r7_VOUT_SET_REG:
		case P9221r7_ILIM_SET_REG:
		case P9221r7_CHARGE_STAT_REG:
		case P9221r7_EPT_REG:
		case P9221r7_SYSTEM_MODE_REG:
		case P9221r7_COM_CHAN_RESET_REG:
		case P9221r7_COM_CHAN_SEND_SIZE_REG:
		case P9221r7_COM_CHAN_SEND_IDX_REG:
		case P9221r7_COM_CHAN_RECV_SIZE_REG:
		case P9221r7_COM_CHAN_RECV_IDX_REG:
		case P9221r7_DEBUG_REG:
		case P9221r7_EPP_Q_FACTOR_REG:
		case P9221r7_EPP_TX_GUARANTEED_POWER_REG:
		case P9221r7_EPP_TX_POTENTIAL_POWER_REG:
		case P9221r7_EPP_TX_CAPABILITY_FLAGS_REG:
		case P9221r7_EPP_RENEGOTIATION_REG:
		case P9221r7_EPP_CUR_RPP_HEADER_REG:
		case P9221r7_EPP_CUR_NEGOTIATED_POWER_REG:
		case P9221r7_EPP_CUR_MAXIMUM_POWER_REG:
		case P9221r7_EPP_CUR_FSK_MODULATION_REG:
		case P9221r7_EPP_REQ_RPP_HEADER_REG:
		case P9221r7_EPP_REQ_NEGOTIATED_POWER_REG:
		case P9221r7_EPP_REQ_MAXIMUM_POWER_REG:
		case P9221r7_EPP_REQ_FSK_MODULATION_REG:
		case P9221r7_VRECT_TARGET_REG:
		case P9221r7_VRECT_KNEE_REG:
		case P9221r7_FOD_SECTION_REG:
		case P9221r7_VRECT_ADJ_REG:
		case P9221r7_ALIGN_X_ADC_REG:
		case P9221r7_ALIGN_Y_ADC_REG:
		case P9221r7_ASK_MODULATION_DEPTH_REG:
		case P9221r7_OVSET_REG:
		case P9221r7_EPP_TX_SPEC_REV_REG:
			return true;
		default:
			return false;
		}
	}

	switch (reg) {
	case P9221_CHIP_REVISION_REG:
	case P9221_CUSTOMER_ID_REG:
	case P9221_CHARGE_STAT_REG:
	case P9221_EPT_REG:
	case P9221_VOUT_SET_REG:
	case P9221_ILIM_SET_REG:
	case P9221_OP_MODE_REG:
	case P9221_COM_REG:
	case P9221_FW_SWITCH_KEY_REG:
	case P9221_ALIGN_X_ADC_REG:
	case P9221_ALIGN_Y_ADC_REG:
		return true;
	default:
		return false;
	}
}

static int p9221_read8(struct wpc_charger_info *wpc, int reg, int *val)
{
	return i2c_read_addr16(wpc->i2c_port, P9221_R7_ADDR, reg, val, 1);
}

static int p9221_write8(struct wpc_charger_info *wpc, int reg, int val)
{
	return i2c_write_addr16(wpc->i2c_port, P9221_R7_ADDR, reg, val, 1);
}

static int p9221_read16(struct wpc_charger_info *wpc, int reg, int *val)
{
	return i2c_read_addr16(wpc->i2c_port, P9221_R7_ADDR, reg, val, 2);
}

static int p9221_write16(struct wpc_charger_info *wpc, int reg, int val)
{
	return i2c_write_addr16(wpc->i2c_port, P9221_R7_ADDR, reg, val, 2);
}

static int p9221_block_read(struct wpc_charger_info *wpc, int reg,
			    uint8_t * data, int len)
{
	return i2c_read_addr16_block(wpc->i2c_port, P9221_R7_ADDR, reg, data,
				     len);
}

static int p9221_block_write(struct wpc_charger_info *wpc, int reg,
			     uint8_t * data, int len)
{
	return i2c_write_addr16_block(wpc->i2c_port, P9221_R7_ADDR, reg, data,
				      len);
}

static int p9221_set_cmd_reg(struct wpc_charger_info *wpc, uint8_t cmd)
{
	int cur_cmd = 0;
	int retry;
	int ret;

	for (retry = 0; retry < P9221_COM_CHAN_RETRIES; retry++) {
		ret = p9221_read8(wpc, P9221_COM_REG, &cur_cmd);
		if (ret == 0 && cur_cmd == 0)
			break;
		msleep(25);
	}

	if (retry >= P9221_COM_CHAN_RETRIES) {
		CPRINTS("Failed to wait for cmd free %02x", cur_cmd);
		return -1;
	}

	ret = p9221_write8(wpc, P9221_COM_REG, cmd);
	if (ret)
		CPRINTS("Failed to set cmd reg %02x: %d", cmd, ret);

	return ret;
}

/*
 * Cook the value according to the register (r7+)
 */
static int p9221_cook_reg_r7(uint16_t reg, uint16_t raw_data, uint32_t * val)
{
	/* Do the appropriate conversion */
	switch (reg) {
		/* The following raw values */
	case P9221r7_ALIGN_X_ADC_REG:
	case P9221r7_ALIGN_Y_ADC_REG:
		*val = raw_data;
		break;

		/* The following are 12-bit ADC raw values */
	case P9221r7_VOUT_ADC_REG:
	case P9221r7_IOUT_ADC_REG:
	case P9221r7_DIE_TEMP_ADC_REG:
	case P9221r7_EXT_TEMP_REG:
		*val = raw_data & 0xFFF;
		break;

		/* The following are in 0.1 mill- and need to go to micro- */
	case P9221r7_VOUT_SET_REG:	/* 100mV -> uV */
		raw_data *= 100;
		/* Fall through */

		/* The following are in milli- and need to go to micro- */
	case P9221r7_IOUT_REG:	/* mA -> uA */
	case P9221r7_VRECT_REG:	/* mV -> uV */
	case P9221r7_VOUT_REG:	/* mV -> uV */
		/* Fall through */

		/* The following are in kilo- and need to go to their base */
	case P9221r7_OP_FREQ_REG:	/* kHz -> Hz */
	case P9221r7_TX_PINGFREQ_REG:	/* kHz -> Hz */
		*val = raw_data * 1000;
		break;

	case P9221r7_ILIM_SET_REG:
		/* 100mA -> uA, 200mA offset */
		*val = ((raw_data * 100) + 200) * 1000;
		break;

	case P9221r7_OVSET_REG:
		/* uV */
		raw_data &= P9221r7_OVSET_MASK;
		*val = p9221_ov_set_lut[raw_data];
		break;

	default:
		return -2;
	}

	return 0;
}

static int p9221_reg_read_cooked(struct wpc_charger_info *wpc, uint16_t reg,
				 uint32_t * val)
{
	int ret = 0;
	int data = 0;

	if (p9221_reg_is_8_bit(wpc, reg)) {
		int data8 = 0;
		ret = p9221_read8(wpc, reg, &data8);
		data = data8;
	} else {
		ret = p9221_read16(wpc, reg, &data);
	}
	if (ret)
		return ret;

	return p9221_cook_reg_r7(reg, data, val);
}

static int p9221_is_online(struct wpc_charger_info *wpc)
{
	int ret, chip_id;
	ret = p9221_read16(wpc, P9221_CHIP_ID_REG, &chip_id);
	if (ret == 0 && chip_id == P9221_CHIP_ID)
		return true;
	else
		return false;
}

int wpc_chip_is_online(void){
	struct wpc_charger_info *wpc = wpc_get_charger_info();
	return p9221_is_online(wpc);
}


void p9221_r7_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_WPC);
}

static int p9221_clear_interrupts(struct wpc_charger_info *wpc, uint16_t mask)
{
	int ret;
	uint16_t reg;

	reg = p9221_is_r7(wpc) ? P9221r7_INT_CLEAR_REG : P9221_INT_CLEAR_REG;

	ret = p9221_write16(wpc, reg, mask);
	if (ret) {
		CPRINTS("Failed to clear INT reg: %d", ret);
		goto out;
	}

	ret = p9221_set_cmd_reg(wpc, P9221_COM_CLEAR_INT_MASK);
	if (ret) {
		CPRINTS("Failed to reset INT: %d", ret);
	}

      out:
	return ret;
}

/*
 * Enable interrupts on the P9221, note we don't really need to disable
 * interrupts since when the device goes out of field, the P9221 is reset.
 */
static int p9221_enable_interrupts(struct wpc_charger_info *wpc)
{
	uint16_t mask = 0;
	int ret;

	CPRINTS("Enable interrupts");

	if (p9221_is_r7(wpc))
		mask = P9221r7_STAT_LIMIT_MASK | P9221r7_STAT_CC_MASK;
	else
		mask = P9221_STAT_LIMIT_MASK;

	mask |= P9221_STAT_VRECT;

	ret = p9221_clear_interrupts(wpc, mask);
	if (ret)
		CPRINTS("Could not clear interrupts: %d", ret);

	ret = p9221_write8(wpc, P9221_INT_ENABLE_REG, mask);
	if (ret)
		CPRINTS("Could not enable interrupts: %d", ret);
	return ret;
}

static int p9221_send_eop(struct wpc_charger_info *wpc, uint8_t reason)
{
	int ret;
	uint8_t cmd;

	CPRINTS("Send EOP reason=%d\n", reason);

	if (p9221_is_r7(wpc)) {
		ret = p9221_write8(wpc, P9221r7_EPT_REG, reason);
		cmd = P9221r7_COM_SENDEPT;
	} else {
		ret = p9221_write8(wpc, P9221_EPT_REG, reason);
		cmd = P9221_COM_SEND_EOP_MASK;
	}
	if (ret == 0)
		ret = p9221_set_cmd_reg(wpc, cmd);

	return ret;
}

/*
 * Number of times to poll the status to see if the current limit condition
 * was transient or not.
 */
static void p9221_over_handle_r7(struct wpc_charger_info *wpc,
				 uint16_t orign_irq_src)
{
	uint8_t reason = 0;
	int i;
	int ret;
	int ovc_count = 0;
	uint32_t iout_val[P9221r7_OVER_CHECK_NUM] = { 0 };
	int irq_src = (int)orign_irq_src;

	CPRINTS("Received OVER INT: %02x\n", irq_src);

	if (irq_src & P9221r7_STAT_OVV) {
		reason = P9221_EOP_OVER_VOLT;
		goto send_eop;
	}

	if (irq_src & P9221r7_STAT_OVT) {
		reason = P9221_EOP_OVER_TEMP;
		goto send_eop;
	}

	if ((irq_src & P9221r7_STAT_UV) && !(irq_src & P9221r7_STAT_OVC))
		return;

	/* Overcurrent, reduce ICL and poll to absorb any transients */
#if 0
	if (wpc->dc_icl_votable) {
		int icl;

		icl = get_effective_result_locked(wpc->dc_icl_votable);
		if (icl < 0) {
			CPRINTS("Failed to read ICL (%d)\n", icl);
		} else if (icl > OVC_BACKOFF_LIMIT) {
			icl -= OVC_BACKOFF_AMOUNT;

			ret = vote(wpc->dc_icl_votable,
				   P9221_OCP_VOTER, true, icl);
			CPRINTS("Reduced ICL to %d (%d)\n", icl, ret);
		}
	}
#endif

	reason = P9221_EOP_OVER_CURRENT;
	for (i = 0; i < P9221r7_OVER_CHECK_NUM; i++) {
		ret =
		    p9221_clear_interrupts(wpc,
					   irq_src & P9221r7_STAT_LIMIT_MASK);
		msleep(50);
		if (ret)
			continue;

		ret = p9221_reg_read_cooked(wpc, P9221r7_IOUT_REG,
					    &iout_val[i]);
		if (ret) {
			CPRINTS("Failed to read IOUT[%d]: %d\n", i, ret);
			continue;
		} else if (iout_val[i] > OVC_THRESHOLD) {
			ovc_count++;
		}

		ret = p9221_read16(wpc, P9221_STATUS_REG, &irq_src);
		if (ret) {
			CPRINTS("Failed to read status: %d\n", ret);
			continue;
		}

		if ((irq_src & P9221r7_STAT_OVC) == 0) {
			//print_current_samples(charger, iout_val, i + 1);
			CPRINTS("OVER condition %04x cleared after %d tries\n",
				irq_src, i);
			return;
		}

		CPRINTS("OVER status is still %04x, retry\n", irq_src);
	}

	if (ovc_count < OVC_LIMIT) {
		/*
		   print_current_samples(charger, iout_val,
		   P9221r7_OVER_CHECK_NUM);
		 */
		CPRINTS("ovc_threshold=%d, ovc_count=%d, ovc_limit=%d\n",
			OVC_THRESHOLD, ovc_count, OVC_LIMIT);
		return;
	}

      send_eop:
	CPRINTS("OVER is %04x, sending EOP %d\n", irq_src, reason);

	ret = p9221_send_eop(wpc, reason);
	if (ret)
		CPRINTS("Failed to send EOP %d: %d\n", reason, ret);
}

static void p9221_abort_transfers(struct wpc_charger_info *wpc)
{
	/* Abort all transfers */
#if 0
	cancel_delayed_work(&wpc->tx_work);
#endif
	wpc->tx_busy = false;
	wpc->tx_done = true;
	wpc->rx_done = true;
	wpc->rx_len = 0;
}

/* Handler for r7 and R7 chips */
static void p9221r7_irq_handler(struct wpc_charger_info *wpc, uint16_t irq_src)
{
	int res;

	if (irq_src & P9221r7_STAT_LIMIT_MASK)
		p9221_over_handle_r7(wpc, irq_src);

	/* Receive complete */
	if (irq_src & P9221r7_STAT_CCDATARCVD) {
		int rxlen = 0;
		res = p9221_read8(wpc, P9221r7_COM_CHAN_RECV_SIZE_REG, &rxlen);
		if (res) {
			CPRINTS("Failed to read len: %d", res);
			rxlen = 0;
		}
#if 0
		if (rxlen) {
			res = p9221_block_read(P9221r7_DATA_RECV_BUF_START,
					       wpc->rx_buf, rxlen);
			if (res)
				CPRINTS("Failed to read len: %d", res);

			wpc->rx_len = rxlen;
			wpc->rx_done = true;
			sysfs_notify(&wpc->dev->kobj, NULL, "rxdone");
		}
#endif
	}

	/* Send complete */
#if 0
	if (irq_src & P9221r7_STAT_CCSENDBUSY) {
		wpc->tx_busy = false;
		wpc->tx_done = true;
		cancel_delayed_work(&wpc->tx_work);
		sysfs_notify(&wpc->dev->kobj, NULL, "txbusy");
		sysfs_notify(&wpc->dev->kobj, NULL, "txdone");
	}
#endif

	/* Proprietary packet */
	if (irq_src & P9221r7_STAT_PPRCVD) {

		res = p9221_block_read(wpc, P9221r7_DATA_RECV_BUF_START,
				       wpc->pp_buf, sizeof(wpc->pp_buf));
		if (res)
			CPRINTS("Failed to read PP len: %d", res);

		/* We only care about PP which come with 0x4F header */

		CPRINTS("pp_buf[0]: 0x%x", wpc->pp_buf[0]);
#if 0
		wpc->pp_buf_valid = (wpc->pp_buf[0] == 0x4F);

		p9221_hex_str(wpc->pp_buf, sizeof(wpc->pp_buf),
			      s, maxsz, false);
		CPRINTS("Received PP: %s", s);
#endif
	}

	/* CC Reset complete */
	if (irq_src & P9221r7_STAT_CCRESET)
		p9221_abort_transfers(wpc);
}

static int p9221_is_epp(struct wpc_charger_info *wpc)
{
	int ret;
	uint16_t vout_reg = P9221_VOUT_ADC_REG;
	uint32_t vout_uv;

	if (p9221_is_r7(wpc)) {
		int reg;

		ret = p9221_read8(wpc, P9221r7_SYSTEM_MODE_REG, &reg);

		if (ret == 0)
			return (reg & P9221r7_SYSTEM_MODE_EXTENDED_MASK) > 0;

		CPRINTS("Could not read mode: %d\n", ret);

		vout_reg = P9221r7_VOUT_ADC_REG;
	}

	/* Check based on power supply voltage */

	ret = p9221_reg_read_cooked(wpc, vout_reg, &vout_uv);
	if (ret) {
		CPRINTS("Could read VOUT_ADC, %d\n", ret);
		goto out;
	}

	CPRINTS("Voltage is %duV\n", vout_uv);
	if (vout_uv > P9221_EPP_THRESHOLD_UV)
		return true;

      out:
	/* Default to BPP otherwise */
	return false;
}

static void p9221_write_fod(struct wpc_charger_info *wpc)
{

	int epp;
	uint8_t *fod;
	uint8_t *fod_epp;
	int fod_count = board_get_fod(&fod);
	int fod_epp_count = board_get_epp_fod(&fod_epp);
	uint16_t fod_reg = p9221_is_r7(wpc) ? P9221r7_FOD_REG : P9221_FOD_REG;

	int ret;
	int retries = 3;

	CPRINTS("WPC:04 - 0 w fod start");

	if (!fod_count && !fod_epp_count)
		goto no_fod;

	if (p9221_is_epp(wpc) && fod_epp_count) {
		fod = fod_epp;
		fod_count = fod_epp_count;
		epp = true;
	}

	if (!fod)
		goto no_fod;

	while (retries) {
		//char s[fod_count * 3 + 1];
		uint8_t fod_read[fod_count];

		CPRINTS("Writing %s FOD (n=%d reg=%02x try=%d)",
			epp ? "EPP" : "BPP", fod_count, fod_reg, retries);

		ret = p9221_block_write(wpc, fod_reg, fod, fod_count);
		if (ret) {
			CPRINTS("Could not write FOD: %d", ret);
			return;
		}

		/* Verify the FOD has been written properly */
		ret = p9221_block_read(wpc, fod_reg, fod_read, fod_count);
		if (ret) {
			CPRINTS("Could not read back FOD: %d", ret);
			return;
		}

		if (memcmp(fod, fod_read, fod_count) == 0)
			return;

		//p9221_hex_str(fod_read, fod_count, s, sizeof(s), 0);
		//CPRINTS("FOD verify error, read: %s", s);

		CPRINTS("FOD verify error");

		retries--;
		msleep(100);
	}

      no_fod:
	CPRINTS("FOD not set! bpp:%d epp:%d r:%d",
		fod_count, fod_epp_count, retries);
}

#if 0
static int p9221_get_voltage(struct wpc_charger_info *wpc,int *data)
{
	return p9221_reg_read_cooked(wpc,P9221r7_VOUT_REG,data);
}

static int p9221_get_current(struct wpc_charger_info *wpc,int *data)
{
	return p9221_reg_read_cooked(wpc,P9221r7_IOUT_REG,data);
}


static int p9221_get_ilim(struct wpc_charger_info *wpc, int *data)
{
	return p9221_reg_read_cooked(wpc,P9221r7_ILIM_SET_REG,data);
}

static int p9221_get_voltage_max(struct wpc_charger_info *wpc, int *data )
{
	return p9221_reg_read_cooked(wpc,P9221r7_VOUT_SET_REG,data);
}
#endif

static void p9221_set_online(struct wpc_charger_info *wpc)
{
	int ret;
	int cid;
	CPRINTS("WPC:03 - 0 Set online");

	wpc->online = true;

	wpc->tx_busy = false;
	wpc->tx_done = true;
	wpc->rx_done = false;
	wpc->last_capacity = -1;
	wpc->charge_supplier = CHARGE_SUPPLIER_WPC_BPP;

	ret = p9221_read8(wpc, P9221_CUSTOMER_ID_REG, &cid);

	wpc->cust_id = (uint8_t) cid;
	CPRINTS("P9221 cid: %02x", wpc->cust_id);

	ret = p9221_enable_interrupts(wpc);
	if (ret)
		CPRINTS("WPC:03 - 1 enable int fail: %d", ret);

	/* NOTE: depends on _is_epp() which is not valid until DC_IN */
	p9221_write_fod(wpc);
}

static void p9221_vbus_check_timeout_defer(void)
{
	struct wpc_charger_info *wpc = wpc_get_charger_info();

	CPRINTS("WPC:02 - 2 timeout VUBS, online=%d", wpc->online);
	if (wpc->online)
		p9221_set_offline(wpc);

}

DECLARE_DEFERRED(p9221_vbus_check_timeout_defer);

static void p9221_set_offline(struct wpc_charger_info *wpc)
{
	CPRINTS("WPC:03 - 2 offline");

	wpc->online = false;
	/* Reset PP buf so we can get a new serial number next time around */
	wpc->pp_buf_valid = false;

	p9221_abort_transfers(wpc);

	hook_call_deferred(&p9221_vbus_check_timeout_defer_data, -1);

#if 0
	p9221_vote_defaults(charger);
#endif
}

/* P9221_NOTIFIER_DELAY_MS from VRECTON */
int p9221_notifier_check_det(struct wpc_charger_info *wpc)
{
	if (wpc->online)
		goto done;

	CPRINTS("WPC:02 - 0");
	/* send out a FOD but is_epp() is still invalid */
	p9221_set_online(wpc);

	//power_supply_changed(wpc->wc_psy);

	/* Give the vbus 2 seconds to come up. */
	CPRINTS("WPC:02 - 1 vbus detect ");
	hook_call_deferred(&p9221_vbus_check_timeout_defer_data, -1);
	hook_call_deferred(&p9221_vbus_check_timeout_defer_data,
			   P9221_DCIN_TIMEOUT_MS);

      done:
	wpc->p9221_check_det = false;
	return 0;
}

static int p9221_get_charge_supplier(struct wpc_charger_info *wpc)
{
	int ret = -1;
	uint32_t tx_id = 0;

	if(!wpc->online)
		return ret;

	if (p9221_is_r7(wpc) && p9221_is_epp(wpc)) {
			ret = p9221_read16(wpc, P9221r7_PROP_TX_ID_REG, &tx_id);
			if(ret == 0 && tx_id == P9221_GPP_TX_ID)
				wpc->charge_supplier = CHARGE_SUPPLIER_WPC_GPP;
			else
				wpc->charge_supplier = CHARGE_SUPPLIER_WPC_EPP;
	}

	CPRINTS("WPC:04 ret=%d tx_id=0x%04x chare_supplier=%d",ret,tx_id,wpc->charge_supplier);
	return ret;
}

static int p9221_get_icl(int charge_supplier)
{
	switch (charge_supplier) {
	case CHARGE_SUPPLIER_WPC_EPP:
	case CHARGE_SUPPLIER_WPC_GPP:
		return P9221_DC_ICL_EPP_UA;
	case CHARGE_SUPPLIER_WPC_BPP:
	default:
		return P9221_DC_ICL_BPP_UA;
	}
}

static int p9221_get_ivl(int charge_supplier)
{
	switch (charge_supplier) {
	case CHARGE_SUPPLIER_WPC_EPP:
	case CHARGE_SUPPLIER_WPC_GPP:
		return P9221_DC_IVL_EPP_UV;
	case CHARGE_SUPPLIER_WPC_BPP:
	default:
		return P9221_DC_IVL_BPP_UV;
	}
}

/* 2 * P9221_NOTIFIER_DELAY_MS from VRECTON */
/*
Vbus changes from 0->1:

Scene A:  Tx absent ; USB Charger present
	Status:
		wpc->online = 0
		p9221_is_online() = 0
	Action:
		Do nothing

Scene B:  Tx present; USB Charger absent; WPC powers VBUS in Tx-In timeout
	Status:
		wpc->online = 1
		p9221_is_online() = 1
	Action:
		Write Fod

Scene C:  Tx present; USB Charger absent; WPC powers VBUS after Tx-In timeout
	Status:
		wpc->online = 0
		p9221_is_online() = 1
	Action:
		Write Fod
		p9221_set_online()

Scene D:  Tx present; USB Charger present in TX-In timeout
	Status:
		wpc->online = 1
		p9221_is_online() = 0
	Action:
		P9221_set_offline()

Vbus changes from 1->0

Scene E: USB Charger absent, Remove Tx
	Status:
		wpc->online = 1
		p9221_is_online() = 0
	Action:
		P9221_set_offline()

Scene F: TX absent, Remove USB Charger
	Status:
		wpc->online = 0
		p9221_is_online() = 0
	Action:
		Do nothing

Scene G: Tx present, Plugin USB Charger
	Status:
		Step 1. Cut off Tx power , ==  E
		Step 2. USB Charger powers Vbus after 50ms, == A
	Action:
		P9221_set_offline()

*/
static void p9221_notifier_check_vbus(struct wpc_charger_info *wpc)
{
	int is_online = p9221_is_online(wpc);
	wpc->p9221_check_vbus = false;

	CPRINTS("WPC:02 - 2 online:%d vbus:%d is_online:%d", wpc->online,
		wpc->vbus_status, is_online);
	/*
	 * We now have confirmation from DC_IN, kill the timer, p9221_online
	 * will be set by this function.
	 */

	hook_call_deferred(&p9221_vbus_check_timeout_defer_data, -1);

	/*
	 * Always write FOD, check dc_icl, send CSP
	 */
	if (is_online) {
		CPRINTS("WPC:02 - 3 write fod");
#if 0
		p9221_set_dc_icl(charger);
#endif
		p9221_write_fod(wpc);
#if 0
		if (wpc->last_capacity > 0)
			p9221_send_csp(charger, wpc->last_capacity);
#endif
		if (!wpc->online)
			p9221_set_online(wpc);
	} else {
		if (wpc->online)
			p9221_set_offline(wpc);
	}

	/* WPC Vbus on , update charge voltage and current */
	if(wpc->vbus_status && wpc->online){
		struct charge_port_info chg;

		p9221_get_charge_supplier(wpc);
		chg.voltage = p9221_get_ivl(wpc->charge_supplier)/1000;
		chg.current = p9221_get_icl(wpc->charge_supplier)/1000;
		if(wpc->cb)
			wpc->cb(wpc->charge_supplier,&chg);

	}else{
		if(wpc->cb)
			wpc->cb(wpc->charge_supplier,NULL);
	}

	CPRINTS("check_vbus changed on:%d vbus:%d", wpc->online,
		wpc->vbus_status);

}

static void p9221_detect_defer_work(void)
{

	struct wpc_charger_info *wpc = wpc_get_charger_info();
	CPRINTS("WPC:01 - 0  on:%d check_vbus:%d check_det:%d vbus:%d",
		wpc->online, wpc->p9221_check_vbus, wpc->p9221_check_det,
		wpc->vbus_status);

	/* Step 1 */
	if (wpc->p9221_check_det)
		p9221_notifier_check_det(wpc);

	/* Step 2 */
	if (wpc->p9221_check_vbus)
		p9221_notifier_check_vbus(wpc);

}

DECLARE_DEFERRED(p9221_detect_defer_work);

void notify_p9221_detect_defer_work(int vbus,cb_update_charger cb)
{
	struct wpc_charger_info *wpc = wpc_get_charger_info();
	wpc->cb = cb;
	CPRINTS("WPC:00 - 3 vbus:%d", vbus);
	wpc->p9221_check_vbus = true;
	wpc->vbus_status = vbus;
	hook_call_deferred(&p9221_detect_defer_work_data,
			   P9221_NOTIFIER_DELAY_MS);
}

static int p9221_r7_irq_thread(struct wpc_charger_info *wpc)
{
	int ret, irq_src;
	ret = p9221_read16(wpc, P9221_INT_REG, &irq_src);
	if (ret) {
		goto out;
	}

	CPRINTS("WPC:00 - 0 INT SRC 0x%04x", irq_src);

	ret = p9221_clear_interrupts(wpc, irq_src);
	if (ret) {
		CPRINTS("WPC:00 - 1 failed clear INT %d", ret);
		goto out;
	}

	if (irq_src & P9221_STAT_VRECT) {
		CPRINTS("WPC:00 - 2 VRECTON, online=%d", wpc->online);

		if (!wpc->online) {
			wpc->p9221_check_det = true;
			hook_call_deferred(&p9221_detect_defer_work_data,
					   P9221_NOTIFIER_DELAY_MS);
		}
	}

	p9221r7_irq_handler(wpc, irq_src);

      out:
	return ret;
}

void wireless_power_charger_task(void *u)
{
	struct wpc_charger_info *wpc = wpc_get_charger_info();
	while (1) {
		task_wait_event(-1);

		//p9221_read16(P9221_CHIP_ID_REG,&id);
		//p9221_read8(P9221_CUSTOMER_ID_REG,&cust_id);

		p9221_r7_irq_thread(wpc);

	}
}
