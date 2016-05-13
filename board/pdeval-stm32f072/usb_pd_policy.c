/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | \
			 PDO_FIXED_DATA_SWAP | \
			 PDO_FIXED_EXTERNAL)

/* ADC in 12-bit mode */
#define ADC_SCALE (1 << 12)
/* ADC power supply : VDDA = 3.3V */
#define VDDA_MV   3300
/* VBUS voltage is measured through 20k / 113k voltage divider = /6.65 */
#define VOLT_DIV_H  (20+113)
#define VOLT_DIV_L  (20)
/* convert VBUS voltage in raw ADC value */
#define VBUS_MV(mv) (((mv)*ADC_SCALE*VOLT_DIV_L)/VOLT_DIV_H/VDDA_MV)
/* convert raw ADC value to mV */
#define ADC_TO_VOLT_MV(vbus) ((vbus)*VOLT_DIV*VDDA_MV/ADC_SCALE)

/* Under-voltage limit is 0.8x Vnom */
#define UVP_MV(mv)  VBUS_MV((mv) * 8 / 10)
/* Over-voltage limit is 1.2x Vnom */
#define OVP_MV(mv)  VBUS_MV((mv) * 12 / 10)
/* Over-voltage recovery threshold is 1.1x Vnom */
#define OVP_REC_MV(mv)  VBUS_MV((mv) * 11 / 10)

/* Maximum discharging delay */
#define DISCHARGE_TIMEOUT (190*MSEC) /* MSEC */
/* Voltage overshoot below the OVP threshold for discharging to avoid OVP */
#define DISCHARGE_OVERSHOOT_MV VBUS_MV(200)

/* Used to fake VBUS presence since no GPIO is available to read VBUS */
static int vbus_present;

/* Voltage indexes for the PDOs */
enum volt_idx {
	PDO_IDX_5V  = 0,
	PDO_IDX_15V = 1,

	PDO_IDX_COUNT
};

