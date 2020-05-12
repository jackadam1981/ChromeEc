/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

#define I2C_ERROR_FAILED_START EC_ERROR_INTERNAL_FIRST

/* Transmit timeout in microseconds */
#define I2C_TX_TIMEOUT_MASTER   (10 * MSEC)

#define  I2C_WAIT_ISR_NORMAL 0

enum i2c_freq_khz {
	freq_100 = 100,
	freq_400 = 400,
	freq_1000 = 1000,
};

struct i2c_timing {
	uint8_t scll;
	uint8_t sclh;
	uint8_t sdadel;
	uint8_t scldel;
	uint8_t presc;
};

#if I2C_WAIT_ISR_NORMAL
/* timing register values for supported input clks / i2c clk rates */
static const uint32_t busyloop_us[I2C_FREQ_COUNT] = {
	[I2C_FREQ_1000KHZ] = 16, /* Enough for 2 bytes */
	[I2C_FREQ_400KHZ] = 40,  /* Enough for 2 bytes */
	[I2C_FREQ_100KHZ] = 0,   /* No busy looping at 100kHz (bus is slow) */
};
#endif

/*
 * The following timing config values are given in Table 371 of TM0440 which
 * assumes an I2CCLK freq of 16 MHZ. I2CCLK is connected to HSI, with is @
 * 16MHz. The TRM reccommends using the STM32CubeMX tool to get more accurate
 * values. Note that the acutal clock period is (scll + 1) + (schl + 1) +
 * internal detection delays for SCL being low/high.
 */
const struct i2c_timing i2c_timingr[I2C_FREQ_COUNT] = {
	[I2C_FREQ_1000KHZ] = {
		.scll = 4,
		.sclh = 2,
		.sdadel = 0,
		.scldel = 2,
		.presc = 0,
	},
		[I2C_FREQ_400KHZ] = {
		.scll = 9,
		.sclh = 4,
		.sdadel = 2,
		.scldel = 3,
		.presc = 1,
	},
		[I2C_FREQ_100KHZ] = {
		.scll = 19,
		.sclh = 15,
		.sdadel = 2,
		.scldel = 4,
		.presc = 3,
	},
};

/*
 * For G4, I2C1 and I2C2 are continguous in address space, but I2C3 and I2C4 are
 * at different offsets. In order to make the driver code easier, the base
 * address for each port's register block is defined here and can be used in i2c
 * register read/write accesses.
 */
static const uint32_t i2c_regs_base[] = {
	STM32_I2C1_BASE,
	STM32_I2C2_BASE,
	STM32_I2C3_BASE,
	STM32_I2C4_BASE,
};

/* I2C port state data */
struct i2c_port_data {
	uint32_t timeout_us;    /* Transaction timeout, or 0 to use default */
	enum i2c_freq freq;	/* Port clock speed */
};
static struct i2c_port_data pdata[I2C_PORT_COUNT];

void i2c_set_timeout(int port, uint32_t timeout)
{
	pdata[port].timeout_us = timeout ? timeout : I2C_TX_TIMEOUT_MASTER;
}

/**
 * Set i2c clock rate..
 *
 * @param p		I2C port struct
 */
