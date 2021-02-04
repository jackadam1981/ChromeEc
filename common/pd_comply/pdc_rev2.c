/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bring-up Procedures */
static bool proc_pd2_e1(void);
/* Bring-up Procedures for DisplayPort */
static bool proc_pd2_e2(void);

struct pdc_tests rev2_tests[] = {
	{ proc_pd2_e1, "proc_pd2_e1", "Bring-up Procedure for UUT and DFP" },
	{ proc_pd2_e2, "proc_pd2_e2", "Bring-up Procedure for DisplayPort" },

	{ NULL, NULL, NULL }
};

/*
 * For UFP UUT:
 * a) The test starts in a disconnected state.
 * b) The tester applies Rp (3A) and waits for the UUT attachment.
 * c) If Ra is detected, the tester applies Vconn.
 * d) The tester applies Vbus and waits 50 ms.
 * e) The tester transmits Source Capabilities until reception of GoodCrc for
 *    tNoResponse max (5.5s). The Source Capabilities includes Fixed 5V 3A PDO.
 * f) The tester waits for the Request from the UUT for tSenderResponse
 *    max (30 ms).
 * g) The tester sends Accept, and when Vbus is stable at the target voltage,
 *    sends PS_RDY
 */
static int proc_pd2_e1_ufp(void)
{
	int ret;

	switch (pdc.step) {
	case a:
		/* Start out disconnected */
		pdc_uut_disconnect();
		pdc_next_step();
		break;
	case b:
		/*
		 * The tester applies Rp (3A) and waits for the
		 * UUT attachment.
		 */
		pdc.current = PDC_3A0;
		if (tcpm_select_rp_value(pdc.port, TYPEC_RP_3A0))
			return PDC_FAIL;

		if (tcpm_set_cc(pdc.port, TYPEC_CC_RP))
			return PDC_FAIL;

		if (pdc_next_step())
			return PDC_DONE;
		break;
	case c:
		switch (pdc.state) {
		case 0:
			ccprintf("Please plug in UUT\n");
			pdc.state++;
			return PDC_CONTINUE;
		case 1:
			/* Wait until UUT is detected */
			if (pd_get_cc_state(pdc.cc1, pdc.cc2) !=
							PD_CC_UFP_ATTACHED)
				return PDC_CONTINUE;

			ccprintf("UUT Detected\n");
			break;
		}

		/* Set role and polarity */
		pdc_update_role_and_polarity();
		/* Enable VCONN */
		tcpm_set_vconn(pdc.port, 1);
		/* Enable receiving of messages */
		tcpm_set_rx_enable(pdc.port, 1);

		if (pdc_next_step())
			return PDC_DONE;

		/* fall through */
	case d:
		/* Enable VBUS */
		pdc_src_power_on();
		/* Init msg id */
		pd[pdc.port].msg_id = 0;

		pdc_set_evt_timeout(10 * MSEC);
		/* The Tester applies Vbus and waits 50 ms */
		pdc.timer = get_time().val + (50 * MSEC);

		if (pdc_next_step())
			return PDC_DONE;

		/* fall through */
	case e:
		switch (pdc.state) {
		case 0:
			/* Waiting 50 ms */
			if (pdc.timer > get_time().val)
				return PDC_CONTINUE;

			pdc.state++;
			pdc_set_evt_timeout(50 * MSEC);
			/* tNoResponse max 5.5 s */
			pdc.timer = get_time().val + (5500 * MSEC);
			/* fall through */
		case 1:
			/*
			 * The tester transmits Source Capabilities until
			 * reception of GoodCrc for tNoResponse max (5.5s).
			 * The Source Capabilities includes Fixed 5V 3A PDO.
			 */
			ret = pdc_send_data_msg(
				PD_DATA_SOURCE_CAP,
				0, /* Not a VDM CMD */
				0,
				0,
				TCPC_TX_SOP,
				(100 * MSEC)); /* wait 100mS for a response */

			/* Check no response timer */
			if (get_time().val > pdc.timer) {
				pdc_report_failure(PDC_FAILURE_NO_RESPONSE,
								pdc.step);
				return PDC_FAIL;
			}

			switch (ret) {
			case PDC_CONTINUE:
				return ret;
			case PDC_FAIL:
				return ret;
			case PDC_NO_RESPONSE:
				/* Resend the message and continue the test. */
				reset_msg_state();
				return PDC_CONTINUE;
			case PDC_DONE:
				if (pdc_next_step())
					return PDC_DONE;
			}
			/* fall through */
		}
		/* fall through */
	case f:
		/* Make sure we received a request */
		if (PD_HEADER_TYPE(pdc.header) != PD_DATA_REQUEST)
			return PDC_FAIL;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case g:
		/*
		 * The tester sends Accept, and when Vbus is stable at the
		 * target voltage, sends PS_RDY.
		 */
		pdc_send_control_msg(PD_CTRL_ACCEPT, TCPC_TX_SOP);
		pdc_send_control_msg(PD_CTRL_PS_RDY, TCPC_TX_SOP);

		/* We are connected to a UUT UFP */
		pdc.flags |= PDC_FLAGS_CONNECTED;
		/* we are done */
		return PDC_DONE;
	}

	return PDC_CONTINUE;
}

