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
 * designed to work in freq ranges of 6 <--> 18 MHz, however recommended
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

/* USB PD message buffer length */
#define UCPD_BUF_LEN 64

#define UCPD_IMR_RX_INT_MASK (STM32_UCPD_IMR_RXNEIE| \
			      STM32_UCPD_IMR_RXORDDETIE | \
			      STM32_UCPD_IMR_RXHRSTDETIE |	\
			      STM32_UCPD_IMR_RXOVRIE |		\
			      STM32_UCPD_IMR_RXMSGENDIE)

#define UCPD_IMR_TX_INT_MASK (STM32_UCPD_IMR_TXISIE | \
				  STM32_UCPD_IMR_TXMSGDISCIE | \
				  STM32_UCPD_IMR_TXMSGSENTIE | \
				  STM32_UCPD_IMR_TXMSGABTIE)

#define UCPD_ANASUB_TO_RP(r) ((r - 1) & 0x3)
#define UCPD_RP_TO_ANASUB(r) ((r + 1) & 0x3)

#define TCPM_TX_MASK BIT(SRC_TCPM)
#define UCPD_TX_MASK BIT(SRC_UCPD)

struct msg_header_info {
	uint8_t pr;
	uint8_t dr;
};
static struct msg_header_info msg_header;

/* Events for pd_interrupt_handler_task */
#define UCPD_GOOD_CRC_PENDING   BIT(0)
#define UCPD_TCPM_MSG_PENDING   BIT(1)
#define UCPD_TX_COMPLETE        BIT(2)
#define UCPD_RX_GOOD_CRC        BIT(3)
#define UCPD_TX_MSG_SUCCESS     BIT(4)

enum ucpd_tx_src {
	SRC_UCPD,
	SRC_TCPM,
	SRC_TOTAL
};

struct ucpd_msg {
	enum tcpm_transmit_type type;
	int len;
	uint8_t buf[UCPD_BUF_LEN];
};

static struct ucpd_msg ucpd_tx_buffers[SRC_TOTAL];
static int ucpd_tx_byte_count;
static uint8_t *ucpd_tx_data_buf;

static int ucpd_rx_byte_count;
static uint8_t ucpd_rx_buffer[UCPD_BUF_LEN];

static int ucpd_txorderset[] = {
	TX_ORDERSET_SOP,
	TX_ORDERSET_SOP1,
	TX_ORDERSET_SOP2,
	TX_ORDERSET_HARD_RESET,
	TX_ORDERSET_CABLE_RESET,
	TX_ORDERSET_SOP1_DEBUG,
	TX_ORDERSET_SOP2_DEBUG,
};

#ifdef CONFIG_STM32G4_UCPD_DEBUG
/* Defines and macros used for ucpd pd message logging */
#define MSG_LOG_LEN 64
#define MSG_BUF_LEN 8

struct msg_info {
	uint8_t dir;
	uint8_t comp;
	uint8_t crc;
	uint16_t header;
	uint32_t ts;
	uint8_t buf[MSG_BUF_LEN];
};
static int msg_log_cnt;
static int msg_log_idx;
static struct msg_info msg_log[MSG_LOG_LEN];

#define UCPD_CC_STRING_LEN 5

static char ccx[4][UCPD_CC_STRING_LEN] = {
	"Ra",
	"Rp",
	"Rd",
	"Open",
};
static char rp_string[][8] = {
	"Rp_usb",
	"Rp_1.5",
	"Rp_3.0",
	"Open",
};
static int ucpd_sr_cc_event;
static int ucpd_cc_set_save;
static int ucpd_cc_change_log;

static void ucpd_cc_status(int port)
{
	int rc = stm32gx_ucpd_get_role_control(port);
	int cc1_pull, cc2_pull;
	enum tcpc_cc_voltage_status v_cc1, v_cc2;
	int rv;
	char *rp_name;

	cc1_pull = ucpd_is_cc_pull_active(port, 0) ? rc & 0x3 : 3;
	cc2_pull = ucpd_is_cc_pull_active(port, 1) ? (rc >> 2) & 0x3 : 3;

	rv = stm32gx_ucpd_get_cc(port,&v_cc1, &v_cc2);
	rp_name = rp_string[(rc >> 4) % 0x3];
	ccprintf("\tcc1\t = %s\n\tcc2\t = %s\n\tRp\t = %s\n",
		 ccx[cc1_pull], ccx[cc2_pull], rp_name);
	if (!rv)
		ccprintf("\tcc1_v\t = %d\n\tcc2_v\t = %d\n", v_cc1, v_cc2);
}

