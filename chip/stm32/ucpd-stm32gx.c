/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32GX UCPD module for Chrome EC */

#include "clock.h"
#include "console.h"
#include "common.h"
#include "driver/tcpm/tcpm.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "ucpd-stm32gx.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
/*
 * UCPD is fed directly from HSI which is @ 16MHz. The ucpd_clk goes to
 * a prescaler who's output feeds the 'half-bit' divider which is used
 * to generate clock for delay counters and BMC Rx/Tx blocks. The rx is
 * desiged to work in freq ranges of 6 <--> 18 MHz, however recommended
 * range is 9 <--> 18 MHz.
 *
 *          ------- @ 16 MHz ---------   @ ~600 kHz   -------------
 * HSI ---->| /psc |-------->| /hbit |--------------->| trans_cnt |
 *          -------          ---------    |           -------------
 *                                        |           -------------
 *                                        |---------->| ifrgap_cnt|
 *                                                    -------------
 * Requirements:
 *   1. hbit_clk ~= 600 kHz: 16 MHz / 600 kHz = 26.67
 *   2. tTransitionWindow - 12 to 20 uSec
 *   3. tInterframGap - uSec
 *
 * hbit_clk = HSI_clk / 26 = 615,385 kHz = 1.625 uSec period
 * tTransitionWindow = 1.625 uS * 8 = 13 uS
 * tInterFrameGap = 1.625 uS * 17 = 27.625 uS
 */
#define UCPD_PSC_DIV 1
#define UCPD_HBIT_DIV 27
#define UCPD_TRANSWIN_CNT 8
#define UCPD_IFRGAP_CNT 17
#define UCPD_BUF_LEN 30

#define TX_MSG_LOG_LEN 25
struct tx_msg_info {
	uint8_t tx_type;
	uint8_t msg_type;
	uint8_t obj_len;
	uint8_t rev;
	uint32_t ts;
};

#define RX_MSG_LOG_LEN 25
struct rx_msg_info {
	uint16_t header;
	uint8_t msg_type;
	uint8_t cnt;
	uint8_t rev;
	uint32_t ts;
};
static int tx_msg_log_cnt;
static int tx_msg_log_idx;
static struct tx_msg_info tx_log[TX_MSG_LOG_LEN];
static int rx_msg_log_cnt;
static int rx_msg_log_idx;
static struct rx_msg_info rx_log[RX_MSG_LOG_LEN];

static int psc_div = UCPD_PSC_DIV;
static int hbit_div = UCPD_HBIT_DIV;

struct msg_header_info {
	uint8_t pr;
	uint8_t dr;
};

static struct msg_header_info msg_header;

static int ucpd_tx_byte_count;
static uint8_t ucpd_tx_data_buf[UCPD_BUF_LEN];
static int ucpd_rx_byte_count;
static uint8_t ucpd_rx_buffer[UCPD_BUF_LEN];
static uint8_t ucpd_rxdr[UCPD_BUF_LEN];
static uint8_t ucpd_rxdr_idx;

#define UCPD_IMR_RX_INT_MASK (STM32_UCPD_IMR_RXNEIE| \
			      STM32_UCPD_IMR_RXORDDETIE | \
			      STM32_UCPD_IMR_RXHRSTDETIE |	\
			      STM32_UCPD_IMR_RXOVRIE |		\
			      STM32_UCPD_IMR_RXMSGENDIE)

static int ucpd_txorderset[] = {
	TX_ORDERSET_SOP,
	TX_ORDERSET_SOP,
	TX_ORDERSET_SOP2,
	TX_ORDERSET_HARD_RESET,
	TX_ORDERSET_CABLE_RESET,
	TX_ORDERSET_SOP1_DEBUG,
	TX_ORDERSET_SOP2_DEBUG,
};

//static struct mutex ucpd_tx_mutex;
enum  debug_gpio {
	TRIGGER_1 = 0,
	TRIGGER_2,
};