static void i2c_set_timingr_port(const struct i2c_port_t *p)
{
	int port = p->port;
	uint32_t base;
	int index;
	uint32_t timingr;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];

	/* Disable port */
	STM32_I2C_CR1(base) = 0;
	STM32_I2C_CR2(base) = 0;
	/*
	 * To configure an I2C port frequency requires 5 values. scll, sclh,
	 * sdadel, scldel, and presc. With these settings, the acutal SCL period
	 * (Tscl) is given by:
	 *     Tscl = Tsync1 + Tsync2 + [(scll+1) + (sclh+1)] * presc * Ti2clck
	 *
	 * Using HSI for i2cclk, so this is fixed @ 16MHz. The recommended
	 * values for the 5 parameters are from the TRM for i2clck @ 16 MHZ.
	 * Note that Tsyncx is a function of SCL rise/fall times and filtering
	 * selected for the given I2C port. sdadel and scldel affect when data
	 * is written or read relative to SCL edges.
	 */

	if (p->kbps == freq_100) {
		index = I2C_FREQ_100KHZ;
	} else if (p->kbps == freq_400) {
		index = I2C_FREQ_400KHZ;
	} else if (p->kbps == freq_400) {
		index = I2C_FREQ_1000KHZ;
	} else {
		index = I2C_FREQ_100KHZ;
		CPRINTS("stm32 i2c[p%d]: Invalid freq, setting 100Khz instead!",
			port);
	}
	/* Assemble write value for timingr register */
	timingr = (i2c_timingr[index].scll << STM32_I2C_TIMINGR_SCLL_OFF) |
		(i2c_timingr[index].sclh << STM32_I2C_TIMINGR_SCLH_OFF) |
		(i2c_timingr[index].sdadel << STM32_I2C_TIMINGR_SDADEL_OFF) |
		(i2c_timingr[index].scldel << STM32_I2C_TIMINGR_SCLDEL_OFF) |
		(i2c_timingr[index].presc << STM32_I2C_TIMINGR_PRESC_OFF);

	/* Write timingr value */
	STM32_I2C_TIMINGR(base) = timingr;

	/* Save freq lookup index for polling loop delay */
	pdata[port].freq = index;
}

/**
 * Initialize on the specified I2C port.
 *
 * @param p		the I2c port
 */
static void i2c_init_port(const struct i2c_port_t *p)
{
	int port = p->port;
	uint32_t base;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];
	CPRINTS("i2c: initializing port %d", port);

	/*
	 * The I2C module clock can be derived from sysclk, hsi16, or pclk1.
	 * CrosEC will typically have sysclk = pclk = cpuclk. hsi16 is fixed at
	 * 16 Mhz and given that it's a known freq, the timing register values
	 * can be obtained via table lookup. The I2C clock source is selected
	 * via the I2CnSEL field for a given I2C port.
	 *
	 * I2CnSEL is a 2 bit field in same register for ports 0-2 (1-3 in
	 * STM32 notation), but is in a different register for port 3.
	 */
	if (port < 3) {
		int clksel;
		int mask;
		int shift;

		shift = STM32_RCC_CCIPR_I2C1SEL_SHIFT + 2 * port;
		mask = STM32_RCC_CCIPR_I2CNSEL_MASK << shift;
		clksel = STM32_RCC_CCIPR;
		clksel &= ~mask;
		STM32_RCC_CCIPR = clksel | (STM32_RCC_CCIPR_I2CNSEL_HSI
					    << shift);
	} else if (port == 3) {
		/* i2c4sel is bits 1:0, no shift required */
		STM32_RCC_CCIPR2 &= ~STM32_RCC_CCIPR2_I2C4SEL_MASK;
		STM32_RCC_CCIPR2 |= STM32_RCC_CCIPR_I2CNSEL_HSI;
	}
	/* Configure GPIO alt-func for this port */
	gpio_config_module(MODULE_I2C, 1);
	/* Enable the I2C clock for a given I2C port */
	clock_enable_module(MODULE_I2C, 1);

	/*
	 * Software reset for an I2C port is performed by cleaing the PE bit in
	 * that port's CR1 register. When this happens, SCL and SDA are
	 * released, internal stame machines are reset, control/status bits are
	 * also reset. The I2C block reset requires 3 APB cycles before setting
	 * PE back to 1. This wait is ensured by the call fo i2c_set_freq_port.
	 */
	STM32_I2C_CR1(base) &= ~STM32_I2C_CR1_PE;
	/* Set up initial bus frequencies */
	i2c_set_timingr_port(p);
	/* Enable the I2C port */
	STM32_I2C_CR1(base) |= STM32_I2C_CR1_PE;

	/* Set up default timeout */
	i2c_set_timeout(port, 0);
}