void ucpd_cc_detect_notify_enable(int enable)
{
	ucpd_cc_change_log = enable;
}

static void ucpd_log_invalidate_entry(void)
{
	if (msg_log_idx < (MSG_LOG_LEN - 1)) {
		int idx = msg_log_idx;

		msg_log[idx].header = 0xabcd;
		msg_log[idx].ts = __hw_clock_source_read();
		msg_log[idx].dir = 0;
		msg_log[idx].comp = 0;
		msg_log[idx].crc = 0;
		msg_log_cnt++;
		msg_log_idx++;
	}
}
static void ucpd_cc_change_notify(void)
{
	if (ucpd_cc_change_log) {
		board_debug_gpio(TRIGGER_2, 1);
		ucpd_log_invalidate_entry();

		ccprintf("vstate: cc1 = %x, cc2 = %x, Rp = %d\n",
			 (ucpd_sr_cc_event >> STM32_UCPD_SR_VSTATE_CC1_SHIFT) & 0x3,
			 (ucpd_sr_cc_event >> STM32_UCPD_SR_VSTATE_CC2_SHIFT) & 0x3,
			 (ucpd_cc_set_save >> STM32_UCPD_CR_ANASUBMODE_SHIFT) & 0x3);
		ucpd_cc_status(0);
		board_debug_gpio(TRIGGER_2, 0);
	}
}
DECLARE_DEFERRED(ucpd_cc_change_notify);

static void ucpd_log_add_msg(uint16_t header, int dir)
{
	uint32_t ts = __hw_clock_source_read();
	int idx = msg_log_idx;
	uint8_t *buf = dir ? ucpd_rx_buffer : ucpd_tx_data_buf;

	if (msg_log_cnt++ < MSG_LOG_LEN) {
		int byte_len;

		msg_log[idx].header = header;
		msg_log[idx].ts = ts;
		msg_log[idx].dir = dir;
		msg_log[idx].comp = 0;
		msg_log[idx].crc = 0;
		msg_log_idx++;
		byte_len = (PD_HEADER_CNT(header) << 2) + 2;
		if (byte_len > MSG_BUF_LEN)
			byte_len = MSG_BUF_LEN;
		memcpy(msg_log[idx].buf, buf, byte_len);
	}
}

static void ucpd_log_mark_tx_comp(void)
{
	/* back up 1 to mark complete finished */
	if (msg_log_cnt < MSG_LOG_LEN) {
		if (msg_log_idx > 0)
			msg_log[msg_log_idx -1].comp = 1;
	}
}

static void ucpd_log_mark_crc(void)
{
	/* back up 1 to mark complete finished */
	if (msg_log_cnt < MSG_LOG_LEN) {
		if (msg_log_idx >= 2)
			msg_log[msg_log_idx -2].crc = 1;
	}
}
#endif

static int ucpd_is_cc_pull_active(int port, int cc_line)
{
	int cc_enable = (STM32_UCPD_CR(port) & STM32_UCPD_CR_CCENABLE_MASK) >>
		STM32_UCPD_CR_CCENABLE_SHIFT;

	return ((cc_enable >> cc_line) & 0x1);
}

static int ucpd_msg_is_good_crc(uint16_t header)
{
	/*
	 * Good CRC is a control message (no data objects) with GOOD_CRC message
	 * type in the header.
	 */
	return ((PD_HEADER_CNT(header) == 0) && (PD_HEADER_TYPE(header) ==
						 PD_CTRL_GOOD_CRC)) ? 1 : 0;
}

static void ucpd_hard_reset_rx_log(void)
{
	CPRINTS("ucpd: hard reset recieved");
}
DECLARE_DEFERRED(ucpd_hard_reset_rx_log);

static void ucpd_port_enable(int port, int enable)
{
	if (enable)
		STM32_UCPD_CFGR1(port) |= STM32_UCPD_CFGR1_UCPDEN;
	else
		STM32_UCPD_CFGR1(port) &= ~STM32_UCPD_CFGR1_UCPDEN;
}

static void ucpd_tx_data_byte(int port)
{
	STM32_UCPD_TXDR(port) =  ucpd_tx_data_buf[ucpd_tx_byte_count++];
}