static inline void ucpd_debug_gpio(int trigger, int enable)
{
	enum gpio_signal signal = (trigger == TRIGGER_1) ?
		GPIO_TRIGGER_1 : GPIO_TRIGGER_2;

	gpio_set_level(signal, enable);
}

static void ucpd_add_rxmsg(uint32_t sr, uint16_t header)
{
	uint16_t rx_hdr = (ucpd_rxdr[1] << 8) | ucpd_rxdr[0];
	uint32_t ts = __hw_clock_source_read();
	int idx = rx_msg_log_idx;

	if (rx_msg_log_cnt++ < RX_MSG_LOG_LEN) {
		rx_log[idx].header = header;
		rx_log[idx].msg_type = PD_HEADER_TYPE(rx_hdr);
		rx_log[idx].cnt = PD_HEADER_CNT(rx_hdr);
		rx_log[idx].rev = PD_HEADER_REV(rx_hdr);
		rx_log[idx].ts = ts;
		rx_msg_log_idx++;
	}
}

static void ucpd_send_good_crc(int port, uint16_t rx_header)
{
	int msg_id;
	int rev_id;
	uint16_t tx_header;
	uint32_t data = 0;

	/*
	 * A GoodCRC message shall be sent by receiver to ack that the previous
	 * message was correctly received. The GoodCRC message shall return the
	 * rx message's msg_id field. The one exception is for GoodCRC messages,
	 * which do not generate a GoodCRC response
	 */
	if (!PD_HEADER_CNT(rx_header) && PD_HEADER_TYPE(rx_header) ==
	    PD_CTRL_GOOD_CRC) {
		ucpd_debug_gpio(TRIGGER_2, 0);
		return;
	}

	/*
	 * PD Header:
	 *   Extended   b15    -> set to 0 for control messages
	 *   Count      b14:12 -> number of 32 bit data objects = 0 for ctrl msg
	 *   MsgID      b11:9  -> running byte counter (extracted from rx msg)
	 *   Power Role b8     -> stored in static, from set_msg_header()
	 *   Spec Rev   b7:b6  -> PD spec revision (extracted from rx msg)
	 *   Data Role  b5     -> stored in static, from set_msg_header
	 *   Msg Type   b4:b0  -> data or ctrl type = PD_CTRL_GOOD_CRC
	 */
	/* construct header message */
	msg_id = PD_HEADER_ID(rx_header);
	rev_id = PD_HEADER_REV(rx_header);
	tx_header = PD_HEADER(PD_CTRL_GOOD_CRC, msg_header.pr, msg_header.dr,
			      msg_id, 0, rev_id, 0);

	ucpd_debug_gpio(TRIGGER_2, 0);
	/* Initiate sending the good CRC control message */
	stm32gx_ucpd_transmit(port, TCPC_TX_SOP, tx_header, &data);
}


static void ucpd_hard_reset_rx_log(void)
{
	/* CPRINTS("ucpd: hard reset recieved"); */
}
DECLARE_DEFERRED(ucpd_hard_reset_rx_log);

static int sr_hard_reset;

static void ucpd_hard_reset_tx_log(void)
{

}
DECLARE_DEFERRED(ucpd_hard_reset_tx_log);

static void ucpd_port_enable(int port, int enable)
{
	if (enable)
		STM32_UCPD_CFGR1(port) |= STM32_UCPD_CFGR1_UCPDEN;
	else
		STM32_UCPD_CFGR1(port) &= ~STM32_UCPD_CFGR1_UCPDEN;
}

static int ucpd_is_cc_pull_active(int port, int cc_line)
{
	int cc_enable = STM32_UCPD_CR(port) & STM32_UCPD_CR_CCENABLE_MASK >>
		STM32_UCPD_CR_CCENABLE_SHIFT;

	return (cc_enable >> cc_line);
}

static void ucpd_tx_data_byte(int port)
{
	STM32_UCPD_TXDR(port) =  ucpd_tx_data_buf[ucpd_tx_byte_count++];
}

static void ucpd_rx_data_byte(int port)
{
	if (ucpd_rx_byte_count < UCPD_BUF_LEN)
		ucpd_rx_buffer[ucpd_rx_byte_count++] = STM32_UCPD_RXDR(port);

	ucpd_rxdr[ucpd_rxdr_idx++] = STM32_UCPD_RXDR(port);
}

static void ucpd_clear_tx_int(int port)
{
	STM32_UCPD_IMR(port) &= ~(STM32_UCPD_IMR_TXISIE |
				  STM32_UCPD_IMR_TXMSGDISCIE |
				  STM32_UCPD_IMR_TXMSGSENTIE |
				  STM32_UCPD_IMR_TXMSGABTIE);
}

static int sr_log;
static int msg_hdr;
static void ucpd_irq_log(void)
{
	//CPRINTS("ucpd: irq: sr = 0x%x", sr_log);
}
DECLARE_DEFERRED(ucpd_irq_log);

static int sr_save;
static void ucpd_irq_txmsg_log(void)
{
	sr_save = 0;
}
DECLARE_DEFERRED(ucpd_irq_txmsg_log);

static void ucpd_irq_txis_log(void)
{

}
DECLARE_DEFERRED(ucpd_irq_txis_log);

void stm32gx_ucpd1_irq(void)
{
	/* STM32_IRQ_UCPD indicates this is from UCPD1, so port = 0 */
	int port = 0;
	uint32_t sr = STM32_UCPD_SR(port);
	uint32_t tx_mask = STM32_UCPD_SR_TXMSGSENT | STM32_UCPD_SR_TXMSGABT |
		STM32_UCPD_SR_TXMSGDISC | STM32_UCPD_SR_HRSTSENT |
		STM32_UCPD_SR_HRSTDISC;

	sr_log = sr;
	hook_call_deferred(&ucpd_irq_log_data, 0);

	/* Check for CC events */
	if (sr & (STM32_UCPD_SR_TYPECEVT1 | STM32_UCPD_SR_TYPECEVT2)) {
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
	}

	if (sr & tx_mask) {
		sr_hard_reset = sr;
		hook_call_deferred(&ucpd_hard_reset_tx_log_data, 0);
		ucpd_debug_gpio(TRIGGER_2, 0);
	}

	/* Check for Tx events */
	/* Check for data register empty */
	if (sr & STM32_UCPD_SR_TXIS) {
		sr_save |= STM32_UCPD_SR_TXIS;
		hook_call_deferred(&ucpd_irq_txis_log_data, 0);
		ucpd_tx_data_byte(port);
	}
	/* Check for tx message complete */
	if (sr & STM32_UCPD_SR_TXMSGSENT) {
		sr_save |= STM32_UCPD_SR_TXMSGSENT;
		pd_transmit_complete(port, TCPC_TX_COMPLETE_SUCCESS);
		ucpd_clear_tx_int(port);
		hook_call_deferred(&ucpd_irq_txmsg_log_data, 0);
	}
	if (sr & (STM32_UCPD_SR_TXMSGABT | STM32_UCPD_SR_TXMSGDISC)) {
		pd_transmit_complete(port, TCPC_TX_COMPLETE_FAILED);
		ucpd_clear_tx_int(port);
		sr_save |= (sr & (STM32_UCPD_SR_TXMSGABT |
				  STM32_UCPD_SR_TXMSGDISC));
		hook_call_deferred(&ucpd_irq_txmsg_log_data, 0);
	}
	if (sr & (STM32_UCPD_SR_HRSTSENT | STM32_UCPD_SR_HRSTDISC)) {

	}

	/* Check for Rx Events */
	/* Check first for start of new message */
	if (sr & STM32_UCPD_SR_RXORDDET) {
		ucpd_rx_byte_count = 0;
		ucpd_rxdr_idx = 0;
		ucpd_debug_gpio(TRIGGER_1, 1);
	}
	/* Check for byte received */
	if (sr & STM32_UCPD_SR_RXNE) {
		ucpd_rx_data_byte(port);
	}
	/* Check for end of message */
	if (sr & STM32_UCPD_SR_RXMSGEND) {
		ucpd_debug_gpio(TRIGGER_1, 0);
		/* Check for errors */
		if (!(sr & STM32_UCPD_SR_RXERR)) {
			uint16_t *rx_header = (uint16_t *)ucpd_rxdr;
			//uint16_t *rx_header = (uint16_t *)ucpd_rx_buffer;

			ucpd_debug_gpio(TRIGGER_2, 1);
			ucpd_add_rxmsg(sr, *rx_header);
			/* TODO - Add error checking here */
			tcpm_enqueue_message(port);
			/* Send GoodCRC message (if required) */
			ucpd_send_good_crc(port, *rx_header);
		};
	}
	/* Check for fault conditions */
	if (sr & STM32_UCPD_SR_RXHRSTDET) {
		/* hard reset received */
		pd_execute_hard_reset(port);
		task_set_event(PD_PORT_TO_TASK_ID(port), TASK_EVENT_WAKE, 0);
		hook_call_deferred(&ucpd_hard_reset_rx_log_data, 0);
	}

	/* Clear interrupts now that PD events have been set */
	STM32_UCPD_ICR(port) = sr;
}
DECLARE_IRQ(STM32_IRQ_UCPD1, stm32gx_ucpd1_irq, 1);