/*
 * For DFP UUT:
 * a) The test starts in a disconnected state.
 * b) The tester applies Rd and waits for Vbus for tNoResponse max (5.5 s).
 * c) The tester waits Source Capabilities for for tNoResponse max (5.5 s).
 * d) The tester replies GoodCrc on reception of the Source Capabilities.
 * e) The tester requests 5V 0.5A.
 * f) The tester waits PS_RDY for tPSSourceOn max (480 ms).
 */
static int proc_pd2_e1_dfp(void)
{
	enum tcpc_cc_pull pull;
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	switch (pdc.step) {
	case a:
		/* The test starts in a disconnected state.*/
		pdc_uut_disconnect();
		pdc_next_step();
		break;
	case b:
		/*
		 * The tester applies Rd and waits for
		 * Vbus for tNoResponse max (5.5 s).
		 */
		switch (pdc.state) {
		case 0:
			if (pdc.flags & PDC_FLAGS_RA_RD)
				pull = TYPEC_CC_RA_RD;
			else
				pull = TYPEC_CC_RD;

			if (tcpm_set_cc(pdc.port, pull))
				return PDC_FAIL;

			pdc_set_evt_timeout(10 * MSEC);
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;

			ccprintf("Please plug in UUT\n");
			return PDC_CONTINUE;
		case 1:
			if (pd_is_vbus_present(pdc.port)) {
				/* Set role and polarity */
				pdc_update_role_and_polarity();
				/* Enable reception of messages */
				tcpm_set_rx_enable(pdc.port, 1);
				/* Goto next step */
				if (pdc_next_step())
					return PDC_DONE;
			} else if (get_time().val > pdc.timer) {
				/* UUT not detected in time */
				return PDC_FAIL;
			} else {
				return PDC_CONTINUE;
			}
		}
		/* fall through */
	case c:
		/*
		 * The tester waits Source Capabilities for
		 * tNoResponse max (5.5 s).
		 */
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
							pdc.payload,
							&pdc.header)
							!= EC_SUCCESS)
					/* dequeue error */
					return PDC_FAIL;

				/* Get header contents */
				type = PD_HEADER_TYPE(pdc.header);
				cnt = PD_HEADER_CNT(pdc.header);
				ext = PD_HEADER_EXT(pdc.header);
				/* Did we receive a source cap message */
				if (type != PD_DATA_SOURCE_CAP ||
					cnt == 0 ||
					ext != 0)
					return PDC_FAIL;

				/* Goto next step */
				if (pdc_next_step())
					return PDC_DONE;
			} else if (get_time().val > pdc.timer) {
				/* We didn't receive a source cap */
				return PDC_FAIL;
			} else {
				return PDC_CONTINUE;
			}
		}
		/* fall through */
	case d:
		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case e:
		/* The tester requests 5V 0.5A. */
		send_request(pdc.port, RDO_FIXED(1, 500, 500, 0));
		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case f:
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (480 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
							 pdc.payload,
							 &pdc.header)
							 != EC_SUCCESS) {
					/* dequeue error */
					return PDC_FAIL;
				}
				/* Get header contents */
				type = PD_HEADER_TYPE(pdc.header);
				cnt = PD_HEADER_CNT(pdc.header);
				ext = PD_HEADER_EXT(pdc.header);
				/* Did we receive an accept message */
				if (type != PD_CTRL_ACCEPT ||
					cnt > 0 ||
					ext != 0)
					return PDC_FAIL;
			} else if (get_time().val > pdc.timer) {
				/* We didn't receive an accept message */
				return PDC_FAIL;
			} else {
				return PDC_CONTINUE;
			}
		}
		pdc.timer = get_time().val + (480 * MSEC);
		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case g:
		if (tcpm_has_pending_message(pdc.port)) {
			/* Dequeue and consume duplicate message ID. */
			if (tcpm_dequeue_message(pdc.port,
						pdc.payload,
						&pdc.header)
							!= EC_SUCCESS) {
				/* dequeue error */
				return PDC_FAIL;
			}
			/* Get header contents */
			type = PD_HEADER_TYPE(pdc.header);
			cnt = PD_HEADER_CNT(pdc.header);
			ext = PD_HEADER_EXT(pdc.header);
			/* Did we receive a ps ready */
			if (type != PD_CTRL_PS_RDY ||
				cnt > 0 ||
				ext != 0)
				return PDC_FAIL;

		} else if (get_time().val > pdc.timer) {
			/* We didn't receive a ps ready message */
			return PDC_FAIL;
		} else {
			return PDC_CONTINUE;
		}

		/* We are connected to a UUT DFP */
		pdc.flags |= PDC_FLAGS_CONNECTED;
		return PDC_DONE;
	}

	return PDC_CONTINUE;
}

