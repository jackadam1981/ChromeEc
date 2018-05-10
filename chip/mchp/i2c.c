/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C port module for MCHP MEC
 * TODO handle chip variants
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "tfdp_chip.h"

#if 1
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#else
#define CPUTS(outstr)
#define CPRINTS(format, args...)
#endif

/*
 * MCHP I2C BAUD clock source is 16 MHz.
 */
#define I2C_CLOCK 16000000 /* 16 MHz */
#define I2C_CLOCK_KBPS 16000

/* SMBus Timing values for 1MHz Speed */
#define SPEED_1MHZ_BUS_CLOCK			0x0509ul
#define SPEED_1MHZ_DATA_TIMING			0x06060601ul
#define SPEED_1MHZ_DATA_TIMING_2		0x06ul
#define SPEED_1MHZ_IDLE_SCALING			0x01000050ul
#define SPEED_1MHZ_TIMEOUT_SCALING		0x149CC2C7ul

/* SMBus Timing values for 400kHz speed */
#define SPEED_400KHZ_BUS_CLOCK			0x0F17ul
#define SPEED_400KHZ_DATA_TIMING		0x040A0F01ul
#define SPEED_400KHZ_DATA_TIMING_2		0x0Aul
#define SPEED_400KHZ_IDLE_SCALING		0x01000050ul
#define SPEED_400KHZ_TIMEOUT_SCALING		0x149CC2C7ul

/* SMBus Timing values for 100kHz speed */
#define SPEED_100KHZ_BUS_CLOCK			0x4F4Ful
#define SPEED_100KHZ_DATA_TIMING		0x0C4D4306ul
#define SPEED_100KHZ_DATA_TIMING_2		0x4Dul
#define SPEED_100KHZ_IDLE_SCALING		0x01FC01EDul
#define SPEED_100KHZ_TIMEOUT_SCALING		0x4B9CC2C7ul

/* SMBus Timing values for 40kHz speed. Use 100KHz except bus clock */
#define SPEED_40KHZ_BUS_CLOCK			0xC7C7ul

/* Status */
#define STS_NBB (1 << 0) /* Bus busy */
#define STS_LAB (1 << 1) /* Arbitration lost */
#define STS_LRB (1 << 3) /* Last received bit */
#define STS_BER (1 << 4) /* Bus error */
#define STS_PIN (1 << 7) /* Pending interrupt */

/* Control */
#define CTRL_ACK (1 << 0) /* Acknowledge */
#define CTRL_STO (1 << 1) /* STOP */
#define CTRL_STA (1 << 2) /* START */
#define CTRL_ENI (1 << 3) /* Enable interrupt */
#define CTRL_ESO (1 << 6) /* Enable serial output */
#define CTRL_PIN (1 << 7) /* Pending interrupt not */

/* Completion */
#define COMP_DTEN	(1ul << 2) /* enable device timeouts */
#define COMP_MCEN	(1ul << 3) /* enable master cumulative timeouts */
#define COMP_SCEN	(1ul << 4) /* enable slave cumulative timeouts */
#define COMP_BIDEN	(1ul << 5) /* enable Bus idle timeouts */
#define COMP_IDLE	(1 << 29) /* i2c bus is idle */
#define COMP_RW_BITS_MASK 0x3C /* R/W bits mask */

/* Configuration */
#define CFG_PORT_MASK	(0x0Ful)	/* port selection field */
#define CFG_TCEN	(1ul << 4)	/* Enable HW bus timeouts */
#define CFG_FEN		(1ul << 8)	/* enable input filtering */
#define CFG_RESET	(1ul << 9)	/* reset controller */
#define CFG_ENABLE	(1ul << 10)	/* enable controller */
/* disable response to general call address */
#define CFG_GC_DIS	(1ul << 14)
#define CFG_ENIDI	(1ul << 29)	/* Enable I2C idle interrupt */
/* Enable network layer master done interrupt */
#define CFG_ENMI	(1ul << 30)
/* Enable network layer slave done interrupt */
#define CFG_ENSI	(1ul << 31)

/* Master Command */
#define MCMD_MRUN		(1ul << 0)
#define MCMD_MPROCEED		(1ul << 1)
#define MCMD_START0		(1ul << 8)
#define MCMD_STARTN		(1ul << 9)
#define MCMD_STOP		(1ul << 10)
#define MCMD_READM		(1ul << 12)
#define MCMD_WCNT_BITPOS	(16)
#define MCMD_WCNT_MASK0		(0xFF)
#define MCMD_WCNT_MASK		(0xFFul << 16)
#define MCMD_RCNT_BITPOS	(24)
#define MCMD_RCNT_MASK0		(0xFF)
#define MCMD_RCNT_MASK		(0xFFul << 24)

/* Maximum transfer of a SMBUS block transfer */
#define SMBUS_MAX_BLOCK_SIZE 32

/*
 * Amount of time to blocking wait for i2c bus to finish. After this
 * blocking timeout, if the bus is still not finished, then allow other
 * tasks to run.
 * Note: this is just long enough for a 400kHz bus to finish transmitting
 * one byte assuming the bus isn't being held.
 */
#define I2C_WAIT_BLOCKING_TIMEOUT_US 35	/* was 25 */

