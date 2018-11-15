/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "intc.h"
#include "it83xx_pd.h"
#include "kmsc_chip.h"
#include "registers.h"
#include "task.h"
#include "tcpm.h"
#include "usb_pd.h"
#include "console.h"
//#include "system.h"

#ifdef CONFIG_USB_PD_TCPM_ITE83XX
//static int get_bbram_idx(int port)
//{
//	switch (port) {
//	case 2:
//		return SYSTEM_BBRAM_IDX_PD2;

//	case 1:
//		return SYSTEM_BBRAM_IDX_PD1;

//	case 0:
//		return SYSTEM_BBRAM_IDX_PD0;

//	default:
//		return -1;
//	}
//}

static void chip_pd_irq(enum usbpd_port port)
{
	int i = 0, cc1, cc2;
	//uint8_t saved_flg;
	task_clear_pending_irq(usbpd_ctrl_regs[port].irq);

	/* check status */
	if (USBPD_IS_HARD_RESET_DETECT(port)) {
		/* clear interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_HARD_RESET_DETECT;
		task_set_event(PD_PORT_TO_TASK_ID(port),
			PD_EVENT_TCPC_RESET, 0);
	} else {
		if (USBPD_IS_RX_DONE(port)) {
			tcpm_enqueue_message(port);
			/* clear RX done interrupt */
			IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_RX_DONE;
		}
		if (USBPD_IS_TX_DONE(port)) {
			/* clear TX done interrupt */
			IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
			task_set_event(PD_PORT_TO_TASK_ID(port),
				TASK_EVENT_PHY_TX_DONE, 0);
		}
		if (USBPD_IS_PLUG_IN_OUT_DETECT(port)) {
			/* clear type-c device plug in/out detect interrupt */
			IT83XX_USBPD_TCDCR(port) |=
				USBPD_REG_PLUG_IN_OUT_DETECT_STAT;
			i = ((IT83XX_USBPD_TCDCR(port) &
				USBPD_REG_PLUG_IN_OUT_SELECT) >> 3);
			ccprints("ISR: P%d PLUG_DTCT %d (IN=0 OUT=1)", port, i);

			//i = system_get_bbram(get_bbram_idx(port), &saved_flg);
			//ccprints("P%d BBRAM Role %d", port,
			//	(saved_flg & PD_BBRMFLG_POWER_ROLE) >> 1);
			cc1 = USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) >> 1;
			cc2 = USBPD_GET_CC2_PULL_REGISTER_SELECTION(port) >> 3;
			ccprints("P%d assert cc1 %d cc2 %d (Rp=1 Rd=0)", port,
				cc1, cc2);
			/*
			 * Plug in and out detect toggle, except when we're SNK
			 * to detect plug out which is monitoring Vbus volt by
			 * polling.
			 */
			if ((cc1 == TYPEC_CC_RP) || (cc2 == TYPEC_CC_RP)) {
				IT83XX_USBPD_TCDCR(port) ^=
					USBPD_REG_PLUG_IN_OUT_SELECT;
				i = tcpc_config[port].drv->get_cc(port, &cc1,
					&cc2);
				if ((cc1 == TYPEC_CC_VOLT_RD &&
					cc2 == TYPEC_CC_VOLT_RD) ||
					(cc1 == TYPEC_CC_VOLT_RA &&
					cc2 == TYPEC_CC_VOLT_RA))
					/*
					 * We're SRC to detect audio/debug plug
					 * out.
					 */
					IT83XX_USBPD_TCDCR(port) |=
					  USBPD_REG_PLUG_OUT_DETECT_TYPE_SELECT;
				else
					/* We're SRC to detect SNK plug out */
					IT83XX_USBPD_TCDCR(port) &=
					 ~USBPD_REG_PLUG_OUT_DETECT_TYPE_SELECT;
			}

			//if ((saved_flg & PD_BBRMFLG_POWER_ROLE)!=PD_ROLE_SINK)
			//	IT83XX_USBPD_TCDCR(port) ^=
			//		USBPD_REG_PLUG_IN_OUT_SELECT;
			task_set_event(PD_PORT_TO_TASK_ID(port),
				PD_EVENT_CC, 0);
		}
	}
}
#endif