static bool proc_pd2_e1(void)
{
	int ret = 0;

	switch (pdc_get_uut_role()) {
	case PD_UFP:
		ret = proc_pd2_e1_ufp();
		break;
	case PD_DFP:
		ret = proc_pd2_e1_dfp();
		break;
	}

	if (ret < 0) {
		pdc_report_failure(PDC_FAILURE_BRINGUP, pdc.step);
		ccprintf("FAILED at step %d\n", pdc.step);
		return false;
	} else if (ret > 0) {
		/* Continue */
		return true;
	}

	ccprintf("DONE\n");
	return false;
}

static int proc_pd2_e2_ufp(void)
{
	int ret;

	/* a) The Tester establishes an explicit contract with the UUT */
	if (!(pdc.flags & PDC_FLAGS_CONNECTED)) {
		ret = proc_pd2_e1_ufp();
		if (ret != PDC_DONE)
			return ret;
		/* We are connected to a UUT UFP */
		pdc.step = b;
	}

	switch (pdc.step) {
	case b:
		/* The Tester sends Discover Identity */
		ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
					CMD_DISCOVER_IDENT,
					USB_SID_PD,
					0,
					TCPC_TX_SOP,
					100 * MSEC);
		if (ret != PDC_DONE)
			return ret;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case c:
		/* The Tester verifies: */
		/*
		 * The UUT responds with Discover Identity ACK within
		 * tVDMReceivreResponse(15ms).
		 */
		if (pdc.delta_time > (15 * MSEC))
			return PDC_FAIL;

		if (PD_VDO_CMDT(pdc.payload[0]) != CMDT_RSP_ACK)
			return PDC_FAIL;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case d:
		/* The Tester sends Discover SVIDs. */
		ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
					CMD_DISCOVER_SVID,
					USB_SID_PD,
					0,
					TCPC_TX_SOP,
					100 * MSEC);
		if (ret != PDC_DONE)
			return ret;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case e:
		/* The Tester verifies: */
		/*
		 * The UUT responds with Discover SVIDs ACK within
		 * tVDMReceivreResponse(15ms).
		 */
		if (pdc.delta_time > (15 * MSEC))
			return PDC_FAIL;

		if (PD_VDO_CMDT(pdc.payload[0]) != CMDT_RSP_ACK)
			return PDC_FAIL;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case f:
		/*
		 * If the UUT doesn't return DP SID,
		 * the test is not applicable
		 */
		if (pdc.payload[1] != 0xff010000)
			return PDC_DONE;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case g:
		/* The Tester sends Discover Modes using the DP SID (0xFF01). */
		ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
					CMD_DISCOVER_MODES,
					USB_SID_DISPLAYPORT,
					0,
					TCPC_TX_SOP,
					100 * MSEC);
		if (ret != PDC_DONE)
			return ret;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case h:
		/* The Tester verifies: */
		/*
		 * The UUT responds with Discover Modes ACK within
		 * tVDMReceiverResponse (15 ms).
		 */
		if (pdc.delta_time > (15 * MSEC))
			return PDC_FAIL;

		/*
		 * The UUT responds with SVIDs = DP SID (0xFF01)
		 * in the VDM Header
		 */
		if (PD_VDO_VID(pdc.payload[0]) != USB_SID_DISPLAYPORT)
			return PDC_FAIL;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case i:
		/*
		 * The Tester sends an Enter Mode command using
		 * DP SID (0xFF01).
		 */
		ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
					CMD_ENTER_MODE,
					USB_SID_DISPLAYPORT,
					0,
					TCPC_TX_SOP,
					100 * MSEC);
		if (ret != PDC_DONE)
			return ret;

		if (pdc_next_step())
			return PDC_DONE;
		/* fall through */
	case j:
		/* The Tester verifies: */
		/* The UUT responds with NAK or ACK */
		/* If the UUT responds with BUSY, the test fails. */
		if (PD_VDO_CMDT(pdc.payload[0]) == CMDT_RSP_BUSY)
			return PDC_FAIL;

		/*
		 * If the UUT responds with ACK, this is received
		 * within tVDMEnterMode (25 ms).
		 */
		if (PD_VDO_CMDT(pdc.payload[0]) == CMDT_RSP_ACK &&
				pdc.delta_time > (25 * MSEC))
			return PDC_FAIL;

		/*
		 * The UUT responds with SVIDs = DisplayPort (0xFF01)
		 * in the VDM Header.
		 */
		if (PD_VDO_VID(pdc.payload[0]) != USB_SID_DISPLAYPORT)
			return PDC_FAIL;

		return PDC_DONE;
	}

	return PDC_CONTINUE;
}