enum i2c_transaction_state {
	/* Stop condition was sent in previous transaction */
	I2C_TRANSACTION_STOPPED,
	/* Stop condition was not sent in previous transaction */
	I2C_TRANSACTION_OPEN,
};

/* I2C controller state data
 * NOTE: I2C_CONTROLLER_COUNT is defined at board level.
 * Added transaction context to reduce stack usage in
 * call tree of chip_i2c_xfer.
 */
static struct i2c_cdata {
	/* Transaction timeout, or 0 to use default. */
	uint32_t timeout_us;
	/* Task waiting on port, or TASK_ID_INVALID if none. */
	/*
	 * MCHP Remove volatile.
	 * ISR only reads.
	 * Non-ISR only writes when interrupt is disabled.
	 */
	task_id_t task_waiting;
	enum i2c_transaction_state transaction_state;
	/* transaction context */
	int out_size;
	const uint8_t *outp;
	int in_size;
	uint8_t *inp;
	int xflags;
	uint32_t i2c_complete; /* ISR write */
	uint32_t flags;
	uint8_t port;
	uint8_t slv_addr;
	uint8_t ctrl;
	uint8_t hwsts;
	uint8_t hwsts2;
	uint8_t hwsts3; /* ISR write */
	uint8_t hwsts4;
	uint8_t lines;
} cdata[I2C_CONTROLLER_COUNT];

static const uint16_t i2c_controller_pcr[MCHP_I2C_CTRL_MAX] = {
	MCHP_PCR_I2C0,
	MCHP_PCR_I2C1,
	MCHP_PCR_I2C2,
	MCHP_PCR_I2C3
};

static void i2c_ctrl_slp_en(int controller, int sleep_en)
{
	if ((controller < 0) || (controller > MCHP_I2C_CTRL_MAX))
		return;

	if (sleep_en)
		MCHP_PCR_SLP_EN_DEV(i2c_controller_pcr[controller]);
	else
		MCHP_PCR_SLP_DIS_DEV(i2c_controller_pcr[controller]);
}

static int chip_i2c_is_controller_valid(int controller)
{
	if ((controller < 0) || (controller >= MCHP_I2C_CTRL_MAX))
		return 0;

	return 1;
}

uint32_t chip_i2c_get_ctx_flags(int port)
{
	int controller = i2c_port_to_controller(port);

	if (!chip_i2c_is_controller_valid(controller))
		return 0;

	return cdata[controller].flags;
}

/*
 * Refer to NXP UM10204 for minimum timing requirement of T_Low and
 * T_High.
 * http://www.nxp.com/documents/user_manual/UM10204.pdf
 * I2C spec. timing value are used in recommended registers values
 * for 100, 400, and 1000 in MCHP I2C_SMB_Controller_3.6.pdf
 * We preserve the ratio of tlo and thi when adjusting bus clock
 * Limit requested 31 < frequency <= 1000 due to Tlo and Thi being
 * 8-bit values and Tbasefreq = 1/16MHz. Print message indicating
 * we are limiting the frequency.
 *
 * T = ((Tlo + 1) + (Thi + 1)) * Tbasefreq
 *
 * F <= 100kHz. Tlo == Thi == Tp
 * Fb/F = 2Tp
 *
 * 100kHz < F <= 400kHz. Thi/Tlo = 0x17/0x0F.
 * Approximate as Thp/Tlp ~ 0x18/0x10. Thp = (0x18/0x10) * Tlp
 * Tlp = (Fb/F) * (0x10/0x28)
 * Thp = (Fb/F) * (0x18/0x28)
 *
 * F > 400kHz. Th/Tl = 0x09/0x05.
 * Tlp = (Fb/F) * (5/14) + 1
 * Thp = (Fb/F) * (9/14)
 */
static void configure_controller_speed(int controller, int kbps)
{
	int t_low, t_high, fdiv;

	if (kbps < 31) {
		CPUTS("I2C frequency(< minimum). Limit to min(31kHz)");
		kbps = 32;
	}

	if (kbps > 1000) {
		CPUTS("I2C frequency(>1000). Limit to 1000kHz");
		kbps = 1000;
	}

	/* Clear PCR sleep enable for controller */
	i2c_ctrl_slp_en(controller, 0);

	fdiv = I2C_CLOCK_KBPS / kbps;

	if (kbps > 400) { /* Fast mode plus */
		MCHP_I2C_DATA_TIM(controller) = SPEED_1MHZ_DATA_TIMING;
		MCHP_I2C_DATA_TIM_2(controller) = SPEED_1MHZ_DATA_TIMING_2;
		MCHP_I2C_IDLE_SCALE(controller) = SPEED_1MHZ_IDLE_SCALING;
		MCHP_I2C_TOUT_SCALE(controller) = SPEED_1MHZ_TIMEOUT_SCALING;
		t_low = (((fdiv << 2) + fdiv) / 14) + 1;
		t_high = ((fdiv  << 3) + fdiv) / 14;
	} else if (kbps > 100) { /* Fast mode */
		MCHP_I2C_DATA_TIM(controller) = SPEED_400KHZ_DATA_TIMING;
		MCHP_I2C_DATA_TIM_2(controller) = SPEED_400KHZ_DATA_TIMING_2;
		MCHP_I2C_IDLE_SCALE(controller) = SPEED_400KHZ_IDLE_SCALING;
		MCHP_I2C_TOUT_SCALE(controller) = SPEED_400KHZ_TIMEOUT_SCALING;
		t_low = (fdiv << 4) / 0x28;
		t_high = ((fdiv << 4) + (fdiv << 3)) / 0x28;
	} else { /* Standard mode */
		MCHP_I2C_DATA_TIM(controller) = SPEED_100KHZ_DATA_TIMING;
		MCHP_I2C_DATA_TIM_2(controller) = SPEED_100KHZ_DATA_TIMING_2;
		MCHP_I2C_IDLE_SCALE(controller) = SPEED_100KHZ_IDLE_SCALING;
		MCHP_I2C_TOUT_SCALE(controller) = SPEED_100KHZ_TIMEOUT_SCALING;
		t_low = (fdiv >> 1);
		t_high = t_low;
	}

	if (t_low > 0x100)
		t_low = 0x100;

	if (t_high > 0x100)
		t_high = 0x100;

	t_low--;
	t_high--;

	MCHP_I2C_BUS_CLK(controller) = ((t_high & 0xff) << 8) |
					  (t_low & 0xff);
}