/*****************************************************************************/
#if !I2C_WAIT_ISR_NORMAL
struct i2c_log {
	uint32_t ts;
	uint32_t cr2;
	uint32_t isr;
	uint32_t group;
};
#define I2C_REG_READS 100
static int log_cnt;
static struct i2c_log logs[I2C_REG_READS];

static void add_log(int port, int group)
{
	uint32_t base;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];

	if (log_cnt < I2C_REG_READS){
		logs[log_cnt].ts = __hw_clock_source_read();
		logs[log_cnt].cr2 = STM32_I2C_CR2(base);
		logs[log_cnt].isr = STM32_I2C_ISR(base);
		logs[log_cnt].group = group;
		log_cnt++;
	}
}
#endif

/* Interface */
#if I2C_WAIT_ISR_NORMAL
/**
 * Wait for ISR register to contain the specified mask.
 *
 * Returns EC_SUCCESS, EC_ERROR_TIMEOUT if timed out waiting, or
 * EC_ERROR_UNKNOWN if an error bit appeared in the status register.
 */
static int wait_isr(int port, int mask)
{
	uint32_t start = __hw_clock_source_read();
	uint32_t delta = 0;
	uint32_t base;
	int count = 0;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];

	do {
		int isr = STM32_I2C_ISR(base);

		if ((mask == STM32_I2C_ISR_TC) && (count < 5))
			CPRINTS("isr_wait[%d]: t = %d, isr = %x", port,
				__hw_clock_source_read(), isr);

		/* Check for errors */
		if (isr & (STM32_I2C_ISR_ARLO | STM32_I2C_ISR_BERR |
			   STM32_I2C_ISR_NACK)) {
			return EC_ERROR_UNKNOWN;
		}

		/* Check for desired mask */
		if ((isr & mask) == mask) {
			return EC_SUCCESS;
		}

		delta = __hw_clock_source_read() - start;

		/**
		 * Depending on the bus speed, busy loop for a while before
		 * sleeping and letting other things run.
		 */
		if (delta >= busyloop_us[pdata[port].freq])
			usleep(100);
		count++;
	} while (delta < pdata[port].timeout_us);

	return EC_ERROR_TIMEOUT;
}
#else

static int wait_isr(int port, int mask, int group)
{
	uint32_t start = __hw_clock_source_read();
	uint32_t delta = 0;
	uint32_t base;
	int count = 0;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];

	gpio_set_level(GPIO_TP41, 1);

	do {
		int isr = STM32_I2C_ISR(base);

		add_log(port, group);

		/* Check for errors */
		if (isr & (STM32_I2C_ISR_ARLO | STM32_I2C_ISR_BERR |
			   STM32_I2C_ISR_NACK)) {
			gpio_set_level(GPIO_TP41, 0);
			return EC_ERROR_UNKNOWN;
		}

		/* Check for desired mask */
		if ((isr & mask) == mask) {
			gpio_set_level(GPIO_TP41, 0);
			return EC_SUCCESS;
		}

		delta = __hw_clock_source_read() - start;

		count++;

	} while (delta < pdata[port].timeout_us);
	gpio_set_level(GPIO_TP41, 0);

	return EC_ERROR_TIMEOUT;
}
#endif

/*****************************************************************************
 * Exported functions declared in i2c.h
 */
/* Perform an i2c transaction. */
int chip_i2c_xfer(const int port, const uint16_t slave_addr_flags,
		  const uint8_t *out, int out_bytes,
		  uint8_t *in, int in_bytes, int flags)
{
	int addr_8bit = I2C_GET_ADDR(slave_addr_flags) << 1;
	int rv = EC_SUCCESS;
	int i;
	int xfer_start = flags & I2C_XFER_START;
	int xfer_stop = flags & I2C_XFER_STOP;
	int cr2_reg;
	uint32_t base;

	ASSERT(port < I2C_PORT_COUNT);
	base = i2c_regs_base[port];

	ASSERT(out || !out_bytes);
	ASSERT(in || !in_bytes);

	add_log(port, 0);
	/* Clear status */
	if (xfer_start) {
		STM32_I2C_ICR(base) = STM32_I2C_ICR_ALL;
		STM32_I2C_CR2(base) = 0;
		STM32_I2C_CR1(base) &= ~STM32_I2C_CR1_PE;
		udelay(10);
		STM32_I2C_CR1(base) |= STM32_I2C_CR1_PE;
		log_cnt = 0;

		i2c_set_timeout(port, 50);
	}
	add_log(port, 1);

	if (out_bytes || !in_bytes) {
		/*
		 * Configure the write transfer: if we are stopping then set
		 * AUTOEND bit to automatically set STOP bit after NBYTES.
		 * if we are not stopping, set RELOAD bit so that we can load
		 * NBYTES again. if we are starting, then set START bit.
		 */
		cr2_reg =  ((out_bytes & 0xFF) << 16)
			| addr_8bit
			| ((in_bytes == 0 && xfer_stop) ?
				STM32_I2C_CR2_AUTOEND : 0)
			| ((in_bytes == 0 && !xfer_stop) ?
				STM32_I2C_CR2_RELOAD : 0)
			| (xfer_start ? STM32_I2C_CR2_START : 0);
		CPRINTS("i2c[%d]: Starting i2C write, CR2 = %x", port, cr2_reg);
		add_log(port, 1);
		STM32_I2C_CR2(base) = cr2_reg;
		add_log(port, 2);
		for (i = 0; i < out_bytes; i++) {
			rv = wait_isr(port, STM32_I2C_ISR_TXIS, 3);
			if (rv)
				goto xfer_exit;
			/* Write next data byte */
			STM32_I2C_TXDR(base) = out[i];
		}
	}

	if (in_bytes) {
		if (out_bytes) { /* wait for completion of the write */
			rv = wait_isr(port, STM32_I2C_ISR_TC, 4);
			if (rv)
				goto xfer_exit;
		}
		/*
		 * Configure the read transfer: if we are stopping then set
		 * AUTOEND bit to automatically set STOP bit after NBYTES.
		 * if we are not stopping, set RELOAD bit so that we can load
		 * NBYTES again. if we were just transmitting, we need to
		 * set START bit to send (re)start and begin read transaction.
		 */
		STM32_I2C_CR2(base) = ((in_bytes & 0xFF) << 16)
			| STM32_I2C_CR2_RD_WRN | addr_8bit
			| (xfer_stop ? STM32_I2C_CR2_AUTOEND : 0)
			| (!xfer_stop ? STM32_I2C_CR2_RELOAD : 0)
			| (out_bytes || xfer_start ? STM32_I2C_CR2_START : 0);

		for (i = 0; i < in_bytes; i++) {
			/* Wait for receive buffer not empty */
			rv = wait_isr(port, STM32_I2C_ISR_RXNE, 5);
			//	CPRINTS("i2c[%d]: rx'd byte %d", port, i);
			if (rv)
				goto xfer_exit;
			in[i] = STM32_I2C_RXDR(base);
		}
	}

	/*
	 * If we are stopping, then we already set AUTOEND and we should
	 * wait for the stop bit to be transmitted. Otherwise, we set
	 * the RELOAD bit and we should wait for transfer complete
	 * reload (TCR).
	 */
	rv = wait_isr(port, xfer_stop ? STM32_I2C_ISR_STOP : STM32_I2C_ISR_TCR, 6);
	if (rv)
		goto xfer_exit;

xfer_exit:
	/* clear status */
	if (xfer_stop)
		STM32_I2C_ICR(base) = STM32_I2C_ICR_ALL;

	/* On error, queue a stop condition */
	if (rv) {
		//CPRINTS("i2c[%d]: error detected", port);
		gpio_set_level(GPIO_TP41, 1);
		/* queue a STOP condition */
		STM32_I2C_CR2(base) |= STM32_I2C_CR2_STOP;
		/* wait for it to take effect */
		/* Wait up to 100 us for bus idle */

		for (i = 0; i < 15; i++) {
			add_log(port, 7);
			if (!(STM32_I2C_ISR(base) & STM32_I2C_ISR_BUSY)) {
				CPRINTS("i2c[%d]: busy bit is clear", port);
				break;
			}
			udelay(2);
		}

		/*
		 * Allow bus to idle for at least one 100KHz clock = 10 us.
		 * This allows slaves on the bus to detect bus-idle before
		 * the next start condition.
		 */
		gpio_set_level(GPIO_TP41, 0);
		udelay(10);
		/* re-initialize the controller */
		STM32_I2C_CR2(base) = 0;
		STM32_I2C_CR1(base) &= ~STM32_I2C_CR1_PE;
		gpio_set_level(GPIO_TP41, 1);
		udelay(10);
		STM32_I2C_CR1(base) |= STM32_I2C_CR1_PE;
		gpio_set_level(GPIO_TP41, 0);
	}

	return rv;
}

