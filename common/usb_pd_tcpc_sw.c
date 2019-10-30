#ifdef CONFIG_COMMON_RUNTIME
static struct mutex pd_crc_lock;
#endif


/* Encode 5 bits using Biphase Mark Coding */
#define BMC(x)   ((x &  1 ? 0x001 : 0x3FF) \
		^ (x &  2 ? 0x004 : 0x3FC) \
		^ (x &  4 ? 0x010 : 0x3F0) \
		^ (x &  8 ? 0x040 : 0x3C0) \
		^ (x & 16 ? 0x100 : 0x300))

/* 4b/5b + Bimark Phase encoding */
static const uint16_t bmc4b5b[] = {
/* 0 = 0000 */ BMC(0x1E) /* 11110 */,
/* 1 = 0001 */ BMC(0x09) /* 01001 */,
/* 2 = 0010 */ BMC(0x14) /* 10100 */,
/* 3 = 0011 */ BMC(0x15) /* 10101 */,
/* 4 = 0100 */ BMC(0x0A) /* 01010 */,
/* 5 = 0101 */ BMC(0x0B) /* 01011 */,
/* 6 = 0110 */ BMC(0x0E) /* 01110 */,
/* 7 = 0111 */ BMC(0x0F) /* 01111 */,
/* 8 = 1000 */ BMC(0x12) /* 10010 */,
/* 9 = 1001 */ BMC(0x13) /* 10011 */,
/* A = 1010 */ BMC(0x16) /* 10110 */,
/* B = 1011 */ BMC(0x17) /* 10111 */,
/* C = 1100 */ BMC(0x1A) /* 11010 */,
/* D = 1101 */ BMC(0x1B) /* 11011 */,
/* E = 1110 */ BMC(0x1C) /* 11100 */,
/* F = 1111 */ BMC(0x1D) /* 11101 */,
/* Sync-1      K-code       11000 Startsynch #1 */
/* Sync-2      K-code       10001 Startsynch #2 */
/* RST-1       K-code       00111 Hard Reset #1 */
/* RST-2       K-code       11001 Hard Reset #2 */
/* EOP         K-code       01101 EOP End Of Packet */
/* Reserved    Error        00000 */
/* Reserved    Error        00001 */
/* Reserved    Error        00010 */
/* Reserved    Error        00011 */
/* Reserved    Error        00100 */
/* Reserved    Error        00101 */
/* Reserved    Error        00110 */
/* Reserved    Error        01000 */
/* Reserved    Error        01100 */
/* Reserved    Error        10000 */
/* Reserved    Error        11111 */
};

static const uint8_t dec4b5b[] = {
/* Error    */ 0x10 /* 00000 */,
/* Error    */ 0x10 /* 00001 */,
/* Error    */ 0x10 /* 00010 */,
/* Error    */ 0x10 /* 00011 */,
/* Error    */ 0x10 /* 00100 */,
/* Error    */ 0x10 /* 00101 */,
/* Error    */ 0x10 /* 00110 */,
/* RST-1    */ 0x13 /* 00111 K-code: Hard Reset #1 */,
/* Error    */ 0x10 /* 01000 */,
/* 1 = 0001 */ 0x01 /* 01001 */,
/* 4 = 0100 */ 0x04 /* 01010 */,
/* 5 = 0101 */ 0x05 /* 01011 */,
/* Error    */ 0x10 /* 01100 */,
/* EOP      */ 0x15 /* 01101 K-code: EOP End Of Packet */,
/* 6 = 0110 */ 0x06 /* 01110 */,
/* 7 = 0111 */ 0x07 /* 01111 */,
/* Error    */ 0x10 /* 10000 */,
/* Sync-2   */ 0x12 /* 10001 K-code: Startsynch #2 */,
/* 8 = 1000 */ 0x08 /* 10010 */,
/* 9 = 1001 */ 0x09 /* 10011 */,
/* 2 = 0010 */ 0x02 /* 10100 */,
/* 3 = 0011 */ 0x03 /* 10101 */,
/* A = 1010 */ 0x0A /* 10110 */,
/* B = 1011 */ 0x0B /* 10111 */,
/* Sync-1   */ 0x11 /* 11000 K-code: Startsynch #1 */,
/* RST-2    */ 0x14 /* 11001 K-code: Hard Reset #2 */,
/* C = 1100 */ 0x0C /* 11010 */,
/* D = 1101 */ 0x0D /* 11011 */,
/* E = 1110 */ 0x0E /* 11100 */,
/* F = 1111 */ 0x0F /* 11101 */,
/* 0 = 0000 */ 0x00 /* 11110 */,
/* Error    */ 0x10 /* 11111 */,
};

static inline int encode_short(int port, int off, uint16_t val16)
{
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 0) & 0xF]);
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 4) & 0xF]);
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 8) & 0xF]);
	return pd_write_sym(port, off, bmc4b5b[(val16 >> 12) & 0xF]);
}

int encode_word(int port, int off, uint32_t val32)
{
	off = encode_short(port, off, (val32 >> 0) & 0xFFFF);
	return encode_short(port, off, (val32 >> 16) & 0xFFFF);
}

/* prepare a 4b/5b-encoded PD message to send */
int prepare_message(int port, uint16_t header, uint8_t cnt,
		   const uint32_t *data)
{
	int off, i;
	/* 64-bit preamble */
	off = pd_write_preamble(port);
#if defined(CONFIG_USB_TYPEC_VPD) || defined(CONFIG_USB_TYPEC_CTVPD)
	/* Start Of Packet Prime: 2x Sync-1 + 2x Sync-3 */
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC3));
	off = pd_write_sym(port, off, BMC(PD_SYNC3));
#else
	/* Start Of Packet: 3x Sync-1 + 1x Sync-2 */
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC2));
#endif
	/* header */
	off = encode_short(port, off, header);

#ifdef CONFIG_COMMON_RUNTIME
	mutex_lock(&pd_crc_lock);
#endif

	crc32_init();
	crc32_hash16(header);
	/* data payload */
	for (i = 0; i < cnt; i++) {
		off = encode_word(port, off, data[i]);
		crc32_hash32(data[i]);
	}
	/* CRC */
	off = encode_word(port, off, crc32_result());

#ifdef CONFIG_COMMON_RUNTIME
	mutex_unlock(&pd_crc_lock);
#endif

	/* End Of Packet */
	off = pd_write_sym(port, off, BMC(PD_EOP));
	/* Ensure that we have a final edge */
	return pd_write_last_edge(port, off);
}

static int send_hard_reset(int port)
{
	int off;

	if (debug_level >= 1)
		CPRINTF("C%d Send hard reset\n", port);

	/* 64-bit preamble */
	off = pd_write_preamble(port);
	/* Hard-Reset: 3x RST-1 + 1x RST-2 */
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST2));
	/* Ensure that we have a final edge */
	off = pd_write_last_edge(port, off);
	/* Transmit the packet */
	if (pd_start_tx(port, pd[port].polarity, off) < 0)
		return PD_TX_ERR_COLLISION;
	pd_tx_done(port, pd[port].polarity);
	/* Keep RX monitoring on */
	pd_rx_enable_monitoring(port);
	return 0;
}

static int send_validate_message(int port, uint16_t header,
				 const uint32_t *data)
{
	int r;
	static uint32_t payload[7];
	uint8_t expected_msg_id = PD_HEADER_ID(header);
	uint8_t cnt = PD_HEADER_CNT(header);
	int retries = PD_HEADER_TYPE(header) == PD_DATA_SOURCE_CAP ?
		      0 : PD_RETRY_COUNT;

	/* retry 3 times if we are not getting a valid answer */
	for (r = 0; r <= retries; r++) {
		int bit_len, head;
		/* write the encoded packet in the transmission buffer */
		bit_len = prepare_message(port, header, cnt, data);
		/* Transmit the packet */
		if (pd_start_tx(port, pd[port].polarity, bit_len) < 0) {
			/*
			 * Collision detected, return immediately so we can
			 * respond to what we have received.
			 */
			return PD_TX_ERR_COLLISION;
		}
		pd_tx_done(port, pd[port].polarity);
		/*
		 * If this is the first attempt, leave RX monitoring off,
		 * and do a blocking read of the channel until timeout or
		 * packet received. If we failed the first try, enable
		 * interrupt and yield to other tasks, so that we don't
		 * starve them.
		 */
		if (r) {
			pd_rx_enable_monitoring(port);
			/* Wait for message receive timeout */
			if (task_wait_event(USB_PD_RX_TMOUT_US) ==
			    TASK_EVENT_TIMER)
				continue;
			/*
			 * Make sure we woke up due to rx recd, otherwise
			 * we need to manually start
			 */
			if (!pd_rx_started(port)) {
				pd_rx_disable_monitoring(port);
				pd_rx_start(port);
			}
		} else {
			/* starting waiting for GoodCrc */
			pd_rx_start(port);
		}
		/* read the incoming packet if any */
		head = pd_analyze_rx(port, payload);
		pd_rx_complete(port);
		/* keep RX monitoring on to avoid collisions */
		pd_rx_enable_monitoring(port);
		if (head > 0) { /* we got a good packet, analyze it */
			int type = PD_HEADER_TYPE(head);
			int nb = PD_HEADER_CNT(head);
			uint8_t id = PD_HEADER_ID(head);
			if (type == PD_CTRL_GOOD_CRC && nb == 0 &&
			    id == expected_msg_id) {
				/* got the GoodCRC we were expecting */
				/* do not catch last edges as a new packet */
				udelay(20);
				return bit_len;
			} else {
				/*
				 * we have received a good packet
				 * but not the expected GoodCRC,
				 * the other side is trying to contact us,
				 * bail out immediately so we can get the retry.
				 */
				return PD_TX_ERR_INV_ACK;
			}
		}
	}
	/* we failed all the re-transmissions */
	if (debug_level >= 1)
		CPRINTF("TX NOACK%d %04x/%d\n", port, header, cnt);
	return PD_TX_ERR_GOODCRC;
}