static void ucpd_rx_data_byte(int port)
{
	if (ucpd_rx_byte_count < UCPD_BUF_LEN)
		ucpd_rx_buffer[ucpd_rx_byte_count++] = STM32_UCPD_RXDR(port);
}

static void ucpd_tx_interrupts_enable(int port, int enable)
{
	if (enable)
		STM32_UCPD_IMR(port) |= UCPD_IMR_TX_INT_MASK;
	else
		STM32_UCPD_IMR(port) &= ~UCPD_IMR_TX_INT_MASK;
}

static void ucpd_rx_enque_error(void)
{
	CPRINTS("ucpd: TCPM Enque Error!!");
}
DECLARE_DEFERRED(ucpd_rx_enque_error);

int stm32gx_ucpd_init(int port)
{
	uint32_t cfgr1_reg;
	uint32_t moder_reg;

	/*
	* After exiting reset, stm32gx will have dead battery mode enabled by
	* default which connects Rd to CC1/CC2. This should be disabled when EC
	* is powered up.
	*/
	STM32_PWR_CR3 |= STM32_PWR_CR3_UCPD1_DBDIS;

	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;

	/* Make sure CC1/CC2 pins PB4/PB6 are set for analog mode */
	moder_reg = STM32_GPIO_MODER(GPIO_B);
	moder_reg |= 0x3300;
	STM32_GPIO_MODER(GPIO_B) = moder_reg;
	/*
	 * CFGR1 must be written when UCPD peripheral is disabled. Note that
	 * disabling ucpd causes the peripheral to quit any ongoing activity and
	 * sets all ucpd registers back their default values.
	 */
	ucpd_port_enable(port, 0);

	cfgr1_reg = STM32_UCPD_CFGR1_PSC_CLK_VAL(UCPD_PSC_DIV - 1) |
		STM32_UCPD_CFGR1_TRANSWIN_VAL(UCPD_TRANSWIN_CNT - 1) |
		STM32_UCPD_CFGR1_IFRGAP_VAL(UCPD_IFRGAP_CNT - 1) |
		STM32_UCPD_CFGR1_HBITCLKD_VAL(UCPD_HBIT_DIV - 1);
	STM32_UCPD_CFGR1(port) = cfgr1_reg;

	/*
	 * Set RXORDSETEN field to control which types of ordered sets the PD
	 * receiver must receive.
	 * SOP, SOP', Hard Reset Det, Cable Reset Det enabled
	 */
	STM32_UCPD_CFGR1(port) |= STM32_UCPD_CFGR1_RXORDSETEN_VAL(0x1B);

	/* Enable ucpd  */
	ucpd_port_enable(port, 1);

	/* Configure CC change interrupts */
	STM32_UCPD_IMR(port) = STM32_UCPD_IMR_TYPECEVT1IE |
		STM32_UCPD_IMR_TYPECEVT2IE;
	STM32_UCPD_ICR(port) = STM32_UCPD_ICR_TYPECEVT1CF |
		STM32_UCPD_ICR_TYPECEVT2CF;

	/* Enable UCPD interrupts */
	task_enable_irq(STM32_IRQ_UCPD1);

	return EC_SUCCESS;
}

int stm32gx_ucpd_release(int port)
{
	ucpd_port_enable(port, 0);

	return EC_SUCCESS;
}

