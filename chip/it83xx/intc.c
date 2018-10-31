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

#ifdef CONFIG_USB_PD_TCPM_ITE83XX
/* Store each port last message id of received packet */
int message_id_last[USBPD_PORT_COUNT];

/* Init last message id variable of received packet */
void init_message_id_last_var(int port)
{
	/* Init invalid value (message id = 0 ~ 7) */
	message_id_last[port] = 0xff;
}

int check_message_id_repeat(int port)
{
	int msg_header = IT83XX_USBPD_RMH(port);
	int msg_type = PD_HEADER_TYPE(msg_header);
	int msg_id = PD_HEADER_ID(msg_header);
	int msg_cnt = PD_HEADER_CNT(msg_header);
	char fsoft_acpt = (msg_id == 0 && (msg_type == 0x0D/*Softreset*/ ||
			   msg_type == 0x03/*Accept*/) && msg_cnt == 0) ? 1 : 0;

	/*
	 * Check if repeat message id, if yes don't respond subsequent
	 * messages, expect softreset.
	 */
	if (message_id_last[port] == msg_id && !fsoft_acpt) {
		/* If clear this bit, USBPD receives next packet */
		IT83XX_USBPD_MRSR(port) = USBPD_REG_MASK_RX_MSG_VALID;
		ccprints("message id repeat: message_id_last[%d]= %d", port,
			  message_id_last[port]);
		/* message id repeat */
		return 1;
	}

	/*
	 * Message id not repeat, but softreset subsequent messages should
	 * init last message id variable.
	 */
	if (fsoft_acpt)
		init_message_id_last_var(port);
	else
		message_id_last[port] = msg_id;
	/* message id not repeat */
	return 0;
}

static void chip_pd_irq(enum usbpd_port port)
{
	task_clear_pending_irq(usbpd_ctrl_regs[port].irq);

	/* check status */
	if (USBPD_IS_HARD_RESET_DETECT(port)) {
		/* clear interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_HARD_RESET_DETECT;
		/* Init last message id variable of received packet */
		init_message_id_last_var(port);
		task_set_event(PD_PORT_TO_TASK_ID(port),
			PD_EVENT_TCPC_RESET, 0);
	} else {
		if (USBPD_IS_RX_DONE(port)) {
			if (!(check_message_id_repeat(port)))
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