static void send_goodcrc(int port, int id)
{
	uint16_t header = PD_HEADER(PD_CTRL_GOOD_CRC, pd[port].power_role,
			pd[port].data_role, id, 0, 0, 0);
	int bit_len = prepare_message(port, header, 0, NULL);

	if (pd_start_tx(port, pd[port].polarity, bit_len) < 0)
		/* another packet recvd before we could send goodCRC */
		return;
	pd_tx_done(port, pd[port].polarity);
	/* Keep RX monitoring on */
	pd_rx_enable_monitoring(port);
}

static void bist_mode_2_tx(int port)
{
	int bit;

	CPRINTF("BIST 2: p%d\n", port);
	/*
	 * build context buffer with 5 bytes, where the data is
	 * alternating 1's and 0's.
	 */
	bit = pd_write_sym(port, 0,   BMC(0x15));
	bit = pd_write_sym(port, bit, BMC(0x0a));
	bit = pd_write_sym(port, bit, BMC(0x15));
	bit = pd_write_sym(port, bit, BMC(0x0a));

	/* start a circular DMA transfer */
	pd_tx_set_circular_mode(port);
	pd_start_tx(port, pd[port].polarity, bit);

	task_wait_event(PD_T_BIST_TRANSMIT);

	/* clear dma circular mode, will also stop dma */
	pd_tx_clear_circular_mode(port);
	/* finish and cleanup transmit */
	pd_tx_done(port, pd[port].polarity);
}

static inline int decode_short(int port, int off, uint16_t *val16)
{
	uint32_t w;
	int end;

	end = pd_dequeue_bits(port, off, 20, &w);

#if 0 /* DEBUG */
	CPRINTS("%d-%d: %05x %x:%x:%x:%x\n",
		off, end, w,
		dec4b5b[(w >> 15) & 0x1f], dec4b5b[(w >> 10) & 0x1f],
		dec4b5b[(w >>  5) & 0x1f], dec4b5b[(w >>  0) & 0x1f]);
#endif
	*val16 = dec4b5b[w & 0x1f] |
		(dec4b5b[(w >>  5) & 0x1f] << 4) |
		(dec4b5b[(w >> 10) & 0x1f] << 8) |
		(dec4b5b[(w >> 15) & 0x1f] << 12);
	return end;
}

static inline int decode_word(int port, int off, uint32_t *val32)
{
	off = decode_short(port, off, (uint16_t *)val32);
	return decode_short(port, off, ((uint16_t *)val32 + 1));
}

/* Convert CC voltage to CC status */
static int cc_voltage_to_status(int port, int cc_volt, int cc_sel)
{
	/* If we have a pull-up, then we are source, check for Rd. */
	if (pd[port].cc_pull == TYPEC_CC_RP) {
		if (CC_NC(port, cc_volt, cc_sel))
			return TYPEC_CC_VOLT_OPEN;
		else if (CC_RA(port, cc_volt, cc_sel))
			return TYPEC_CC_VOLT_RA;
		else
			return TYPEC_CC_VOLT_RD;
	/* If we have a pull-down, then we are sink, check for Rp. */
	}
#ifdef CONFIG_USB_PD_DUAL_ROLE
	else if (pd[port].cc_pull == TYPEC_CC_RD) {
		if (cc_volt >= TYPE_C_SRC_3000_THRESHOLD)
			return TYPEC_CC_VOLT_RP_3_0;
		else if (cc_volt >= TYPE_C_SRC_1500_THRESHOLD)
			return TYPEC_CC_VOLT_RP_1_5;
		else if (CC_RP(cc_volt))
			return TYPEC_CC_VOLT_RP_DEF;
		else
			return TYPEC_CC_VOLT_OPEN;
	}
#endif
	/* If we are open, then always return 0 */
	else
		return 0;
}

int pd_read_cc_status(int port, int cc)
{
	return cc_voltage_to_status(port, pd_adc_read(port, i), i);
}

