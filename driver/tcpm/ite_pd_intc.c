/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "it83xx_pd.h"
#include "ite_pd_intc.h"
#include "task.h"
#include "tcpm/tcpm.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_USB_PD_TCPM_DRIVER_IT8XXX2
extern uint8_t tx_recovery_count[];
extern enum tcpci_msg_type tx_sop_type[];

static void restore_sop_header_pwr_data_role(enum usbpd_port port,
					     enum tcpci_msg_type type)
{
	if (type != TCPCI_MSG_SOP) {
		if (pd_get_power_role(port) == PD_ROLE_SOURCE)
			/* Bit0: source */
			IT83XX_USBPD_MHSR1(port) |= USBPD_REG_MASK_SOP_PORT_POWER_ROLE;
		else
			/* Bit0: sink */
			IT83XX_USBPD_MHSR1(port) &= ~USBPD_REG_MASK_SOP_PORT_POWER_ROLE;

		if (pd_get_data_role(port) == PD_ROLE_DFP)
			/* Bit5: DFP */
			IT83XX_USBPD_MHSR0(port) |= USBPD_REG_MASK_SOP_PORT_DATA_ROLE;
		else
			/* Bit5: UFP */
			IT83XX_USBPD_MHSR0(port) &= ~USBPD_REG_MASK_SOP_PORT_DATA_ROLE;
	}
}
#endif

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
		/* check TX status, clear by TX_DONE status too */
		if (USBPD_IS_TX_ERR(port)) {
			uint8_t tx_error_status = IT83XX_USBPD_MTCR(port) &
					(USBPD_REG_MASK_TX_NOT_EN_STAT |
					 USBPD_REG_MASK_TX_DISCARD_STAT |
					 USBPD_REG_MASK_TX_NO_RESPONSE_STAT);
			/*
			 * Check Tx error status (TCPC won't set multi tx errors
			 * at one time transmission):
			 * 1) If we doesn't enable Tx.
			 * 2) If discard, means HW doesn't send the msg and resend.
			 * 3) If port partner doesn't respond GoodCRC.
			 */
			if (tx_error_status & USBPD_REG_MASK_TX_NOT_EN_STAT) {
				printk("p%d TxErr: Tx EN and resend\n", port); //?
				IT83XX_USBPD_PDGCR(port) |=
					USBPD_REG_MASK_TX_MESSAGE_ENABLE;
				/* clear TX done interrupt */
				IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
				if (tx_recovery_count[port]--) {
					/* Start Tx */
					USBPD_KICK_TX_START(port);
				} else {
					pd_transmit_complete(port, TCPC_TX_COMPLETE_DISCARDED);
				}
			} else if (tx_error_status &
				   USBPD_REG_MASK_TX_DISCARD_STAT) {
				printk("p%d TxErr: Discard and resend\n", port); //?
				/* clear TX done interrupt */
				IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
				if (tx_recovery_count[port]--) {
					/* Start Tx */
					USBPD_KICK_TX_START(port);
				} else {
					pd_transmit_complete(port, TCPC_TX_COMPLETE_DISCARDED);
				}
			} else if (tx_error_status &
				   USBPD_REG_MASK_TX_NO_RESPONSE_STAT) {
				/* HW had automatically resent nRetry times */
				/*
				 * The power role and data role bits in the
				 * message header are only set for SOP messages.
				 * If an SOP'/SOP'' message fails, restore the
				 * power role and data role bits.
				 */
				restore_sop_header_pwr_data_role(port, tx_sop_type[port]);
				pd_transmit_complete(port, TCPC_TX_COMPLETE_FAILED);
				/* clear TX done interrupt */
				IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
			}
		} else {
			/*
			 * Restored power and data role in the MHSR registers
			 * when SOP'/SOP'' message is successfully transmitted.
			 */
			restore_sop_header_pwr_data_role(port, tx_sop_type[port]);
			pd_transmit_complete(port, TCPC_TX_COMPLETE_SUCCESS);
			/* clear TX done interrupt */
			IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
		}
#else
		/* clear TX done interrupt */
		IT83XX_USBPD_ISR(port) = USBPD_REG_MASK_MSG_TX_DONE;
		task_set_event(PD_PORT_TO_TASK_ID(port),
			       TASK_EVENT_PHY_TX_DONE);
#endif
	}

	if (IS_ENABLED(IT83XX_INTC_PLUG_IN_OUT_SUPPORT)) {
		if (USBPD_IS_PLUG_IN_OUT_DETECT(port)) {
			if (USBPD_IS_PLUG_IN(port))
				/*
				 * When tcpc detect type-c plug in:
				 * 1)If we are sink, disable detect interrupt,
				 * messages on cc line won't trigger interrupt.
				 * 2)If we are source, then set plug out
				 * detection.
				 */
				switch_plug_out_type(port);
			else
				/*
				 * When tcpc detect type-c plug out:
				 * switch to detect plug in.
				 */
				IT83XX_USBPD_TCDCR(port) &=
					~USBPD_REG_PLUG_OUT_SELECT;

			/* clear type-c device plug in/out detect interrupt */
			IT83XX_USBPD_TCDCR(port) |=
				USBPD_REG_PLUG_IN_OUT_DETECT_STAT;
			task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC);
		}
	}
}