int stm32gx_ucpd_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	int vstate_cc1;
	int vstate_cc2;
	int anamode;
	uint32_t sr;

	/*
	 * cc_voltage_status is determined from vstate_cc bit field in the
	 * status register. The meaning of the value vstate_cc depends on
	 * current value of ANAMODE (src/snk).
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
	int role_control;
	int cc1;
	int cc2;
	int anamode = !!(STM32_UCPD_CR(port) & STM32_UCPD_CR_ANAMODE);
	int anasubmode = (STM32_UCPD_CR(port) & STM32_UCPD_CR_ANASUBMODE_MASK)
		>> STM32_UCPD_CR_ANASUBMODE_SHIFT;

	/*
	 * Role control register is defined as:
	 *     R_cc1 -> b 1:0
	 *     R_cc2 -> b 3:2
	 *     Rp    -> b 5:4
	 *
	 * In TCPCI, CCx is defined as:
	 *    00b -> Ra
	 *    01b -> Rp
	 *    10b -> Rd
	 *    11b -> Open (don't care)
	 *
	 * For ucpd, this information is encoded in ANAMODE and ANASUBMODE
	 * fields as follows:
	 *   ANAMODE            CCx
	 *     0   ->    Rp   -> 1
	 *     1   ->    Rd   -> 2
	 *
	 *   ANASUBMODE:
	 *     00b -> TYPEC_RP_RESERVED (open)
	 *     01b -> TYPEC_RP_USB
	 *     10b -> TYPEC_RP_1A5
	 *     11b -> TYPEC_RP_3A0
	 *
	 *   CCx = ANAMODE + 1, if CCx is enabled
	 *   Rp  = (ANASUBMODE - 1) & 0x3
	 */
	cc1 = ucpd_is_cc_pull_active(port, USBPD_CC_PIN_1) ? anamode + 1 :
		TYPEC_CC_OPEN;
	cc2 = ucpd_is_cc_pull_active(port, USBPD_CC_PIN_2) ? anamode + 1 :
		TYPEC_CC_OPEN;
	role_control = cc1 | (cc2 << 2);
	/* Circular shift anasubmode to convert to Rp range */
	role_control |= (UCPD_ANASUB_TO_RP(anasubmode) << 4);

	return role_control;
}

int stm32gx_ucpd_vconn_disc_rp(int port, int enable)
{
	int cr = STM32_UCPD_CR(port);
	int pol;
	int cc_disable_mask;

	/*
	 * This function is called when tcpm_set_vconn() method is called to
	 * enable VCONN. ucpd does not provide vconn, but Rp must be
	 * disconnected from the CCx line prior to enabling vconn.
	 */
	if (enable) {
		/* Get CC polarity */
		pol = !!(cr & STM32_UCPD_CR_PHYCCSEL);
		/* Disconnect cc line that is not being used for PD messaging */
		cc_disable_mask = 1 << (STM32_UCPD_CR_CCENABLE_SHIFT + !pol);
		cr &= ~cc_disable_mask;
		CPRINTS("ucpd: vconn disable Rp, pol = %d, cr = %x", pol, cr);
	} else {
		/* make sure Rp/Rd is connected */
		cr |= STM32_UCPD_CR_CCENABLE_MASK;
	}
	/* Apply cc pull resistor change */
	STM32_UCPD_CR(port) = cr;

	return EC_SUCCESS;
}

int stm32gx_ucpd_set_cc(int port, int cc_pull, int rp)
{
	uint32_t cr = STM32_UCPD_CR(port);

	/*
	 * Always set ANASUBMODE to match desired Rp. TCPM layer has a valid
	 * range of 0, 1, or 2. This range maps to 1, 2, or 3 in ucpd for
	 * ANASUBMODE.
	 */
	cr &= ~STM32_UCPD_CR_ANASUBMODE_MASK;
	cr |= STM32_UCPD_CR_ANASUBMODE_VAL(UCPD_RP_TO_ANASUB(rp));

	/* Disconnect both pull from both CC lines by default */
	cr &= ~STM32_UCPD_CR_CCENABLE_MASK;
	/* Set ANAMODE if cc_pull is Rd */
	if (cc_pull == TYPEC_CC_RD) {
		cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	/* Clear ANAMODE if cc_pull is Rp */
	} else if (cc_pull == TYPEC_CC_RP) {
		cr &= ~(STM32_UCPD_CR_ANAMODE);
		cr |= STM32_UCPD_CR_CCENABLE_MASK;
	}

#ifdef CONFIG_STM32G4_UCPD_DEBUG
	if (ucpd_cc_change_log) {
		CPRINTS("ucpd: set_cc: pull = %d, rp = %d", cc_pull, rp);
	}
#endif
	/* Update pull values */
	STM32_UCPD_CR(port) = cr;

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

#ifdef CONFIG_STM32G4_UCPD_DEBUG
	ucpd_cc_set_save = STM32_UCPD_CR(port);
#endif

	return EC_SUCCESS;
}