/*
 * NOTE: direct mode interrupts do not need GIRQn bit
 * set in aggregator block enable register.
 */
static void enable_controller_irq(int controller)
{
	MCHP_INT_ENABLE(MCHP_I2C_GIRQ) =
			MCHP_I2C_GIRQ_BIT(controller);
	task_enable_irq(MCHP_IRQ_I2C_0 + controller);
}

static void disable_controller_irq(int controller)
{
	MCHP_INT_DISABLE(MCHP_I2C_GIRQ) =
			MCHP_I2C_GIRQ_BIT(controller);
	/* read back into read-only reg. to insure disable takes effect */
	MCHP_INT_BLK_IRQ = MCHP_INT_DISABLE(MCHP_I2C_GIRQ);
	task_disable_irq(MCHP_IRQ_I2C_0 + controller);
	task_clear_pending_irq(MCHP_IRQ_I2C_0 + controller);
}

static void configure_controller(int controller, int port, int kbps)
{
	if (!chip_i2c_is_controller_valid(controller))
		return;

	disable_controller_irq(controller);
	MCHP_INT_SOURCE(MCHP_I2C_GIRQ) =
			MCHP_I2C_GIRQ_BIT(controller);

	/* set to default except for port select field b[3:0] */
	MCHP_I2C_CONFIG(controller) = (uint32_t)(port & 0xf);

	MCHP_I2C_CTRL(controller) = CTRL_PIN;
	MCHP_I2C_OWN_ADDR(controller) = board_i2c_slave_addrs(controller);
	configure_controller_speed(controller, kbps);

	MCHP_I2C_CTRL(controller) = CTRL_PIN | CTRL_ESO |
				CTRL_ACK | CTRL_ENI;

	/* Do not enable SMBus timeouts. I2C allows slaves to
	 * clock stretch almost indefinitely.
	 */
	MCHP_I2C_COMPLETE(controller) =
		MCHP_I2C_COMPLETE(controller) & ~(0x3Cul);
	/* filter enable, disable General Call */
	MCHP_I2C_CONFIG(controller) |= CFG_FEN + CFG_GC_DIS;
	/* enable controller */
	MCHP_I2C_CONFIG(controller) |= CFG_ENABLE;
}

static void reset_controller(int controller)
{
	int i;

	MCHP_I2C_CONFIG(controller) |= 1 << 9;
	udelay(100);
	MCHP_I2C_CONFIG(controller) &= ~(1 << 9);

	for (i = 0; i < i2c_ports_used; ++i)
		if (controller == i2c_port_to_controller(i2c_ports[i].port)) {
			configure_controller(controller, i2c_ports[i].port,
						i2c_ports[i].kbps);
			cdata[controller].transaction_state =
				I2C_TRANSACTION_STOPPED;
			break;
		}
}

static int check_addr_is_slave(int controller, int addr)
{
	uint32_t own_addrs = MCHP_I2C_OWN_ADDR(controller);

	addr &= 0xff;

	if (((own_addrs & 0xff) == addr) ||
		(((own_addrs >> 8) & 0xff) == addr))
		return 1;

	return 0;
}

static int wait_for_interrupt(int controller, int timeout)
{
	int event;

	if (timeout <= 0)
		return EC_ERROR_TIMEOUT;

	cdata[controller].task_waiting = task_get_current();
	enable_controller_irq(controller);

	/* Enable Idle interrupt */
	MCHP_I2C_CONFIG(controller) |= CFG_ENIDI;

	/* Wait until I2C interrupt or timeout. */
	event = task_wait_event_mask(TASK_EVENT_I2C_IDLE, timeout);

	disable_controller_irq(controller);
	cdata[controller].task_waiting = TASK_ID_INVALID;

	MCHP_I2C_CONFIG(controller) &= ~CFG_ENIDI;

	return (event & TASK_EVENT_TIMER) ? EC_ERROR_TIMEOUT : EC_SUCCESS;
}