int stm32gx_ucpd_init(int port)
{
	uint32_t cfgr1_reg;
	uint32_t moder_reg;

	/*
	* After exiting reset, stm32gx will have dead battery mode enabled by
	* deafult which connects Rd to CC1/CC2. This should be disabled when EC
	* is powered up.
	*/
	STM32_PWR_CR3 |= STM32_PWR_CR3_UCPD1_DBDIS;

	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;

	/* Make sure CC1/CC2 pins PB4/PB6 are set for analog mode */
	moder_reg = STM32_GPIO_MODER(GPIO_B);
	moder_reg |= 0x330;
	STM32_GPIO_MODER(GPIO_B) = moder_reg;
	/*
	 * CFGR1 must be written when UCPD peripheral is disabled. Note that
	 * disabling ucpd causes the peripheral to quit any ongoing activity and
	 * sets all ucpd registers back their default values.
	 */
	ucpd_port_enable(port, 0);

	cfgr1_reg = STM32_UCPD_CFGR1_PSC_CLK_VAL(psc_div - 1) |
		STM32_UCPD_CFGR1_TRANSWIN_VAL(UCPD_TRANSWIN_CNT - 1) |
		STM32_UCPD_CFGR1_IFRGAP_VAL(UCPD_IFRGAP_CNT - 1) |
		STM32_UCPD_CFGR1_HBITCLKD_VAL(hbit_div -1);
	STM32_UCPD_CFGR1(port) = cfgr1_reg;

	/*
	 * Set RXORDSETEN field to control which types of ordered sets the PD
	 * receiver must receive.
	 * SOP, SOP', Hard Reset Det, Cable Reset Det enabled
	 */
	STM32_UCPD_CFGR1(port) |= STM32_UCPD_CFGR1_RXORDSETEN_VAL(0x1B);

	/* Configure and enable DMAs */

	/* TODO(b/): Should rx filtering be enabled */

	/* Enable ucpd  */
	ucpd_port_enable(port, 1);

	/* configure interrupts */
	STM32_UCPD_IMR(port) = STM32_UCPD_IMR_TYPECEVT1IE |
		STM32_UCPD_IMR_TYPECEVT2IE;
	STM32_UCPD_ICR(port) = STM32_UCPD_ICR_TYPECEVT1CF |
		STM32_UCPD_ICR_TYPECEVT2CF;

	/* Enable UCPD interrupts */
	task_enable_irq(STM32_IRQ_UCPD1);

	tx_msg_log_cnt = 0;
	tx_msg_log_idx = 0;

	return EC_SUCCESS;
}