int stm32gx_ucpd_set_rx_enable(int port, int enable)
{
	/*
	 * USB PD receiver enable is controlled by the bit PHYRXEN in
	 * UCPD_CR. Enable Rx interrupts when RX PD decoder is active.
	 */
	if (enable) {
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

static int stm32gx_ucpd_start_transmit(int port, int src)
{
	enum ucpd_tx_ordset orderset;
	enum tcpm_transmit_type type;
#ifdef CONFIG_STM32G4_UCPD_DEBUG
	uint16_t *header = (uint16_t *)ucpd_tx_data_buf;
#endif

	/* Start message transmission */

	/* set up tx data pointer */
	ucpd_tx_data_buf = ucpd_tx_buffers[src].buf;
	ucpd_tx_byte_count = 0;
	type = ucpd_tx_buffers[src].type;
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
		/* TODO(b/): Add support to send cable reset */
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
		STM32_UCPD_TX_PAYSZR(port) = ucpd_tx_buffers[src].len;

		/* Enable interrupts */
		ucpd_tx_interrupts_enable(port, 1);

		/* Trigger ucpd peripheral to start pd message transmit */
		STM32_UCPD_CR(port) |= STM32_UCPD_CR_TXSEND;
		break;
	case TCPC_TX_INVALID:
		break;
	default:
		CPRINTS("ucpd: unknown message type %d", type);
		break;
	}

#ifdef CONFIG_STM32G4_UCPD_DEBUG
	if (type != TCPC_TX_HARD_RESET)
		ucpd_log_add_msg(*header, 0);
#endif

	return EC_SUCCESS;
}

/*
 * Main task entry point for UCPD task
 *
 * @param p The PD port number for which to handle interrupts (pointer is
 * reinterpreted as an integer directly).
 */
void ucpd_task(void *p)
{
	const int port = (int) ((intptr_t) p);
	static int ucpd_tx_active = 0;
	static int ucpd_tx_pending = 0;
	static int ucpd_tx_wait_good_crc = 0;

	while (1) {
		const int evt = task_wait_event(-1);

		/*
		 * USB-PD messages are intiated in TCPM stack (PRL
		 * layer). However, GoodCRC messages are initiated within the
		 * UCPD driver based on USB-PD rx messages. These 2 types of
		 * transmit paths are managed via task events.
		 *
		 * UCPD generated GoodCRC messages, are the priority path as
		 * they must be sent immediately following a successful USB-PD
		 * rx message. As long as a transmit operation is not underway,
		 * then a transmit message will be started upon request. The ISR
		 * routine sets the event to indicate that the transmit
		 * operation is complete.
		 */

		if (evt & UCPD_GOOD_CRC_PENDING)
			ucpd_tx_pending |= UCPD_TX_MASK;

		if (evt & UCPD_TCPM_MSG_PENDING)
			ucpd_tx_pending |= TCPM_TX_MASK;

		if (evt & UCPD_TX_COMPLETE)
			ucpd_tx_active = 0;

		if (evt & UCPD_TX_MSG_SUCCESS)
			ucpd_tx_wait_good_crc = 1;

		if ((evt & UCPD_RX_GOOD_CRC) && ucpd_tx_wait_good_crc) {
			pd_transmit_complete(port, TCPC_TX_COMPLETE_SUCCESS);
			ucpd_tx_wait_good_crc = 0;
		}

		if (!ucpd_tx_active && ucpd_tx_pending) {
			enum ucpd_tx_src type;

			/* Extract which transmit path is being used */
			type = ucpd_tx_pending & UCPD_TX_MASK ?
				SRC_UCPD : SRC_TCPM;
			ucpd_tx_wait_good_crc = 0;
			/* Initiate the USB-PD message transmit */
			stm32gx_ucpd_start_transmit(port, type);
			/* Prevent another USB-PD message from starting */
			ucpd_tx_active = 1;
			/* This transmit path is now clear */
			ucpd_tx_pending &= ~(1 << type);
		}
	}
}

static void ucpd_send_good_crc(int port, uint16_t rx_header)
{
	int msg_id;
	int rev_id;
	uint16_t tx_header;
	uint8_t *buf = ucpd_tx_buffers[SRC_UCPD].buf;

	/*
	 * A GoodCRC message shall be sent by receiver to ack that the previous
	 * message was correctly received. The GoodCRC message shall return the
	 * rx message's msg_id field. The one exception is for GoodCRC messages,
	 * which do not generate a GoodCRC response
	 */
	if (ucpd_msg_is_good_crc(rx_header)) {
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

	ucpd_tx_buffers[SRC_UCPD].len = 2;
	ucpd_tx_buffers[SRC_UCPD].type = TCPC_TX_SOP;
	/* Copy data to ucpd data buffer */
	memcpy(buf, (uint8_t *)&tx_header, 2);

	/* Notify ucpd task that a GoodCRC message tx request is pending */
	task_set_event(TASK_ID_UCPD, UCPD_GOOD_CRC_PENDING, 0);
}

int stm32gx_ucpd_transmit(int port,
			  enum tcpm_transmit_type type,
			  uint16_t header,
			  const uint32_t *data)
{
	uint8_t *buf = ucpd_tx_buffers[SRC_TCPM].buf;

	/* Length in bytes 4 * object len + 2 header byes */
	ucpd_tx_buffers[SRC_TCPM].len = (PD_HEADER_CNT(header) << 2) + 2;
	ucpd_tx_buffers[SRC_TCPM].type = type;
	/* Copy data to ucpd data buffer */
	memcpy(buf, (uint8_t *)&header, 2);
	memcpy(buf + 2, (uint8_t *)data,
	       PD_HEADER_CNT(header) << 2);

	/* Notify ucpd task that a TCPM message tx request is pending */
	task_set_event(TASK_ID_UCPD, UCPD_TCPM_MSG_PENDING, 0);

	return EC_SUCCESS;
}

int stm32gx_ucpd_get_message_raw(int port, uint32_t *payload, int *head)
{
	uint16_t *rx_header = (uint16_t *)ucpd_rx_buffer;
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
	rxpaysz = STM32_UCPD_RX_PAYSZR(port) & STM32_UCPD_RX_PAYSZR_MASK;
	/* This size includes 2 bytes for message header */
	rxpaysz -= 2;
	/* Copy payload (src/dst are both 32 bit aligned) */
	memcpy(payload, ucpd_rx_buffer + 2, rxpaysz);

	return EC_SUCCESS;
}

void stm32gx_ucpd1_irq(void)
{
	/* STM32_IRQ_UCPD indicates this is from UCPD1, so port = 0 */
	int port = 0;
	uint32_t sr = STM32_UCPD_SR(port);
	uint32_t tx_done_mask = STM32_UCPD_SR_TXMSGSENT | STM32_UCPD_SR_TXMSGABT
		| STM32_UCPD_SR_TXMSGDISC | STM32_UCPD_SR_HRSTSENT |
		STM32_UCPD_SR_HRSTDISC;

	/* Check for CC events, set event to wake PD task */
	if (sr & (STM32_UCPD_SR_TYPECEVT1 | STM32_UCPD_SR_TYPECEVT2)) {
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
#ifdef CONFIG_STM32G4_UCPD_DEBUG
		ucpd_sr_cc_event = sr;
		hook_call_deferred(&ucpd_cc_change_notify_data, 0);
#endif
	}

	/*
	 * Check for Tx events. tx_mask includes all status bits related to the
	 * end of a USB-PD tx message. If any of these bits are set, the
	 * transmit attempt is completed. Set an event to notify ucpd tx state
	 * machine that transmit operation is complete.
	 */
	if (sr & tx_done_mask) {
		/* Transmit is complete */
		task_set_event(TASK_ID_UCPD, UCPD_TX_COMPLETE, 0);
		/* Disable Tx interrupts */
		ucpd_tx_interrupts_enable(port, 1);
	}

	/* Check for data register empty */
	if (sr & STM32_UCPD_SR_TXIS)
		ucpd_tx_data_byte(port);

	/* Check for tx message complete */
	if (sr & STM32_UCPD_SR_TXMSGSENT) {
		task_set_event(TASK_ID_UCPD, UCPD_TX_MSG_SUCCESS, 0);
#ifdef CONFIG_STM32G4_UCPD_DEBUG
		ucpd_log_mark_tx_comp();
#endif
	}

	/* USB-PD message tx failed, notify TCPM layer */
	if (sr & (STM32_UCPD_SR_TXMSGABT | STM32_UCPD_SR_TXMSGDISC)) {
		pd_transmit_complete(port, TCPC_TX_COMPLETE_FAILED);
	}

	/* Check for Rx Events */
	/* Check first for start of new message */
	if (sr & STM32_UCPD_SR_RXORDDET) {
		ucpd_rx_byte_count = 0;
	}
	/* Check for byte received */
	if (sr & STM32_UCPD_SR_RXNE)
		ucpd_rx_data_byte(port);

	/* Check for end of message */
	if (sr & STM32_UCPD_SR_RXMSGEND) {
		/* Check for errors */
		if (!(sr & STM32_UCPD_SR_RXERR)) {
			int rv;
			uint16_t *rx_header = (uint16_t *)ucpd_rx_buffer;

#ifdef CONFIG_STM32G4_UCPD_DEBUG
			ucpd_log_add_msg(*rx_header, 1);
#endif
			/* Don't pass GoodCRC control messages TCPM */
			if (!ucpd_msg_is_good_crc(*rx_header)) {
				/* TODO - Add error checking here */
				rv = tcpm_enqueue_message(port);
				if (rv)
					hook_call_deferred(&ucpd_rx_enque_error_data,
							   0);
				/* Send GoodCRC message (if required) */
				ucpd_send_good_crc(port, *rx_header);
			} else {
				/*
				 * GoodCRC message received. Notify tcpm layer
				 * that transmit is complete.
				 */
				task_set_event(TASK_ID_UCPD, UCPD_RX_GOOD_CRC,
					       0);
#ifdef CONFIG_STM32G4_UCPD_DEBUG
				ucpd_log_mark_crc();
#endif
			}
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


#ifdef CONFIG_STM32G4_UCPD_DEBUG
static char ctrl_names[][10] = {
	"rsvd",
	"GoodCRC",
	"Goto Min",
	"Accept",
	"Reject",
	"Ping",
	"PS_Rdy",
	"Get_SRC",
	"Get_SNK",
	"DR_Swap",
	"PR_Swap",
	"VCONN_Swp",
	"Wait",
	"Soft_Rst"
};

static char data_names[][10] = {
	"RSVD",
	"SRC_CAP",
	"REQUEST",
	"BIST",
	"SINK_CAP",
	"BATTERY",
        "ALERT",
	"GET_INFO",
	"ENTER_USB",
	"RSVD",
	"RSVD",
	"RSVD",
	"RSVD",
	"RSVD",
	"RSVD",
	"VDM",
};

static void ucpd_dump_msg_log(void)
{
	int i;
	int type;
	int len;
	int dir;
	uint16_t header;
	char *name;


	ccprintf("ucpd: msg_total = %d\n", msg_log_cnt);
	ccprintf("Idx\t  Delta(us)\tDir\t   Type\t\tLen\t s1  s2   PR\t DR\n");
	ccprintf("----------------------------------------------------------------------------\n");

	for (i = 0; i < msg_log_idx; i++) {
		uint32_t delta_ts = 0;
		int j;

		header = msg_log[i].header;

		if (header != 0xabcd) {
			type = PD_HEADER_TYPE(header);
			len = PD_HEADER_CNT(header);
			name = len ? data_names[type] : ctrl_names[type];
			dir = msg_log[i].dir;
			if (i) {
				delta_ts = msg_log[i].ts - msg_log[i-1].ts;
			}

			ccprintf("msg[%02d]: %08d\t %s\t %8s\t %02d\t %d  %d\t %s\t %s",
				 i,
				 delta_ts,
				 dir ? "Rx" : "Tx",
				 name,
				 len,
				 msg_log[i].comp,
				 msg_log[i].crc,
				 PD_HEADER_PROLE(header) ? "SRC" : "SNK",
				 PD_HEADER_DROLE(header) ? "DFP" : "UFP");
			len = MIN((len * 4) + 2, MSG_BUF_LEN);
			for (j = 0; j < len; j++)
				ccprintf(" %02x", msg_log[i].buf[j]);
		} else {
			if (i) {
				delta_ts = msg_log[i].ts - msg_log[i-1].ts;
			}
			ccprintf("msg[%02d]: %08d\t CC Voltage Change!",
				 i, delta_ts);
		}
		ccprintf("\n");
		msleep(5);
	}
}

static void stm32gx_ucpd_set_cc_debug(int port, int cc_mask, int pull, int rp)
{
	int cc_enable;
	uint32_t cr = STM32_UCPD_CR(port);

	/*
	 * Only update ANASUBMODE if specified pull type is Rp.
	 */
	if (pull == TYPEC_CC_RP) {
		cr &= ~STM32_UCPD_CR_ANASUBMODE_MASK;
		cr |= STM32_UCPD_CR_ANASUBMODE_VAL(UCPD_RP_TO_ANASUB(rp));
	}

	/*
	 * Can't independently set pull value for CC1 from CC2. But, can
	 * independently connect or disconnect pull for CC1 and CC2. Enable here
	 * the CC lines specified by cc_mask. If desired pull is TYPEC_CC_OPEN,
	 * then this CC lines specified in cc_mask will be disabled.
	 */
	/* Get exsiting cc enable value */
	cc_enable = (cr & STM32_UCPD_CR_CCENABLE_MASK) >>
		STM32_UCPD_CR_CCENABLE_SHIFT;
	/* Apply cc_mask (enable CC line specified) */
	cc_enable |= cc_mask;

	/* Set ANAMODE if cc_pull is Rd */
	if (pull == TYPEC_CC_RD)
		cr |= STM32_UCPD_CR_ANAMODE;
	/* Clear ANAMODE if cc_pull is Rp */
	else if (pull == TYPEC_CC_RP)
		cr &= ~(STM32_UCPD_CR_ANAMODE);
	else if (pull == TYPEC_CC_OPEN)
		cc_enable &= ~cc_mask;

	/* The value for this field needs to be OR'd in */
	cr &= ~STM32_UCPD_CR_CCENABLE_MASK;
	cr |= STM32_UCPD_CR_CCENABLE_VAL(cc_enable);
	/* Update pull values */
	STM32_UCPD_CR(port) = cr;
	/* Display updated settings */
	ucpd_cc_status(port);
}

void ucpd_info(int port)
{
	ucpd_cc_status(port);
	ccprintf("\trx_en\t = %d\n\tpol\t = %d\n",
		 !!(STM32_UCPD_CR(port) & STM32_UCPD_CR_PHYRXEN),
		 !!(STM32_UCPD_CR(port) & STM32_UCPD_CR_PHYCCSEL));
}

static int command_ucpd(int argc, char **argv)
{
	uint8_t tx_buffer[6] = {0x61, 0x1, 0x2C, 0x91, 0x01, 0x26};
	char *e;
	int val;
	int port = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "rst")) {
		stm32gx_ucpd_init(port);
	} else if (!strcasecmp(argv[1], "info")) {
		ucpd_info(port);
	} else if (!strcasecmp(argv[1], "tc")) {
		if (!strcasecmp(argv[2], "on"))
			ucpd_cc_change_log = 1;
		else
			ucpd_cc_change_log = 0;
		ccprintf("ucpd: vstate_cc change log = %d\n",
			 ucpd_cc_change_log);
	} else if (!strcasecmp(argv[1], "src")) {
		uint16_t header;

		/*
		 * Send SRC CAP message: (5V/3A fixed msg id = 0)
		 *   type = PD_DATA_SOURCE_CAP
		 *   pr   = source
		 *   dr   = DFP
		 *   id   = 0
		 *   cnt  = 1
		 *   ext  = 0
		 */
		header = PD_HEADER(PD_DATA_SOURCE_CAP, PD_ROLE_SOURCE,
				   PD_ROLE_DFP, 0, 1, 1, 0);
		stm32gx_ucpd_transmit(port, TCPC_TX_SOP, header,
				      (uint32_t *)tx_buffer);
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
	} else if (!strcasecmp(argv[1], "cc")) {
		int cc_mask;
		int pull;
		int rp = 0; /* needs to be initialized */

		if (argc < 3) {
			ucpd_cc_status(port);
			return EC_SUCCESS;
		}
		cc_mask = strtoi(argv[2], &e, 10);
		if (cc_mask < 1 || cc_mask > 3)
			return EC_ERROR_PARAM2;
		/* cc_mask has determines which cc setting to apply */
		if (!strcasecmp(argv[3], "rd")) {
			pull = TYPEC_CC_RD;
		} else if (!strcasecmp(argv[3], "rp")) {
			pull = TYPEC_CC_RP;
			rp = strtoi(argv[4], &e, 10);
			if (rp < 0 || rp > 2)
				return EC_ERROR_PARAM4;
		} else if (!strcasecmp(argv[3], "open")) {
			pull = TYPEC_CC_OPEN;
		} else {
			return EC_ERROR_PARAM3;
		}
		stm32gx_ucpd_set_cc_debug(port, cc_mask, pull, rp);

	} else if (!strcasecmp(argv[1], "log")) {
		if (argc < 3) {
			ucpd_dump_msg_log();
		} else if (!strcasecmp(argv[2], "clr")) {
			msg_log_cnt = 0;
			msg_log_idx = 0;
		}
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ucpd, command_ucpd,
			"[rst|src|bist|amber",
			"Turn on/off LED.");
#endif