static int proc_pd2_e2_dfp(void)
{
	int ret;
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;
	uint8_t vdo_cmd;

	/*
	 * a) The Tester enables Rd for emulating a UFP_U,
	 * as well as Ra for emulating a Vconn-Powered
	 * Accessory.
	 */
	pdc.flags |= PDC_FLAGS_RA_RD;

	/* The Tester establishes an explicit contract with the UUT */
	if (!(pdc.flags & PDC_FLAGS_CONNECTED)) {
		ret = proc_pd2_e1_dfp();
		if (ret != PDC_DONE)
			return ret;
		/* We are connected to a UUT DFP */
		pdc.step = b;
		pdc.state = 0;
	}

	switch (pdc.step) {
	case b:
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			/*
			 * The Tester awaits Discover Identity from the UUT and
			 * responds appropriately with GoodCrc and Discover
			 * Identity ACK. The test is not applicable if Discover
			 * Identity is not received timely.
			 */
			if (tcpm_has_pending_message(pdc.port)) {
				pdc.timer = get_time().val + (5500 * MSEC);
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
						 pdc.payload,
						 &pdc.header)
							== EC_SUCCESS) {
					/* Get header contents */
					type = PD_HEADER_TYPE(pdc.header);
					cnt = PD_HEADER_CNT(pdc.header);
					ext = PD_HEADER_EXT(pdc.header);
					vdo_cmd = PD_VDO_CMD(pdc.payload[0]);

					/* Wait for Discover Identity */
					if (type == PD_DATA_VENDOR_DEF &&
						cnt > 0 &&
						ext == 0 &&
						vdo_cmd == CMD_DISCOVER_IDENT) {
						/* Now send the ACK */
						pdc.state++;
					} else {
						pdc_send_control_msg(
							PD_CTRL_REJECT,
							TCPC_TX_SOP);
						return PDC_CONTINUE;
					}
				} else {
					/* dequeue error */
					return PDC_FAIL;
				}
			} else if (get_time().val > pdc.timer) {
				/* No response. Test not applicable */
				return PDC_DONE;
			} else {
				return PDC_CONTINUE;
			}
			/* fall through */
		case 2:
			/*
			 * The Tester sends an Discover Identity ACK.
			 */
			pdc.payload[1] = 0x6c002109;
			pdc.payload[2] = 0x0000037e;
			pdc.payload[3] = 0x01010001;
			pdc.payload[4] = 0x00000039;
			ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
				CMD_DISCOVER_IDENT | VDO_CMDT(CMDT_RSP_ACK),
				USB_SID_DISPLAYPORT,
				4,
				TCPC_TX_SOP,
				0); /* No response timeout */

			if (ret != PDC_DONE)
				return ret;

			if (pdc_next_step())
				return PDC_DONE;
		}
		/* fall through */
	case c:
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			/*
			 * The Tester awaits Discover SVIDs from the UUT and
			 * responds appropriately with GoodCrc and Discover
			 * SVIDs ACK using DP SVID in SVID0 and 0x0000 in SVID1.
			 * The test is not applicable if Discover SVIDs is not
			 * received timely.
			 */
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
						 pdc.payload,
						 &pdc.header)
							== EC_SUCCESS) {
					/* Get header contents */
					type = PD_HEADER_TYPE(pdc.header);
					cnt = PD_HEADER_CNT(pdc.header);
					ext = PD_HEADER_EXT(pdc.header);
					vdo_cmd = PD_VDO_CMD(pdc.payload[0]);

					/* Wait for Discover Identity */
					if (type == PD_DATA_VENDOR_DEF &&
						cnt > 0 &&
						ext == 0 &&
						vdo_cmd == CMD_DISCOVER_SVID) {
						pdc.state++;
					} else {
						return PDC_CONTINUE;
					}
				} else {
					/* dequeue error */
					return PDC_FAIL;
				}
			} else if (get_time().val > pdc.timer) {
				pdc_report_failure(PDC_FAILURE_NO_RESPONSE,
								pdc.step);
				/* No response. Test not applicable */
				return PDC_DONE;
			} else {
				return PDC_CONTINUE;
			}
			/* fall through */
		case 2:
			pdc.payload[1] = 0xff010000;
			ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
				CMD_DISCOVER_SVID | VDO_CMDT(CMDT_RSP_ACK),
				USB_SID_DISPLAYPORT,
				1,
				TCPC_TX_SOP,
				0); /* No response timeout */
			if (ret != PDC_DONE)
				return ret;

			if (pdc_next_step())
				return PDC_DONE;
		}
		/* fall through */
	case d:
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			/*
			 * The Tester awaits DisplayPort Discover Modes from the
			 * UUT and responds appropriately with GoodCrc and
			 * Discover Modes ACK (Port Data Role = UFP, Port
			 * Capability = Both UFP_D and DFP_D Capable,
			 * DP v1.3 = Yes). The test is not applicable if
			 * DisplayPort Discover Modes is not received timely.
			 */
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
						 pdc.payload,
						 &pdc.header)
							== EC_SUCCESS) {
					/* Get header contents */
					type = PD_HEADER_TYPE(pdc.header);
					cnt = PD_HEADER_CNT(pdc.header);
					ext = PD_HEADER_EXT(pdc.header);
					vdo_cmd = PD_VDO_CMD(pdc.payload[0]);

					/* Wait for Discover Identity */
					if (type == PD_DATA_VENDOR_DEF &&
						cnt > 0 &&
						ext == 0 &&
						vdo_cmd == CMD_DISCOVER_MODES) {
						pdc.state++;
					} else {
						return PDC_CONTINUE;
					}
				} else {
					/* dequeue error */
					return PDC_FAIL;
				}
			} else if (get_time().val > pdc.timer) {
				pdc_report_failure(PDC_FAILURE_NO_RESPONSE,
								pdc.step);
				/* No response. Test not applicable */
				return PDC_DONE;
			} else {
				return PDC_CONTINUE;
			}
			/* fall through */
		case 2:
			pdc.payload[1] = 0x00000c07;
			ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
				CMD_DISCOVER_MODES | VDO_CMDT(CMDT_RSP_ACK),
				USB_SID_DISPLAYPORT,
				1,
				TCPC_TX_SOP,
				0); /* No response timeout */
			if (ret != PDC_DONE)
				return ret;

			if (pdc_next_step())
				return PDC_DONE;
		}
		/* fall through */
	case e:
		switch (pdc.state) {
		case 0:
			pdc.timer = get_time().val + (5500 * MSEC);
			pdc.state++;
			/* fall through */
		case 1:
			/*
			 * The Tester awaits DisplayPort Enter Mode from the
			 * UUT and responds appropriately with GoodCrc and Enter
			 * Mode ACK. The test fails if DisplayPort Enter Mode is
			 * not received timely
			 */
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
						 pdc.payload,
						 &pdc.header)
							== EC_SUCCESS) {
					/* Get header contents */
					type = PD_HEADER_TYPE(pdc.header);
					cnt = PD_HEADER_CNT(pdc.header);
					ext = PD_HEADER_EXT(pdc.header);
					vdo_cmd = PD_VDO_CMD(pdc.payload[0]);

					/* Wait for Discover Identity */
					if (type == PD_DATA_VENDOR_DEF &&
						cnt > 0 &&
						ext == 0 &&
						vdo_cmd == CMD_ENTER_MODE) {
						pdc.state++;
					} else {
						return PDC_CONTINUE;
					}
				} else {
					/* dequeue error */
					return PDC_FAIL;
				}
			} else if (get_time().val > pdc.timer) {
				pdc_report_failure(PDC_FAILURE_NO_RESPONSE,
								pdc.step);
				/* No response. Test not applicable */
				return PDC_DONE;
			} else {
				return PDC_CONTINUE;
			}
			/* fall through */
		case 2:
			ret = pdc_send_data_msg(PD_DATA_VENDOR_DEF,
				CMD_ENTER_MODE | VDO_CMDT(CMDT_RSP_ACK),
				USB_SID_DISPLAYPORT,
				0,
				TCPC_TX_SOP,
				0); /* No response timeout */
			if (ret != PDC_DONE)
				return ret;
		}
		return PDC_DONE;
	}

	/* Not implemented */
	return PDC_CONTINUE;
}

static bool proc_pd2_e2(void)
{
	int ret = 0;

	switch (pdc.uut_role) {
	case PD_UFP:
		ret = proc_pd2_e2_ufp();
		break;
	case PD_DFP:
		ret = proc_pd2_e2_dfp();
		break;
	}

	if (ret < 0) {
		ccprintf("FAILED at step %d\n", pdc.step);
		return false;
	} else if (ret > 0) {
		/* Continue */
		return true;
	}

	ccprintf("DONE\n");
	return false;
}