int stm32gx_ucpd_release(int port)
{
	ucpd_port_enable(port, 0);

	/* Does this clear all interrupts that were selected */

	return EC_SUCCESS;
}

int stm32gx_ucpd_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	int vstate_cc1;
	int vstate_cc2;
	int anamode;
	uint32_t sr;

	/* cc_voltage_status is determined from vstate_cc bit field in the
	 * status register. The meaning of the value vstate_cc depends on curent
	 * value of ANAMODE (src/snk).
	 *
	 * vstate_cc maps directly to cc_state from tcpci spec when ANAMODE = 1,
	 * but needs to be modified slightly for case ANAMODE = 0.
         *
	 * If presenting Rp (source), then need to to a circular shift of
	 * vstate_ccx value:
	 *     vstate_cc | cc_state
	 *     ------------------
	 *        0     ->    1
	 *        1     ->    2
	 *        2     ->    0
	 */

	/* Get vstate_ccx values and power role */
	sr = STM32_UCPD_SR(port);
	/* Get Rp or Rd active */
	anamode = !!(STM32_UCPD_CR(port) & STM32_UCPD_CR_ANAMODE);
	vstate_cc1 = (sr & STM32_UCPD_SR_VSTATE_CC1_MASK) >>
		STM32_UCPD_SR_VSTATE_CC1_SHIFT;
	vstate_cc2 = (sr & STM32_UCPD_SR_VSTATE_CC2_MASK) >>
		STM32_UCPD_SR_VSTATE_CC2_SHIFT;

	/* Do circular shift if port == source */
	if (anamode) {
		if (vstate_cc1 != STM32_UCPD_SR_VSTATE_RA)
			vstate_cc1 += 4;
		if (vstate_cc2 != STM32_UCPD_SR_VSTATE_RA)
			vstate_cc2 += 4;
	} else {
		if (vstate_cc1 != STM32_UCPD_SR_VSTATE_OPEN)
			vstate_cc1 = (vstate_cc1 + 1) % 3;
		if (vstate_cc2 != STM32_UCPD_SR_VSTATE_OPEN)
			vstate_cc2 = (vstate_cc2 + 1) % 3;
	}

	*cc1 = vstate_cc1;
	*cc2 = vstate_cc2;

	return EC_SUCCESS;
}

int stm32gx_ucpd_get_role_control(int port)
{
	int role_control = 0;
	int anamode = !!(STM32_UCPD_CR(port) & STM32_UCPD_CR_ANAMODE);
	int rp = (STM32_UCPD_CR(port) & STM32_UCPD_CR_ANASUBMODE_MASK) >>
		STM32_UCPD_CR_ANASUBMODE_SHIFT;

	/*
	 * Get R pull type for CC1/CC2 if that line is enabled
	 *     R_cc1 -> b 1:0
	 *     R_cc2 -> b 3:2
	 *     Rp    -> b 5:4
	 */
	if (ucpd_is_cc_pull_active(port, USBPD_CC_PIN_1))
		role_control |= (anamode + 1);
	if (ucpd_is_cc_pull_active(port, USBPD_CC_PIN_2))
		role_control |= ((anamode + 1) << 2);
	/* add Rp type which is given by anasubmode field */
	role_control |= (rp << 4);

	return role_control;
}