int i2c_raw_get_scl(int port)
{
	enum gpio_signal g;

	if (get_scl_from_i2c_port(port, &g) == EC_SUCCESS)
		return gpio_get_level(g);

	/* If no SCL pin defined for this port, then return 1 to appear idle. */
	return 1;
}

int i2c_raw_get_sda(int port)
{
	enum gpio_signal g;

	if (get_sda_from_i2c_port(port, &g) == EC_SUCCESS)
		return gpio_get_level(g);

	/* If no SDA pin defined for this port, then return 1 to appear idle. */
	return 1;
}

int i2c_get_line_levels(int port)
{
	return (i2c_raw_get_sda(port) ? I2C_LINE_SDA_HIGH : 0) |
		(i2c_raw_get_scl(port) ? I2C_LINE_SCL_HIGH : 0);
}

/*****************************************************************************/
/* Hooks */

#ifdef CONFIG_I2C_MASTER
/* Handle CPU clock changing frequency */
static void i2c_freq_change(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	for (i = 0; i < i2c_ports_used; i++, p++)
		i2c_set_timingr_port(p);
}

/* Handle an upcoming frequency change. */
static void i2c_pre_freq_change_hook(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	/* Lock I2C ports so freq change can't interrupt an I2C transaction */
	for (i = 0; i < i2c_ports_used; i++, p++)
		i2c_lock(p->port, 1);
}
DECLARE_HOOK(HOOK_PRE_FREQ_CHANGE, i2c_pre_freq_change_hook, HOOK_PRIO_DEFAULT);

/* Handle a frequency change */
static void i2c_freq_change_hook(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	i2c_freq_change();

	/* Unlock I2C ports we locked in pre-freq change hook */
	for (i = 0; i < i2c_ports_used; i++, p++)
		i2c_lock(p->port, 0);
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, i2c_freq_change_hook, HOOK_PRIO_DEFAULT);
#endif

/*****************************************************************************/

/* Init all available i2c ports */
void i2c_init(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	for (i = 0; i < i2c_ports_used; i++, p++)
		i2c_init_port(p);
}

static int console_stm(int argc, char **argv)
{
	int i;

	ccprintf("log entries = %d\n", log_cnt);
	ccprintf("Idx\t t_usec \t CR2 \t\tISR\t\tGroup\n");
	for (i = 0; i < log_cnt; i++) {
		ccprintf("[%02d]:\t%08d\t%08x\t%04x\t%d\n", i,
			 logs[i].ts, logs[i].cr2, logs[i].isr, logs[i].group);
		msleep(10);
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(stm, console_stm,
			"<nothing>",
			"start qsi test");