void intc_cpu_int_group_5(void)
{
	/* Determine interrupt number. */
	int intc_group_5 = intc_get_ec_int();

	switch (intc_group_5) {
#if defined(CONFIG_HOSTCMD_X86) && defined(HAS_TASK_KEYPROTO)
	case IT83XX_IRQ_KBC_OUT:
		lpc_kbc_obe_interrupt();
		break;

	case IT83XX_IRQ_KBC_IN:
		lpc_kbc_ibf_interrupt();
		break;
#endif
	default:
		break;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_5, intc_cpu_int_group_5, 2);

void intc_cpu_int_group_4(void)
{
	/* Determine interrupt number. */
	int intc_group_4 = intc_get_ec_int();

	switch (intc_group_4) {
#ifdef CONFIG_HOSTCMD_X86
	case IT83XX_IRQ_PMC_IN:
		pm1_ibf_interrupt();
		break;

	case IT83XX_IRQ_PMC2_IN:
		pm2_ibf_interrupt();
		break;

	case IT83XX_IRQ_PMC3_IN:
		pm3_ibf_interrupt();
		break;

	case IT83XX_IRQ_PMC4_IN:
		pm4_ibf_interrupt();
		break;

	case IT83XX_IRQ_PMC5_IN:
		pm5_ibf_interrupt();
		break;
#endif
	default:
		break;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_4, intc_cpu_int_group_4, 2);

void intc_cpu_int_group_12(void)
{
	/* Determine interrupt number. */
	int intc_group_12 = intc_get_ec_int();

	switch (intc_group_12) {
#ifdef CONFIG_PECI
	case IT83XX_IRQ_PECI:
		peci_interrupt();
		break;
#endif
#ifdef CONFIG_HOSTCMD_ESPI
	case IT83XX_IRQ_ESPI:
		espi_interrupt();
		break;

	case IT83XX_IRQ_ESPI_VW:
		espi_vw_interrupt();
		break;
#endif
#ifdef CONFIG_USB_PD_TCPM_ITE83XX
	case IT83XX_IRQ_USBPD0:
		chip_pd_irq(USBPD_PORT_A);
		break;

	case IT83XX_IRQ_USBPD1:
		chip_pd_irq(USBPD_PORT_B);
		break;
#endif /* CONFIG_USB_PD_TCPM_ITE83XX */
	default:
		break;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_12, intc_cpu_int_group_12, 2);

void intc_cpu_int_group_7(void)
{
	/* Determine interrupt number. */
	int intc_group_7 = intc_get_ec_int();

	switch (intc_group_7) {
#ifdef CONFIG_ADC
	case IT83XX_IRQ_ADC:
		adc_interrupt();
		break;
#endif
	default:
		break;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_7, intc_cpu_int_group_7, 2);

void intc_cpu_int_group_6(void)
{
	/* Determine interrupt number. */
	int intc_group_6 = intc_get_ec_int();

	switch (intc_group_6) {
#ifdef CONFIG_I2C
	case IT83XX_IRQ_SMB_A:
		i2c_interrupt(IT83XX_I2C_CH_A);
		break;

	case IT83XX_IRQ_SMB_B:
		i2c_interrupt(IT83XX_I2C_CH_B);
		break;

	case IT83XX_IRQ_SMB_C:
		i2c_interrupt(IT83XX_I2C_CH_C);
		break;

	case IT83XX_IRQ_SMB_D:
		i2c_interrupt(IT83XX_I2C_CH_D);
		break;

	case IT83XX_IRQ_SMB_E:
		i2c_interrupt(IT83XX_I2C_CH_E);
		break;

	case IT83XX_IRQ_SMB_F:
		i2c_interrupt(IT83XX_I2C_CH_F);
		break;
#endif
	default:
		break;
	}
}
DECLARE_IRQ(CPU_INT_GROUP_6, intc_cpu_int_group_6, 2);