int stm32gx_ucpd_set_cc(int port, int cc_pull, int rp)
{
	uint32_t cr = STM32_UCPD_CR(port);

	/* Always set ANASUBMODE to match desired Rp */
	cr &= ~STM32_UCPD_CR_ANASUBMODE_MASK;
	cr |= STM32_UCPD_CR_ANASUBMODE_VAL(rp);

	/* Disconnect both pull from both CC lines by default */
	cr &= ~STM32_UCPD_CR_CCENABLE_MASK;
	/* Set ANAMODE if cc_pull is Rd */
	if (cc_pull == TYPEC_CC_RD) {
		cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
		cr &= ~(STM32_UCPD_CR_CC1TCDIS | STM32_UCPD_CR_CC2TCDIS);
	/* Clear ANAMODE if cc_pull is Rp */
	} else if (cc_pull == TYPEC_CC_RP) {
		cr &= ~(STM32_UCPD_CR_ANAMODE);
		cr |= STM32_UCPD_CR_CCENABLE_MASK;
	}

	/* Update pull values */
	STM32_UCPD_CR(port) = cr;

	/* TODO (b/):Should this return error if cc+pull == Ra */
	return EC_SUCCESS;
}

int stm32gx_ucpd_set_polarity(int port, enum tcpc_cc_polarity polarity) {
	/*
	 * Polarity impacts the PHYCCSEL, CCENABLE, and CCxTCDIS fields. This
	 * function is called when polarity is updated at TCPM layer. STM32Gx
	 * only supports POLARITY_CC1 or POLARITY_CC2 and this is stored in the
	 * PHYCCSEL bit in the CR register.
	 */
	if (polarity > POLARITY_CC2)
		return EC_ERROR_UNIMPLEMENTED;

	if (polarity == POLARITY_CC1)
		STM32_UCPD_CR(port) &= ~STM32_UCPD_CR_PHYCCSEL;
	else if (polarity == POLARITY_CC2)
		STM32_UCPD_CR(port) |= STM32_UCPD_CR_PHYCCSEL;

	return EC_SUCCESS;
}

int stm32gx_ucpd_set_rx_enable(int port, int enable)
{
	/*
	 * USB PD receiver enable is controlled by the bit PHYRXEN in
	 * UCPD_CR. Enable Rx interrupts when RX PD decoder is active.
	 */
	if (enable) {
		/* int pol; */
		/* int mask; */

		STM32_UCPD_CR(port) |= STM32_UCPD_CR_PHYRXEN;
		STM32_UCPD_ICR(port) |= UCPD_IMR_RX_INT_MASK;
		STM32_UCPD_IMR(port) |= UCPD_IMR_RX_INT_MASK;
	} else {
		STM32_UCPD_CR(port) &= ~STM32_UCPD_CR_PHYRXEN;
		STM32_UCPD_IMR(port) &= ~UCPD_IMR_RX_INT_MASK;
	}

	return EC_SUCCESS;
}

int stm32gx_ucpd_set_msg_header(int port, int power_role, int data_role)
{
	msg_header.pr = power_role;
	msg_header.dr = data_role;

	return EC_SUCCESS;
}

int stm32gx_ucpd_transmit(int port,
			enum tcpm_transmit_type type,
			uint16_t header,
			const uint32_t *data)
{
	enum ucpd_tx_ordset orderset;

	/* Start message transmission */
	//gpio_set_level(GPIO_TRIGGER_2, 1);

	//mutex_lock(&ucpd_tx_mutex);
	/*
	 * First check if transmit_type is hard reset or cable reset. These
	 * messages are triggered via bits in CR and don't need to be written to
	 * TXDR to cause transmit. Must also check for TX_BIST_MODE here as
	 * well.
	 *
	 * The transmission type in ucpd is controlled via the
	 */
	switch (type) {
	case TCPC_TX_HARD_RESET:
		/*
		 * From RM0440 45.4.4:
		 * In order to facilitate generation of a Hard Reset, a special
		 * code of TXMODE field is used. No other fields need to be
		 * written. On writing the correct code, the hardware forces
		 * Hard Reset Tx under the correct (optimal) timings with
		 * respect to an on-going Tx message, which (if still in
		 * progress) is cleanly terminated by truncating the current
		 * sequence and directly appending an EOP K-code sequence. No
		 * specific interrupt is generated relating to this truncation
		 * event.
		 */
		/* Enable interrupt for Hard Reset sent/discarded */
		STM32_UCPD_IMR(port) |= STM32_UCPD_IMR_HRSTDISCIE |
			STM32_UCPD_IMR_HRSTSENTIE;
		/* Initiate Hard Reset */
		STM32_UCPD_CR(port) |= STM32_UCPD_CR_TXHRST;
		break;
	case TCPC_TX_CABLE_RESET:
		CPRINTS("ucpd: cable reset ctrl msg");
		break;
	case TCPC_TX_BIST_MODE_2:
		CPRINTS("ucpd: Bist ctrl msg");
		/* Clear TX mode */
		STM32_UCPD_CR(port) &= ~STM32_UCPD_CR_TXMODE_MASK;
		/* Select BIST mode */
		STM32_UCPD_CR(port) |= STM32_UCPD_CR_TXMODE_BIST;
		break;
	case TCPC_TX_SOP:
	case TCPC_TX_SOP_PRIME:
	case TCPC_TX_SOP_PRIME_PRIME:
	case TCPC_TX_SOP_DEBUG_PRIME:
	case TCPC_TX_SOP_DEBUG_PRIME_PRIME:
		/*
		 * These types are normal transmission, TXMODE = 0. To transmit
		 * regular message, control or data, requires the following:
		 *     1. Set TXMODE = 0
		 *     2. Set TX_ORDSETR based on message type
		 *     3. Set TX_PAYSZR which must account for 2 bytes of header
		 *     4. Configure DMA (optional if DMA is desired)
		 *     5. Enable transmit interrupts
		 *     6. Start TX by setting TXSEND in CR
		 */
		/* Clear TX mode */
		STM32_UCPD_CR(port) &= ~STM32_UCPD_CR_TXMODE_MASK;
		/* Index into ordset enum (skip 2 resets in debug type) */
		orderset = (type >= TCPC_TX_HARD_RESET) ? type += 2 : type;
		STM32_UCPD_TX_ORDSETR(port) = ucpd_txorderset[orderset];
		/*
		 * Set tx length parameter (in bytes). Note the count field in
		 * the header is number of 32 bit objects. Also, the length
		 * field must account for the 2 header bytes.
		 */
		STM32_UCPD_TX_PAYSZR(port) = (PD_HEADER_CNT(header) << 2) + 2;
		/* Configure DMA */
		/* TODO:  */

		/* Enable interrupts */
		STM32_UCPD_IMR(port) |= (STM32_UCPD_IMR_TXISIE |
					 STM32_UCPD_IMR_TXMSGDISCIE |
					 STM32_UCPD_IMR_TXMSGSENTIE |
					 STM32_UCPD_IMR_TXMSGABTIE);

		/* Copy data to ucpd data buffer */
		memcpy(ucpd_tx_data_buf, (uint8_t *)&header, 2);
		memcpy(ucpd_tx_data_buf + 2, (uint8_t *)data,
		       PD_HEADER_CNT(header) << 2);
		ucpd_tx_byte_count = 0;

		/* Write first data byte to TXDR */
		//ucpd_tx_data_byte(port);
		STM32_UCPD_CR(port) |= STM32_UCPD_CR_TXSEND;
		break;
	case TCPC_TX_INVALID:
		break;
	default:
		CPRINTS("ucpd: unknown message type %d", type);
		break;
	}

	//mutex_unlock(&ucpd_tx_mutex);

	if (tx_msg_log_cnt++ < TX_MSG_LOG_LEN) {
		int idx = tx_msg_log_idx++;

		tx_log[idx].ts = __hw_clock_source_read();
		tx_log[idx].tx_type = type;
		tx_log[idx].msg_type = PD_HEADER_TYPE(header);
		tx_log[idx].obj_len = (PD_HEADER_CNT(header) << 2) + 2;
		tx_log[idx].rev = PD_HEADER_REV(header);
	};

	return EC_SUCCESS;
}

