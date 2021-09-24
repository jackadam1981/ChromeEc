/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hwtimer_chip.h"
#include "it83xx_pd.h"
#include "ite_pd_intc.h"
#include "task.h"
#include "tcpm/tcpm.h"
#include "usb_pd.h"

void chip_pd_irq(enum usbpd_port port)
{
	task_clear_pending_irq(usbpd_ctrl_regs[port].irq);

	/* check status */
	if (IS_ENABLED(IT83XX_INTC_FAST_SWAP_SUPPORT) &&
		IS_ENABLED(CONFIG_USB_PD_FRS_TCPC) &&
		IS_ENABLED(CONFIG_USB_PD_REV30)) {
		/*
		 * FRS detection must handle first, because we need to short
		 * the interrupt -> board_frs_handler latency-critical time.
		 */
		if (USBPD_IS_FAST_SWAP_DETECT(port)) {
			/* clear detect FRS signal (cc to GND) status */
			USBPD_CLEAR_FRS_DETECT_STATUS(port);
			if (board_frs_handler)
				board_frs_handler(port);
			/* inform TCPMv2 to change state */
			pd_got_frs_signal(port);
		}
	}

	if (USBPD_IS_HARD_RESET_DETECT(port)) {
		/* clear interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_HARD_RESET_DETECT;
		USBPD_SW_RESET(port);
		task_set_event(PD_PORT_TO_TASK_ID(port),
			       PD_EVENT_RX_HARD_RESET);
	}

	if (USBPD_IS_RX_DONE(port)) {
		tcpm_enqueue_message(port);
		/* clear RX done interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_RX_DONE;
	}

	if (USBPD_IS_TX_DONE(port)) {
#ifdef CONFIG_USB_PD_TCPM_DRIVER_IT8XXX2
		it8xxx2_clear_tx_error_status(port);
		/* check TX status, clear by TX_DONE status too */
		if (USBPD_IS_TX_ERR(port))
			it8xxx2_get_tx_error_status(port);
#endif
		/* clear TX done interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
		task_set_event(PD_PORT_TO_TASK_ID(port),
			       TASK_EVENT_PHY_TX_DONE);
	}

	if (IS_ENABLED(IT83XX_INTC_PLUG_IN_OUT_SUPPORT)) {
		if (USBPD_IS_PLUG_IN_OUT_DETECT(port)) {
			if (USBPD_IS_PLUG_IN(port)) {
				/*
				 * When tcpc detect type-c plug in:
				 * 1)If we are sink, disable detect interrupt,
				 * messages on cc line won't trigger interrupt.
				 * 2)If we are source, then set plug out
				 * detection.
				 */
				switch_plug_out_type(port);

				/* Stop auto toggle */
				if (port == USBPD_PORT_A)
					/* Disable timer1 interrupt */
					task_disable_irq(IT83XX_IRQ_EXT_TIMER1);
				else if (port == USBPD_PORT_B)
					/* Disable timer2 interrupt */
					task_disable_irq(IT83XX_IRQ_EXT_TIMER2);
			} else {
				/*
				 * When tcpc detect type-c plug out:
				 * switch to detect plug in.
				 */
				IT83XX_USBPD_TCDCR(port) &=
					~USBPD_REG_PLUG_OUT_SELECT;
			}

			/* clear type-c device plug in/out detect interrupt */
			IT83XX_USBPD_TCDCR(port) |=
				USBPD_REG_PLUG_IN_OUT_DETECT_STAT;
			task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC);
		}
	}
}

#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
void auto_toggle_timer_interrupt(enum usbpd_port port)
{
	uint32_t hw_cnt;

	if (port == USBPD_PORT_A) {
		/* Disable timer1 interrupt */
		task_disable_irq(IT83XX_IRQ_EXT_TIMER1);

		/* Get current power role */
		if (USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) ==  USBPD_POWER_ROLE_SRC) {
			/* CCs assert Rd */
			tcpm_set_cc(port, TYPEC_CC_RD);
			hw_cnt = MS_TO_COUNT(1024, PD_T_DRP_SNK);
		} else {
			/* CCs assert Rp */
			tcpm_set_cc(port, TYPEC_CC_RP);
			hw_cnt = MS_TO_COUNT(1024, PD_T_DRP_SRC);
		}

		/*
		 * Set timer1 count.
		 * After write ET1CNTLLR, timer1 will start.
		 */
		IT83XX_ETWD_ET1CNTLHR = (uint8_t)((hw_cnt >> 8) & 0xff);
		IT83XX_ETWD_ET1CNTLLR = (uint8_t)(hw_cnt & 0xff);

		/* Clear timer1 interrupt status */
		task_clear_pending_irq(IT83XX_IRQ_EXT_TIMER1);

		/* Enable timer1 interrupt */
		task_enable_irq(IT83XX_IRQ_EXT_TIMER1);
	} else if (port == USBPD_PORT_B) {
		/* Disable timer2 interrupt */
		task_disable_irq(IT83XX_IRQ_EXT_TIMER2);

		/* Get current power role */
		if (USBPD_GET_CC1_PULL_REGISTER_SELECTION(port) ==  USBPD_POWER_ROLE_SRC) {
			/* CCs assert Rd */
			tcpm_set_cc(port, TYPEC_CC_RD);
			hw_cnt = MS_TO_COUNT(32768, PD_T_DRP_SNK);
		} else {
			/* CCs assert Rp */
			tcpm_set_cc(port, TYPEC_CC_RP);
			hw_cnt = MS_TO_COUNT(32768, PD_T_DRP_SRC);
		}

		/*
		 * Set timer2 count.
		 * After write ET2CNTLLR, timer2 will start.
		 */
		IT83XX_ETWD_ET2CNTLH2R = (uint8_t)((hw_cnt >> 16) & 0xff);
		IT83XX_ETWD_ET2CNTLHR = (uint8_t)((hw_cnt >> 8) & 0xff);
		IT83XX_ETWD_ET2CNTLLR = (uint8_t)(hw_cnt & 0xff);


		/* Clear timer2 interrupt status */
		task_clear_pending_irq(IT83XX_IRQ_EXT_TIMER2);

		/* Enable timer2 interrupt */
		task_enable_irq(IT83XX_IRQ_EXT_TIMER2);
	}
}
#endif /* CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE */
