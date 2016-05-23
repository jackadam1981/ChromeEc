/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "common.h"
#include "debug_printf.h"
#include "ec_commands.h"
#include "registers.h"
#include "util.h"
#include "usb_pd.h"
#include "gpio.h"

/* ------------------------- Power supply control ------------------------ */

/* GPIO level setting helpers through BSRR register */
#define GPIO_SET(n)   (1 << (n))
#define GPIO_RESET(n) (1 << ((n) + 16))

extern int board_enable_vlan(int enable);

static inline void output_enable(void)
{
	/* GPF0 (enable OR'ing FETs) = 1 */
	STM32_GPIO_BSRR(GPIO_F) = GPIO_SET(0);
}

static inline void output_disable(void)
{
	/* GPF0 (disable OR'ing FETs) = 0 */
	STM32_GPIO_BSRR(GPIO_F) = GPIO_RESET(0);
}

/* ----------------------- USB Power delivery policy ---------------------- */

#define PDO_FIXED_FLAGS (PDO_FIXED_EXTERNAL | PDO_FIXED_DATA_SWAP)

#define RATED_CURRENT 3000

/* Voltage indexes for the PDOs */
enum volt_idx {
  PDO_IDX_5V  = 0,

  PDO_IDX_COUNT
};

/* Power Delivery Objects */
const uint32_t pd_src_pdo[] = {
	[PDO_IDX_5V]  = PDO_FIXED(5000,  RATED_CURRENT, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);
BUILD_ASSERT(ARRAY_SIZE(pd_src_pdo) == PDO_IDX_COUNT);

/* Fake PDOs : we just want our pre-defined voltages */
const uint32_t pd_snk_pdo[] = {
  PDO_FIXED(5000,   500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

void pd_set_input_current_limit(int port, uint32_t max_ma,
    uint32_t supply_voltage)
{
  /* No battery, nothing to do */
  return;
}

int pd_is_valid_input_voltage(int mv)
{
  /* Any voltage less than the max is allowed */
  return 1;
}

int pd_board_check_request(uint32_t rdo)
{
	int idx = RDO_POS(rdo);

	/* Invalid index */
	if (!idx || idx > pd_src_pdo_cnt)
		return EC_ERROR_INVAL;

	return EC_SUCCESS;
}

void pd_transition_voltage(int idx)
{
}

int pd_set_power_supply_ready(int port)
{
	output_enable();

	return EC_SUCCESS; /* we are ready */
}

void pd_power_supply_reset(int port)
{
	output_disable();
}

int pd_board_checks(void)
{
  return EC_SUCCESS;
}

int pd_snk_is_vbus_provided(int port)
{
  return 0;
}

int pd_check_power_swap(int port)
{
  /* We are source only */
  return 0;
}

int pd_check_data_swap(int port, int data_role)
{
	/* Allow data swap if we are a DFP, otherwise don't allow */
	return (data_role == PD_ROLE_DFP) ? 1 : 0;
}

void pd_execute_data_swap(int port, int data_role)
{
	/* Do nothing */
}

void pd_check_pr_role(int port, int pr_role, int flags)
{
}

void pd_check_dr_role(int port, int dr_role, int flags)
{
	/* If DFP, try to switch to UFP */
	if ((flags & PD_FLAGS_PARTNER_DR_DATA) && dr_role == PD_ROLE_DFP)
		pd_request_data_swap(port);
}

/* ----------------- Vendor Defined Messages ------------------ */
const uint32_t vdo_idh = VDO_IDH(0, /* data caps as USB host */
				 0, /* data caps as USB device */
				 IDH_PTYPE_UNDEF, /* Undefined */
				 1, /* supports alt modes */
				 USB_VID_GOOGLE);

const uint32_t vdo_product = VDO_PRODUCT(CONFIG_USB_PID, CONFIG_USB_BCD_DEV);

/* When set true, we are in GFU mode */
static int gfu_mode;

static int svdm_response_identity(int port, uint32_t *payload)
{
	payload[VDO_I(IDH)] = vdo_idh;
	payload[VDO_I(CSTAT)] = VDO_CSTAT(0);
	payload[VDO_I(PRODUCT)] = vdo_product;
	return VDO_I(PRODUCT) + 1;
}

static int svdm_response_svids(int port, uint32_t *payload)
{
	payload[1] = VDO_SVID(USB_VID_GOOGLE, 0);
	return 2;
}

/* Will only ever be a single mode for this device */
#define MODE_CNT 1
#define OPOS 1

const uint32_t vdo_dp_mode[MODE_CNT] =  {
	VDO_MODE_GOOGLE(MODE_GOOGLE_FU)
};

static int svdm_response_modes(int port, uint32_t *payload)
{
	if (PD_VDO_VID(payload[0]) != USB_VID_GOOGLE)
		return 0; /* nak */

	memcpy(payload + 1, vdo_dp_mode, sizeof(vdo_dp_mode));
	return MODE_CNT + 1;
}

static int svdm_enter_mode(int port, uint32_t *payload)
{
	/* SID & mode request is valid */
	if ((PD_VDO_VID(payload[0]) != USB_VID_GOOGLE) ||
	    (PD_VDO_OPOS(payload[0]) != OPOS))
		return 0; /* will generate NAK */

	gfu_mode = 1;
	debug_printf("GFU\n");
	return 1;
}

static int svdm_exit_mode(int port, uint32_t *payload)
{
	gfu_mode = 0;
	return 1; /* Must return ACK */
}

int pd_alt_mode(int port, uint16_t svid)
{
	if (svid == USB_VID_GOOGLE)
		return 1;
	return 0;
}

static struct amode_fx dp_fx = {
	.status = NULL,
	.config = NULL,
};

const struct svdm_response svdm_rsp = {
	.identity = &svdm_response_identity,
	.svids = &svdm_response_svids,
	.modes = &svdm_response_modes,
	.enter_mode = &svdm_enter_mode,
	.amode = &dp_fx,
	.exit_mode = &svdm_exit_mode,
};

int pd_custom_vdm(int port, int cnt, uint32_t *payload,
		  uint32_t **rpayload)
{
	int cmd = PD_VDO_CMD(payload[0]);
	int rsize;

	if (PD_VDO_VID(payload[0]) != USB_VID_GOOGLE || !gfu_mode)
		return 0;

	debug_printf("%T] VDM/%d [%d] %08x\n", cnt, cmd, payload[0]);
	*rpayload = payload;

	rsize = pd_custom_flash_vdm(port, cnt, payload);
	if (!rsize) {
		switch (cmd) {
		case VDO_CMD_PING_ENABLE:
			pd_ping_enable(0, payload[1]);
			rsize = 1;
			break;
    case VDO_CMD_GET_LOG:
      rsize = pd_vdm_get_log_entry(payload);
      break;
		case VDO_CMD_VLAN_ENABLE:
			rsize = board_enable_vlan(payload[1]);
			break;
		default:
			/* Unknown : do not answer */
			return 0;
		}
	}

	/* respond (positively) to the request */
	payload[0] |= VDO_SRC_RESPONDER;

	return rsize;
}