int stm32gx_ucpd_get_message_raw(int port, uint32_t *payload, int *head)
{
	/* uint16_t *rx_header = (uint16_t *)ucpd_rx_buffer; */
	uint16_t *rx_header = (uint16_t *)ucpd_rxdr;
	int rxpaysz;
#ifdef CONFIG_USB_PD_DECODE_SOP
	int sop;
#endif

	/* First 2 bytes of data buffer are the header */
	*head = *rx_header;

#ifdef CONFIG_USB_PD_DECODE_SOP
/*
 * The message header is a 16-bit value that's stored in a 32-bit data type.
 * SOP* is encoded in bits 31 to 28 of the 32-bit data type.
 * NOTE: This is not part of the PD spec.
 */
	/* Get SOP value */
	sop = STM32_UCPD_RX_ORDSETR(port) & STM32_UCPD_RXORDSETR_MASK;
	/* Put SOP in bits 31:28 of 32 bit header */
	*head |= PD_HEADER_SOP(sop);
#endif
	msg_hdr = *head;

	rxpaysz = STM32_UCPD_RX_PAYSZR(port) & STM32_UCPD_RX_PAYSZR_MASK;
	/* This size includes 2 bytes for message header */
	rxpaysz -= 2;
	/* Copy payload (src/dst are both 32 bit aligned) */
	memcpy(payload, ucpd_rxdr + 2, rxpaysz);

	/* Do I need to clear any UCDP status bits here?? */

	return EC_SUCCESS;
}

static void ucpd_dump_tx_log(void)
{
	int i;

	ccprintf("ucpd: tx_msg_total = %d\n", tx_msg_log_cnt);
	for (i = 0; i < tx_msg_log_idx; i++)
		ccprintf("msg[%02d]: %08d\t%d\t%d\t%d\t%d\n", i,
			tx_log[i].ts,
		        tx_log[i].tx_type, tx_log[i].msg_type,
			tx_log[i].obj_len, tx_log[i].rev);

	tx_msg_log_cnt = 0;
	tx_msg_log_idx = 0;
}

static void ucpd_dump_rx_log(void)
{
	int i;

	ccprintf("ucpd: rx_msg_total = %d\n", rx_msg_log_cnt);
	for (i = 0; i < rx_msg_log_idx; i++)
		ccprintf("msg[%02d]: %08d\t%d\t%d\t%d\t%x\n",
			 i,
			 rx_log[i].ts,
			 rx_log[i].msg_type,
			 rx_log[i].cnt,
			 rx_log[i].rev,
			 rx_log[i].header);

	rx_msg_log_cnt = 0;
	rx_msg_log_idx = 0;
}

static int command_ucpd(int argc, char **argv)
{
	uint8_t tx_buffer[8];
	char *e;
	int val;
	int port = 0;

	memset(tx_buffer, 0, 8);
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "rst")) {
		stm32gx_ucpd_init(port);
	} else if (!strcasecmp(argv[1], "src")) {

	} else if (!strcasecmp(argv[1], "bist")) {
		stm32gx_ucpd_transmit(port, TCPC_TX_BIST_MODE_2, 0,
				      (uint32_t *)tx_buffer);
	} else if (!strcasecmp(argv[1], "hard")) {
		stm32gx_ucpd_transmit(port, TCPC_TX_HARD_RESET, 0,
				      (uint32_t *)tx_buffer);
	} else if (!strcasecmp(argv[1], "pol")) {
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		val = strtoi(argv[2], &e, 10);
		if (val > 1)
			val = 0;
		stm32gx_ucpd_set_polarity(port, val);
		stm32gx_ucpd_set_rx_enable(port, 1);
		ccprintf("ucpd: set pol = %d, PHYRXEN = 1\n", val);
	} else if (!strcasecmp(argv[1], "tx_log")) {
		ucpd_dump_tx_log();
	} else if (!strcasecmp(argv[1], "rx_log")) {
		ucpd_dump_rx_log();
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ucpd, command_ucpd,
			"[rst|src|bist|amber",
			"Turn on/off LED.");