static int wait_idle(int controller)
{
	uint8_t sts = MCHP_I2C_STATUS(controller);
	uint64_t block_timeout = get_time().val + I2C_WAIT_BLOCKING_TIMEOUT_US;
	uint64_t task_timeout = block_timeout + cdata[controller].timeout_us;
	int rv = 0;

	while (!(sts & STS_NBB)) {
		if (rv)
			return rv;
		if (get_time().val > block_timeout)
			rv = wait_for_interrupt(controller,
						task_timeout - get_time().val);
		sts = MCHP_I2C_STATUS(controller);
	}

	if (sts & (STS_BER | STS_LAB))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/*
 * Return EC_SUCCESS on ACK of byte else EC_ERROR_UNKNOWN.
 * Record I2C.Status in cdata[controller] structure.
 * Byte transmit finished with no I2C bus error or lost arbitration.
 * PIN -> 0. LRB bit contains slave ACK/NACK bit.
 * Slave ACK:  I2C.Status == 0x00
 * Slave NACK: I2C.Status == 0x08
 * Byte transmit finished with I2C bus errors or lost arbitration.
 * PIN -> 0 and BER and/or LAB set.
 *
 * Byte receive finished with no I2C bus errors or lost arbitration.
 * PIN -> 0. LRB=0/1 based on ACK bit in I2C.Control.
 * Master receiver must NACK last byte it wants to receive.
 * How do we handle this if we don't know direction of transfer?
 * I2C.Control is write-only so we can't see Master's ACK control
 * bit.
 *
 */
static int wait_byte_done(int controller, uint8_t mask,
							uint8_t expected)
{
	uint64_t block_timeout;
	uint64_t task_timeout;
	int rv;
	uint8_t sts;

	rv = 0;
	block_timeout = get_time().val + I2C_WAIT_BLOCKING_TIMEOUT_US;
	task_timeout = block_timeout + cdata[controller].timeout_us;
	sts = MCHP_I2C_STATUS(controller);
	cdata[controller].hwsts = sts;
	while (sts & STS_PIN) {
		if (rv)
			return rv;
		if (get_time().val > block_timeout)
			rv = wait_for_interrupt(controller,
						task_timeout - get_time().val);
		sts = MCHP_I2C_STATUS(controller);
		cdata[controller].hwsts = sts;
	}

	rv = EC_SUCCESS;
	if ((sts & mask) != expected)
		rv = EC_ERROR_UNKNOWN;

	return rv;
}

/*
 * Select port on controller. If controller configured
 * for speicified port do nothing.
 * A more robust port switching:
 * Soft reset controller. Cleans up HW state machines
 * due to bad transaction on current port.
 * Re-configure controller with new port.
 */
static void select_port(int port, int controller)
{
	uint32_t port_sel;

	port_sel = (uint32_t)(port & 0x0f);
	if ((MCHP_I2C_CONFIG(controller) & 0x0f) == port_sel)
		return;

	MCHP_I2C_CONFIG(controller) |= 1 << 9;
	udelay(100);
	MCHP_I2C_CONFIG(controller) &= ~(1 << 9);

	configure_controller(controller, port_sel, i2c_ports[port].kbps);
}

/*
 * Use safe method (reading GPIO.Control PAD input bit)
 * to obtain line levels. Store SCL line state in bit[0]
 * and SDA line state in bit[1].
 * NOTE: I2C controller bit-bang register is not safe. Using
 * bit-bang requires timeouts be disabled AND the controller must
 * be idle. Switching controller to bit-bang mode when the controller
 * is not idle will cause problems.
 */
static uint32_t get_line_level(int port)
{
	uint32_t lines;

	lines = i2c_raw_get_scl(port) & 0x01;
	lines |= (i2c_raw_get_sda(port) & 0x01) << 1;

	return lines;
}

/*
 * Debug: dump cdata[ctrl] using MCHP TFDP which
 * is much faster than UART.
 * Future alternative Use ARM ITM single wire output and
 * capture with JTAG box.
 */
#if DEBUG_I2C
static void i2c_debug_dump(int ctrl)
{
	int i;

	trace3(0, I2C, 0, "I2C Error: ctrl[%d] port[%d] xflags=%0x",
		ctrl, cdata[ctrl].port, cdata[ctrl].xflags);
	trace3(0, I2C, 0, "  slvAddr=0x%0x wlen=%d rlen=%d",
		cdata[ctrl].slv_addr, cdata[ctrl].out_size,
		cdata[ctrl].in_size);
	trace12(0, I2C, 0, "  complete=0x%08x flags=0x%08x",
		cdata[ctrl].i2c_complete, cdata[ctrl].flags);
	trace4(0, I2C, 0, "  sts1=0x%0x sts2=0x%0x sts3=0x%0x sts4=0x%0x",
		cdata[ctrl].hwsts, cdata[ctrl].hwsts2,
		cdata[ctrl].hwsts3, cdata[ctrl].hwsts4);
	trace1(0, I2C, 0, "  lines=0x%0x", cdata[ctrl].lines);
	trace13(0, I2C, 0, "  timeout_us=%d  task_id=%d state=%d",
		cdata[ctrl].timeout_us, (uint32_t)cdata[ctrl].task_waiting,
		(uint32_t)cdata[ctrl].transaction_state);
	if (cdata[ctrl].out_size) {
		if (cdata[ctrl].outp != NULL) {
			for (i = 0; i < cdata[ctrl].out_size; i++) {
				trace2(0, I2C, 0,
					"  out_data[%d]=0x%0x", i,
					cdata[ctrl].outp[i]);
			}
		}
	}
}
#endif

/*
 * Check if I2C port connected to controller has bus error or
 * other signalling issues such as stuck clock/data lines.
 */
static int i2c_check_recover(int port, int controller)
{
	uint32_t lines;
	uint8_t reg;

	lines = get_line_level(port);
	reg = MCHP_I2C_STATUS(controller);

	if ((((reg & (STS_BER | STS_LAB)) || !(reg & STS_NBB)) ||
			(lines != I2C_LINE_IDLE))) {
		cdata[controller].flags |= (1ul << 16);

		CPRINTS("I2C%d port%d recov status 0x%02x, SDA:SCL=0x%0x\n",
			controller, port, reg, lines);

		/* Attempt to unwedge the port. */
		if (lines != I2C_LINE_IDLE)
			i2c_unwedge(port);

		/* Bus error, bus busy, or arbitration lost. Try reset. */
		reset_controller(controller);
		select_port(port, controller);

		/*
		 * We don't know what edges the slave saw, so sleep long enough
		 * that the slave will see the new start condition below.
		 */
		usleep(1000);

		reg = MCHP_I2C_STATUS(controller);
		lines = get_line_level(port);

		if ((reg & (STS_BER | STS_LAB)) || !(reg & STS_NBB) ||
				(lines != I2C_LINE_IDLE))
			return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static inline void push_in_buf(uint8_t **in, uint8_t val, int skip)
{
	if (!skip) {
		**in = val;
		(*in)++;
	}
}

/*
 * I2C Master transmit
 * Caller has filled in cdata[ctrl] parameters
 */
static int i2c_mtx(int ctrl)
{
	int i, rv;

	rv = EC_SUCCESS;
	cdata[ctrl].flags |= (1ul << 1);

	if (cdata[ctrl].xflags & I2C_XFER_START) {
		cdata[ctrl].flags |= (1ul << 2);
		MCHP_I2C_DATA(ctrl) = cdata[ctrl].slv_addr;

		/* Clock out the slave address, sending START bit */
		MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
				       CTRL_ENI | CTRL_ACK |
				       CTRL_STA;
		cdata[ctrl].transaction_state = I2C_TRANSACTION_OPEN;
	}

	for (i = 0; i < cdata[ctrl].out_size; ++i) {
		rv = wait_byte_done(ctrl, 0xff, 0x00);
		if (rv) {
			cdata[ctrl].flags |= (1ul << 17);
			MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
					CTRL_ENI | CTRL_STO | CTRL_ACK;
			return rv;
		}
		cdata[ctrl].flags |= (1ul << 15);
		MCHP_I2C_DATA(ctrl) = cdata[ctrl].outp[i];
	}

	rv = wait_byte_done(ctrl, 0xff, 0x00);
	if (rv) {
		cdata[ctrl].flags |= (1ul << 18);
		MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO | CTRL_ENI |
			CTRL_STO | CTRL_ACK;
		return rv;
	}

	/*
	 * Send STOP bit if the stop flag is on, and caller
	 * doesn't expect to receive data.
	 */
	if ((cdata[ctrl].xflags & I2C_XFER_STOP) &&
			(cdata[ctrl].in_size == 0)) {
		cdata[ctrl].flags |= (1ul << 3);
		MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
				       CTRL_STO | CTRL_ACK;
		cdata[ctrl].transaction_state = I2C_TRANSACTION_STOPPED;
	}

	return rv;
}

/*
 * I2C Master-Receive helper routine for sending START or
 * Repeated-START.
 * This routine should only be called if a (Repeated-)START
 * is required.
 * If I2C controller is Idle or Stopped
 *   Send START by:
 *	Write read address to I2C.Data
 *	Write PIN=ESO=STA=ACK=1, STO=0 to I2C.Ctrl. This
 *	will trigger controller to output 8-bits of data.
 * Else if I2C controller is Open (previous START sent)
 *   Send Repeated-START by:
 *	Write ESO=STA=ACK=1, PIN=STO=0 to I2C.Ctrl. Controller
 *	will generate START but not transmit data.
 *	Write read address to I2C.Data. Controller will transmit
 *	8-bits of data
 * Endif
 * NOTE: Controller clocks in address on SDA as its transmitting.
 * Therefore 1-byte RX-FIFO will contain address plus R/nW bit.
 * Controller will wait for slave to release SCL before transmitting
 * 9th clock and latching (N)ACK on SDA.
 * Spin on I2C.Status PIN -> 0. Enable I2C interrupt if spin time
 * exceeds threshold. If a timeout occurs generate STOP and return
 * an error.
 *
 * Because I2C generates clocks for next byte when reading I2C.Data
 * register we must prepare control logic.
 * If the caller requests STOP and read length is 1 then set
 * clear ACK bit in I2C.Ctrl. Set ESO=ENI=1, PIN=STA=STO=ACK=0
 * in I2C.Ctrl. Master must NACK last byte.
 */
static int i2c_mrx_start(int ctrl)
{
	uint8_t u8;
	int rv;

	cdata[ctrl].flags |= (1ul << 4);

	u8 = CTRL_ESO | CTRL_ENI | CTRL_STA | CTRL_ACK;
	if (cdata[ctrl].transaction_state == I2C_TRANSACTION_OPEN) {
		cdata[ctrl].flags |= (1ul << 5);
		/* Repeated-START then address */
		MCHP_I2C_CTRL(ctrl) = u8;
	}
	MCHP_I2C_DATA(ctrl) = cdata[ctrl].slv_addr | 0x01;
	if (cdata[ctrl].transaction_state == I2C_TRANSACTION_STOPPED) {
		cdata[ctrl].flags |= (1ul << 6);
		/* address then START */
		MCHP_I2C_CTRL(ctrl) = u8 | CTRL_PIN;
	}
	cdata[ctrl].transaction_state = I2C_TRANSACTION_OPEN;

	/* Controller generates START, transmits data(address) capturing
	 * 9-bits from SDA (8-bit address + (N)Ack bit).
	 * We leave captured address in I2C.Data register.
	 * Master Receive data read routine assumes data is pending
	 * in I2C.Data
	 */
	cdata[ctrl].flags |= (1ul << 7);
	rv = wait_byte_done(ctrl, 0xff, 0x00);
	if (rv) {
		cdata[ctrl].flags |= (1ul << 19);
		MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
			CTRL_STO | CTRL_ACK;
		return rv;
	}

	/* if STOP requested and last 1 or 2 bytes prepare controller
	 * to NACK last byte. Do this before read of dummy data so
	 * controller is setup to NACK last byte.
	 */
	cdata[ctrl].flags |= (1ul << 8);
	if (cdata[ctrl].xflags & I2C_XFER_STOP &&
			(cdata[ctrl].in_size < 2)) {
		cdata[ctrl].flags |= (1ul << 9);
		MCHP_I2C_CTRL(ctrl) = CTRL_ESO | CTRL_ENI;
	}

	/*
	 * Read & discard slave address.
	 * Generates clocks for next data
	 */
	cdata[ctrl].flags |= (1ul << 10);
	u8 = MCHP_I2C_DATA(ctrl);

	return rv;
}

/*
 * I2C Master-Receive data read helper.
 * Assumes I2C is in use, (Rpt-)START was previously sent.
 * Reading I2C.Data generates clocks for the next byte. If caller
 * requests STOP then we must clear I2C.Ctrl ACK before reading
 * second to last byte from RX-FIFO data register. Before reading
 * the last byte we must set I2C.Ctrl to generate a stop after
 * the read from RX-FIFO register.
 * NOTE: I2C.Status.LRB only records the (N)ACK bit in master
 * transmit mode, not in master receive mode.
 * NOTE2: Do not set ENI bit in I2C.Ctrl for STOP generation.
 */
static int i2c_mrx_data(int ctrl)
{
	uint32_t nrx = (uint32_t)cdata[ctrl].in_size;
	uint32_t stop = (uint32_t)cdata[ctrl].xflags & I2C_XFER_STOP;
	uint8_t *pdest = cdata[ctrl].inp;
	int rv;

	cdata[ctrl].flags |= (1ul << 11);
	while (nrx) {
		rv = wait_byte_done(ctrl, 0xff, 0x00);
		if (rv) {
			cdata[ctrl].flags |= (1ul << 20);
			MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
				CTRL_STO | CTRL_ACK;
			return rv;
		}

		if (stop) {
			if (nrx == 2) {
				cdata[ctrl].flags |= (1ul << 12);
				MCHP_I2C_CTRL(ctrl) = CTRL_ESO | CTRL_ENI;
			} else if (nrx == 1) {
				cdata[ctrl].flags |= (1ul << 13);
				MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
					CTRL_STO | CTRL_ACK;
			}
		}

		*pdest++ = MCHP_I2C_DATA(ctrl);
		nrx--;
	}

	cdata[ctrl].flags |= (1ul << 14);
	return EC_SUCCESS;
}

/*
 * Called from common/i2c_master
 */
int chip_i2c_xfer(int port, int slave_addr, const uint8_t *out,
		int out_size, uint8_t *in, int in_size, int flags)
{
	int ctrl;
	int ret_done;

	if (out_size == 0 && in_size == 0)
		return EC_SUCCESS;

	ctrl = i2c_port_to_controller(port);

	if (!chip_i2c_is_controller_valid(ctrl))
		return EC_ERROR_INVAL;

	disable_controller_irq(ctrl);

	if (check_addr_is_slave(ctrl, slave_addr))
		return EC_ERROR_INVAL;

	cdata[ctrl].flags = (1ul << 0);

	/* store transfer context */
	cdata[ctrl].i2c_complete = 0;
	cdata[ctrl].hwsts = 0;
	cdata[ctrl].hwsts2 = 0;
	cdata[ctrl].hwsts3 = 0;
	cdata[ctrl].hwsts4 = 0;
	cdata[ctrl].port = (uint8_t)(port & 0xff);
	cdata[ctrl].slv_addr = (uint8_t)(slave_addr & 0xff);
	cdata[ctrl].out_size = out_size;
	cdata[ctrl].outp = out;
	cdata[ctrl].in_size = in_size;
	cdata[ctrl].inp = in;
	cdata[ctrl].xflags = flags;

	select_port(port, ctrl);

	if ((flags & I2C_XFER_START) &&
	    cdata[ctrl].transaction_state == I2C_TRANSACTION_STOPPED) {
		wait_idle(ctrl);
		ret_done = i2c_check_recover(port, ctrl);
		if (ret_done)
			goto err_chip_i2c_xfer;
	}

	ret_done = EC_SUCCESS;

	if (out_size) {
		ret_done = i2c_mtx(ctrl);
		if (ret_done)
			goto err_chip_i2c_xfer;
	}

	if (in_size) {
		if (cdata[ctrl].xflags & I2C_XFER_START) {
			ret_done = i2c_mrx_start(ctrl);
			if (ret_done)
				goto err_chip_i2c_xfer;
		}

		ret_done = i2c_mrx_data(ctrl);
		if (ret_done)
			goto err_chip_i2c_xfer;
	}

	cdata[ctrl].flags |= (1ul << 15);
	/* MCHP wait for STOP to complete */
	if (cdata[ctrl].xflags & I2C_XFER_STOP)
		wait_idle(ctrl);

	/* Check for error conditions */
	if (MCHP_I2C_STATUS(ctrl) & (STS_LAB | STS_BER)) {
		cdata[ctrl].flags |= (1ul << 21);
		goto err_chip_i2c_xfer;
	}

	cdata[ctrl].flags |= (1ul << 14);

	return EC_SUCCESS;

err_chip_i2c_xfer:
	cdata[ctrl].flags |= (1ul << 22);

	cdata[ctrl].hwsts2 = MCHP_I2C_STATUS(ctrl); /* record status */

	/*
	 * NOTE: writing I2C.Ctrl.PIN=1 will clear all bits
	 * except NBB in I2C.Status
	 */
	MCHP_I2C_CTRL(ctrl) = CTRL_PIN | CTRL_ESO |
				       CTRL_STO | CTRL_ACK;
	cdata[ctrl].transaction_state = I2C_TRANSACTION_STOPPED;

	/* record status after STOP */
	cdata[ctrl].hwsts4 = MCHP_I2C_STATUS(ctrl);

	/* record line levels.
	 * NOTE: line levels may reflect STOP condition
	 */
	cdata[ctrl].lines = (uint8_t)get_line_level(cdata[ctrl].port);

#if DEBUG_I2C
	i2c_debug_dump(ctrl);
#endif
	if (cdata[ctrl].hwsts2 & STS_BER) {
		cdata[ctrl].flags |= (1ul << 23);
		reset_controller(ctrl);
	}

	return EC_ERROR_UNKNOWN;
}

/*
 * A safe method of reading port's SCL pin level.
 */
int i2c_raw_get_scl(int port)
{
	enum gpio_signal g;

	/* If no SCL pin defined for this port,
	 * then return 1 to appear idle.
	 */
	if (get_scl_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

/*
 * A safe method of reading port's SDA pin level.
 */
int i2c_raw_get_sda(int port)
{
	enum gpio_signal g;

	if (get_sda_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

/*
 * Caller is responsible for locking the port.
 */
int i2c_get_line_levels(int port)
{
	int rv, controller;

	controller = i2c_port_to_controller(port);
	if (controller < 0)
		return 0x03; /* No controller, return high line levels */

	select_port(port, controller);
	rv = get_line_level(port);
	return rv;
}

/*
 * I2C port must be a zero based number.
 * MCHP I2C can map any port to any of the 4 controllers.
 * Call board level function as board designs may choose
 * to wire up and group ports differently.
 */
int i2c_port_to_controller(int port)
{
	return board_i2c_p2c(port);
}

void i2c_set_timeout(int port, uint32_t timeout)
{
	/* Param is port, but timeout is stored by-controller. */
	cdata[i2c_port_to_controller(port)].timeout_us =
		timeout ? timeout : I2C_TIMEOUT_DEFAULT_US;
}

/*
 * Initialize I2C controllers specified by the board configuration.
 * If multiple ports are mapped to the same controller choose the
 * lowest speed.
 */
static void i2c_init(void)
{
	int i;
	int controller, kbps;
	int controller_kbps[MCHP_I2C_CTRL_MAX];

	for (i = 0; i < MCHP_I2C_CTRL_MAX; i++)
		controller_kbps[i] = 0;

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);

	memset(cdata, 0, sizeof(cdata));

	for (i = 0; i < i2c_ports_used; ++i) {
		controller = i2c_port_to_controller(i2c_ports[i].port);

		kbps = i2c_ports[i].kbps;
		if (controller_kbps[controller] &&
				(controller_kbps[controller] != kbps)) {
			CPRINTS("i2c_init(): controller %d"
				" speed conflict: %d != %d",
				controller, kbps,
				controller_kbps[controller]);
			kbps = MIN(kbps, controller_kbps[controller]);
		}
		controller_kbps[controller] = kbps;

		configure_controller(controller, i2c_ports[i].port,
					controller_kbps[controller]);

		memset(&cdata[controller], 0, sizeof(struct i2c_cdata));
		cdata[controller].task_waiting = TASK_ID_INVALID;
		cdata[controller].transaction_state = I2C_TRANSACTION_STOPPED;
		/* Use default timeout. */
		i2c_set_timeout(i2c_ports[i].port, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_INIT_I2C);

/*
 * Handle I2C interrupts.
 * I2C controller is configured to fire interrupts on
 * anything causing PIN 1->0 and I2C IDLE (NBB -> 1).
 * NVIC interrupt disable must clear NVIC pending bit.
 */
static void handle_interrupt(int controller)
{
	uint32_t r;
	uint8_t sts;
	int id = cdata[controller].task_waiting;

	/*
	 * Write to control register interferes with I2C transaction.
	 * Instead, let's disable IRQ from the core until the next time
	 * we want to wait for STS_PIN/STS_NBB.
	 */

	/* disable Idle interrupt */
	MCHP_I2C_CONFIG(controller) &= ~CFG_ENIDI;

	disable_controller_irq(controller);

	sts = MCHP_I2C_STATUS(controller);
	cdata[controller].hwsts3 = sts;

	/* Clear all interrupt status */
	r = MCHP_I2C_COMPLETE(controller);
	MCHP_I2C_COMPLETE(controller) = r;
	cdata[controller].i2c_complete = r;

	MCHP_INT_SOURCE(MCHP_I2C_GIRQ) = MCHP_I2C_GIRQ_BIT(controller);

	/* Wake up the task waiting on the I2C interrupt, if any. */
	if (id != TASK_ID_INVALID)
		task_set_event(id, TASK_EVENT_I2C_IDLE, 0);
}

void i2c0_interrupt(void) { handle_interrupt(0); }
void i2c1_interrupt(void) { handle_interrupt(1); }
void i2c2_interrupt(void) { handle_interrupt(2); }
void i2c3_interrupt(void) { handle_interrupt(3); }

DECLARE_IRQ(MCHP_IRQ_I2C_0, i2c0_interrupt, 2);
DECLARE_IRQ(MCHP_IRQ_I2C_1, i2c1_interrupt, 2);
DECLARE_IRQ(MCHP_IRQ_I2C_2, i2c2_interrupt, 2);
DECLARE_IRQ(MCHP_IRQ_I2C_3, i2c3_interrupt, 2);

#if DEBUG_I2C
static int dump_i2c(int argc, char **argv)
{
	char *e;
	int ctrl, busclk, freq;

	ctrl = 0;
	if (argc > 1) {
		ctrl = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
	}

	if (ctrl > I2C_CONTROLLER_COUNT)
		return EC_ERROR_INVAL;

	CPRINTS("I2C[%d] registers", ctrl);
	CPRINTS("Control    =0x%02x", MCHP_I2C_CTRL(ctrl));
	CPRINTS("Status     =0x%02x", MCHP_I2C_STATUS(ctrl));
	CPRINTS("Complete   =0x%08x", MCHP_I2C_COMPLETE(ctrl));
	CPRINTS("Config     =0x%08x", MCHP_I2C_CONFIG(ctrl));
	CPRINTS("BusClk     =0x%04x", MCHP_I2C_BUS_CLK(ctrl));
	CPRINTS("DataTim    =0x%04x", MCHP_I2C_DATA_TIM(ctrl));
	CPRINTS("DataTim2   =0x%04x", MCHP_I2C_DATA_TIM_2(ctrl));
	CPRINTS("IdleScale  =0x%04x", MCHP_I2C_IDLE_SCALE(ctrl));
	CPRINTS("TmoutScale =0x%04x", MCHP_I2C_TOUT_SCALE(ctrl));
	CPRINTS("timeout_us =%d", cdata[ctrl].timeout_us);
	CPRINTS("transaction_state =%d", cdata[ctrl].transaction_state);
	CPRINTS("task_waiting =%d", cdata[ctrl].task_waiting);

	busclk = MCHP_I2C_BUS_CLK(ctrl);
	freq = 16000 / ((busclk & 0xFF) + ((busclk >> 8) & 0xFF) + 2);
	CPRINTS("I2C clock = %d kHz", freq);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2c_dump, dump_i2c, "ctrl", "Dump I2C");

static int setfreq_i2c(int argc, char **argv)
{
	char *e;
	int ctrl, freq;

	ctrl = 0;
	if (argc > 1) {
		ctrl = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
	}

	if (ctrl > I2C_CONTROLLER_COUNT)
		return EC_ERROR_INVAL;

	freq = 100;
	if (argc > 2) {
		freq = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;
	}

	CPRINTS("I2C[%d] Set frequency = %d", freq);

	MCHP_I2C_CONFIG(ctrl) |= 1 << 9;
	udelay(100);
	MCHP_I2C_CONFIG(ctrl) &= ~(1 << 9);

	configure_controller_speed(ctrl, freq);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2c_freq, setfreq_i2c,
	"ctrl freq", "Set I2C to freq(kHz)");
#endif