int pd_analyze_rx(int port, uint32_t *payload)
{
	int bit;
	char *msg = "---";
	uint32_t val = 0;
	union pd_header_sop phs;
	uint32_t pcrc, ccrc;
	int p, cnt;
	uint32_t eop;

	pd_init_dequeue(port);

	/* Detect preamble */
	bit = pd_find_preamble(port);
	if (bit == PD_RX_ERR_HARD_RESET || bit == PD_RX_ERR_CABLE_RESET) {
		/* Hard reset or cable reset */
		invalidate_last_message_id(port);
		return bit;
	} else if (bit < 0) {
		msg = "Preamble";
		goto packet_err;
	}

	/* Find the Start Of Packet sequence */
	while (bit > 0) {
		bit = pd_dequeue_bits(port, bit, 20, &val);
#if defined(CONFIG_USB_TYPEC_VPD) || defined(CONFIG_USB_TYPEC_CTVPD)
		if (val == PD_SOP_PRIME) {
			break;
		} else if (val == PD_SOP) {
			CPRINTF("SOP\n");
			return PD_RX_ERR_UNSUPPORTED_SOP;
		} else if (val == PD_SOP_PRIME_PRIME) {
			CPRINTF("SOP''\n");
			return PD_RX_ERR_UNSUPPORTED_SOP;
		}
#else /* CONFIG_USB_TYPEC_VPD || CONFIG_USB_TYPEC_CTVPD */
#ifdef CONFIG_USB_PD_DECODE_SOP
		if (val == PD_SOP || val == PD_SOP_PRIME ||
						val == PD_SOP_PRIME_PRIME)
			break;
#else
		if (val == PD_SOP) {
			break;
		} else if (val == PD_SOP_PRIME) {
			CPRINTF("SOP'\n");
			return PD_RX_ERR_UNSUPPORTED_SOP;
		} else if (val == PD_SOP_PRIME_PRIME) {
			CPRINTF("SOP''\n");
			return PD_RX_ERR_UNSUPPORTED_SOP;
		}
#endif /* CONFIG_USB_PD_DECODE_SOP */
#endif /* CONFIG_USB_TYPEC_VPD || CONFIG_USB_TYPEC_CTVPD */
	}
	if (bit < 0) {
#ifdef CONFIG_USB_PD_DECODE_SOP
		if (val == PD_SOP)
			msg = "SOP";
		else if (val == PD_SOP_PRIME)
			msg = "SOP'";
		else if (val == PD_SOP_PRIME_PRIME)
			msg = "SOP''";
		else
			msg = "SOP*";
#else
		msg = "SOP";
#endif
		goto packet_err;
	}

	phs.head = 0;

	/* read header */
	bit = decode_short(port, bit, &phs.pd_header);

#ifdef CONFIG_COMMON_RUNTIME
	mutex_lock(&pd_crc_lock);
#endif

	crc32_init();
	crc32_hash16(phs.pd_header);
	cnt = PD_HEADER_CNT(phs.pd_header);

#ifdef CONFIG_USB_PD_DECODE_SOP
	/* Encode message address */
	if (val == PD_SOP) {
		phs.head |= PD_HEADER_SOP(PD_MSG_SOP);
	} else if (val == PD_SOP_PRIME) {
		phs.head |= PD_HEADER_SOP(PD_MSG_SOPP);
	} else if (val == PD_SOP_PRIME_PRIME) {
		phs.head |= PD_HEADER_SOP(PD_MSG_SOPPP);
	} else {
		msg = "SOP*";
		goto packet_err;
	}
#endif

	/* read payload data */
	for (p = 0; p < cnt && bit > 0; p++) {
		bit = decode_word(port, bit, payload+p);
		crc32_hash32(payload[p]);
	}
	ccrc = crc32_result();

#ifdef CONFIG_COMMON_RUNTIME
	mutex_unlock(&pd_crc_lock);
#endif

	if (bit < 0) {
		msg = "len";
		goto packet_err;
	}

	/* check transmitted CRC */
	bit = decode_word(port, bit, &pcrc);
	if (bit < 0 || pcrc != ccrc) {
		msg = "CRC";
		if (pcrc != ccrc)
			bit = PD_RX_ERR_CRC;
		if (debug_level >= 1)
			CPRINTF("CRC%d %08x <> %08x\n", port, pcrc, ccrc);
		goto packet_err;
	}

	/*
	 * Check EOP. EOP is 5 bits, but last bit may not be able to
	 * be dequeued, depending on ending state of CC line, so stop
	 * at 4 bits (assumes last bit is 0).
	 */
	bit = pd_dequeue_bits(port, bit, 4, &eop);
	if (bit < 0 || eop != PD_EOP) {
		msg = "EOP";
		goto packet_err;
	}

	return phs.head;
packet_err:
	if (debug_level >= 2)
		pd_dump_packet(port, msg);
	else
		CPRINTF("RXERR%d %s\n", port, msg);
	return bit;
}