const uint32_t pd_src_pdo[] = {
		[PDO_IDX_5V]  = PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
		/* Add 2nd src PDO for NXP Thames Lite demo board */
		[PDO_IDX_15V] = PDO_FIXED(15000, 1200, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

/* PDO voltages (should match the table above) */
static const struct {
	int       uvp;    /* under-voltage limit in mV */
	int       ovp;    /* over-voltage limit in mV */
	int       ovp_rec;/* over-voltage recovery threshold in mV */
} voltages[ARRAY_SIZE(pd_src_pdo)] = {
	[PDO_IDX_5V]  = {UVP_MV(5000),  OVP_MV(5000),
						OVP_REC_MV(5000)},
	[PDO_IDX_15V] = {UVP_MV(15000), OVP_MV(15000),
						OVP_REC_MV(15000)},
};

const uint32_t pd_snk_pdo[] = {
		PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

/* current and previous selected PDO entry */
static int volt_idx;
static int last_volt_idx;
static int adc_enable = 0;
/* expiration date of the discharge */
static timestamp_t discharge_deadline;

/* PTN5100 power management functions */
int ptn_en_discharge(int port, int enable);
int ptn_is_discharge_en(int port);
int ptn_wait_vsafe0(int port);
int ptn_wait_vsafe5(int port, int dir);
int ptn_close_usbfet1(int port, int close);
int ptn_close_usbsrc(int port, int close);
int ptn_close_usbfet2(int port, int close);
int ptn_is_vbus_on(int port);

static inline void discharge_enable(int port)
{
	ptn_en_discharge(port, 1);
	gpio_set_level(GPIO_LED_R, 1);
}

static inline void discharge_disable(int port)
{
	STM32_ADC_IER = 0;
	ptn_en_discharge(port, 0);
	adc_disable_watchdog();
	gpio_set_level(GPIO_LED_R, 0);
}

static inline int discharge_is_enabled(int port)
{
	return ptn_is_discharge_en(port);
}

static void discharge_voltage(int port, int target_volt)
{
	discharge_enable(port);
	discharge_deadline.val = get_time().val + DISCHARGE_TIMEOUT;
	/* Monitor VBUS voltage */
	target_volt -= DISCHARGE_OVERSHOOT_MV;
	adc_enable_watchdog(ADC_CH_V_SENSE, 0xFFF, target_volt);
}

int pd_is_valid_input_voltage(int mv)
{
	return 1;
}

void pd_transition_voltage(int idx)
{
	/* No-operation: we are always 5V */
	
	/* Tie port = 0 because this function doesn't support multiport */
	int port = 0;

	/* For PTN5100 Thames-Lite app board only  */
	CPRINTS("Transfer voltage to idx = %d", idx);

	volt_idx = idx - 1;

	/* Support port 0 only */
	if (volt_idx == PDO_IDX_5V) {
		if (last_volt_idx > volt_idx) {
			/* Down transision */

			/*
			 * Do this as quick as possible so we can
			 * avoid RCP as much as we can
			 */
			/* Enable 5V output */
			ptn_close_usbsrc(port, 1);
			msleep(1);
			/* Disable 15V output */
			ptn_close_usbfet2(port, 0);

			if (!adc_enable) {
				/* Enable discharge */
				ptn_en_discharge(port, 1);
				/* Wait for VSafe5v flag turned off */
				ptn_wait_vsafe5(port, 0);
				/* Disable discharge */
				ptn_en_discharge(port, 0);
			}
			else {
				/* Monitor VBUS voltage by on-chip ADC */
				discharge_voltage(port, voltages[volt_idx].ovp);
			}
		}
	}
	else if (volt_idx == PDO_IDX_15V) {
		/* Enable 15V output on thames-lite */
		ptn_close_usbfet2(port, 1);
		/* Disable 5V output */
		ptn_close_usbsrc(port, 0);
	}

	last_volt_idx = volt_idx;
}

int pd_set_power_supply_ready(int port)
{
	last_volt_idx = volt_idx = PDO_IDX_5V;

	/* Stop sinking power from VBUS */
	ptn_close_usbfet1(port, 0);

	/* Turn on the "up" LED when we output VBUS */
	gpio_set_level(GPIO_LED_U, 1);
	CPRINTS("Power supply ready/%d", port);

	/* PTN5100, output VBUS 5V */
	ptn_close_usbsrc(port, 1);

	return EC_SUCCESS; /* we are ready */
}

void pd_power_supply_reset(int port)
{
	if (ptn_is_vbus_on(port)) {
		/* Turn off the "up" LED when we shutdown VBUS */
		gpio_set_level(GPIO_LED_U, 0);

		/* PTN5100, shutdown all VBUS sources immediately. */
		ptn_close_usbfet2(port, 0);
		ptn_close_usbsrc(port, 0);

		/* Enable discharge */
		ptn_en_discharge(port, 1);

		ptn_wait_vsafe0(port);

		/* Disable discharge */
		ptn_en_discharge(port, 0);

		/* Disable VBUS */
		CPRINTS("Disable VBUS", port);

		/* Enable sinking power from VBUS */
		ptn_close_usbfet1(port, 1);
	}
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	CPRINTS("USBPD current limit port %d max %d mA %d mV",
		port, max_ma, supply_voltage);
	/* do some LED coding of the power we can sink */
	if (max_ma) {
		if (supply_voltage > 6500)
			gpio_set_level(GPIO_LED_R, 1);
		else
			gpio_set_level(GPIO_LED_L, 1);
	} else {
		gpio_set_level(GPIO_LED_L, 0);
		gpio_set_level(GPIO_LED_R, 0);
	}
}

void typec_set_input_current_limit(int port, uint32_t max_ma,
				   uint32_t supply_voltage)
{
	CPRINTS("TYPEC current limit port %d max %d mA %d mV",
		port, max_ma, supply_voltage);
	gpio_set_level(GPIO_LED_R, !!max_ma);
}

void button_event(enum gpio_signal signal)
{
	vbus_present = !vbus_present;
	CPRINTS("VBUS %d", vbus_present);
}

static int command_vbus_toggle(int argc, char **argv)
{
	vbus_present = !vbus_present;
	CPRINTS("VBUS %d", vbus_present);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(vbus, command_vbus_toggle,
			"",
			"Toggle VBUS detected",
			NULL);

int pd_snk_is_vbus_provided(int port)
{
	/* Now VBUS status is coming from real hardware */
	return ptn_is_vbus_on(port);
}

int pd_board_checks(void)
{
	/* Tie port = 0 because this function doesn't support multiport */
	int port = 0;

	/* the discharge did not work properly */
	if (discharge_is_enabled(port) &&
		(get_time().val > discharge_deadline.val)) {
		/* stop it */
		CPRINTS("Stop! The discharge did not work properly");
		discharge_disable(port);
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
int pd_check_power_swap(int port)
{
	/*
	 * Allow power swap as long as we are acting as a dual role device,
	 * otherwise assume our role is fixed (not in S0 or console command
	 * to fix our role).
	 */
	return pd_get_dual_role() == PD_DRP_TOGGLE_ON ? 1 : 0;
}
#endif

int pd_check_data_swap(int port, int data_role)
{
	/* Always allow data swap */
	return 1;
}

void pd_execute_data_swap(int port, int data_role)
{
}

void pd_check_pr_role(int port, int pr_role, int flags)
{
}

void pd_check_dr_role(int port, int dr_role, int flags)
{
}

void pd_adc_interrupt(void)
{
	/* Clear flags */
	STM32_ADC_ISR = 0x8e;

	if (discharge_is_enabled(0)) {
		discharge_disable(0);
	}

	/* clear ADC irq so we don't get a second interrupt */
	task_clear_pending_irq(STM32_IRQ_ADC_COMP);
}
DECLARE_IRQ(STM32_IRQ_ADC_COMP, pd_adc_interrupt, 1);

/* ----------------- Vendor Defined Messages ------------------ */
const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int pd_custom_vdm(int port, int cnt, uint32_t *payload,
		  uint32_t **rpayload)
{
	int cmd = PD_VDO_CMD(payload[0]);
	uint16_t dev_id = 0;
	int is_rw;

	/* make sure we have some payload */
	if (cnt == 0)
		return 0;

	switch (cmd) {
	case VDO_CMD_VERSION:
		/* guarantee last byte of payload is null character */
		*(payload + cnt - 1) = 0;
		CPRINTF("version: %s\n", (char *)(payload+1));
		break;
	case VDO_CMD_READ_INFO:
	case VDO_CMD_SEND_INFO:
		/* copy hash */
		if (cnt == 7) {
			dev_id = VDO_INFO_HW_DEV_ID(payload[6]);
			is_rw = VDO_INFO_IS_RW(payload[6]);

			CPRINTF("DevId:%d.%d SW:%d RW:%d\n",
				HW_DEV_ID_MAJ(dev_id),
				HW_DEV_ID_MIN(dev_id),
				VDO_INFO_SW_DBG_VER(payload[6]),
				is_rw);
		} else if (cnt == 6) {
			/* really old devices don't have last byte */
			pd_dev_store_rw_hash(port, dev_id, payload + 1,
					     SYSTEM_IMAGE_UNKNOWN);
		}
		break;
	}

	return 0;
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static int dp_flags[CONFIG_USB_PD_PORT_COUNT];

static void svdm_safe_dp_mode(int port)
{
	/* make DP interface safe until configure */
	dp_flags[port] = 0;
	/* board_set_usb_mux(port, TYPEC_MUX_NONE, pd_get_polarity(port)); */
}

static int svdm_enter_dp_mode(int port, uint32_t mode_caps)
{
	/* Only enter mode if device is DFP_D capable */
	if (mode_caps & MODE_DP_SNK) {
		svdm_safe_dp_mode(port);
		return 0;
	}

	return -1;
}

static int svdm_dp_status(int port, uint32_t *payload)
{
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
	payload[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_STATUS | VDO_OPOS(opos));
	payload[1] = VDO_DP_STATUS(0, /* HPD IRQ  ... not applicable */
				   0, /* HPD level ... not applicable */
				   0, /* exit DP? ... no */
				   0, /* usb mode? ... no */
				   0, /* multi-function ... no */
				   (!!(dp_flags[port] & DP_FLAGS_DP_ON)),
				   0, /* power low? ... no */
				   (!!(dp_flags[port] & DP_FLAGS_DP_ON)));
	return 2;
};

static int svdm_dp_config(int port, uint32_t *payload)
{
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
	/* board_set_usb_mux(port, TYPEC_MUX_DP, pd_get_polarity(port)); */
	payload[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_CONFIG | VDO_OPOS(opos));
	payload[1] = VDO_DP_CFG(MODE_DP_PIN_E, /* pin mode */
				1,             /* DPv1.3 signaling */
				2);            /* UFP connected */
	return 2;
};

static void svdm_dp_post_config(int port)
{
	dp_flags[port] |= DP_FLAGS_DP_ON;
	if (!(dp_flags[port] & DP_FLAGS_HPD_HI_PENDING))
		return;
}

static int svdm_dp_attention(int port, uint32_t *payload)
{
	/* ack */
	return 1;
}

static void svdm_exit_dp_mode(int port)
{
	svdm_safe_dp_mode(port);
	/* gpio_set_level(PORT_TO_HPD(port), 0); */
}

static int svdm_enter_gfu_mode(int port, uint32_t mode_caps)
{
	/* Always enter GFU mode */
	return 0;
}

static void svdm_exit_gfu_mode(int port)
{
}

static int svdm_gfu_status(int port, uint32_t *payload)
{
	/*
	 * This is called after enter mode is successful, send unstructured
	 * VDM to read info.
	 */
	pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_READ_INFO, NULL, 0);
	return 0;
}

static int svdm_gfu_config(int port, uint32_t *payload)
{
	return 0;
}

static int svdm_gfu_attention(int port, uint32_t *payload)
{
	return 0;
}

const struct svdm_amode_fx supported_modes[] = {
	{
		.svid = USB_SID_DISPLAYPORT,
		.enter = &svdm_enter_dp_mode,
		.status = &svdm_dp_status,
		.config = &svdm_dp_config,
		.post_config = &svdm_dp_post_config,
		.attention = &svdm_dp_attention,
		.exit = &svdm_exit_dp_mode,
	},
	{
		.svid = USB_VID_GOOGLE,
		.enter = &svdm_enter_gfu_mode,
		.status = &svdm_gfu_status,
		.config = &svdm_gfu_config,
		.attention = &svdm_gfu_attention,
		.exit = &svdm_exit_gfu_mode,
	}
};
const int supported_modes_cnt = ARRAY_SIZE(supported_modes);
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
