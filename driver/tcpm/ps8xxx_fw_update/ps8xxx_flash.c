/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Type-C port manager for Parade PS8XXX with integrated superspeed muxes.
 *
 * Supported TCPCs:
 * - PS8705
 * - PS8751
 * - PS8755
 * - PS8805
 * - PS8815
 */

#include "common.h"
#include "console.h"
#include "driver/tcpm/ps8xxx_fw_update/ps8xxx_images/ps8805/ps8805_a2_testing_20210823.h"
#include "ps8xxx.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"

#define PS8751_DEBUG	0

#define PAGE_0		0
#define PAGE_1		1
#define PAGE_2		2
#define PAGE_3		3 /* primary TCPCI registers */

#define PS8751_P1_SPI_WP	0x4b
/* NOTE: on 8751 A3 silicon, P1_SPI_WP_EN reads back inverted! */
#define PS8751_P1_SPI_WP_EN	0x10	/* WP enable bit */
#define PS8751_P1_SPI_WP_DIS	0x00	/* WP disable "bit" */

#define PS8805_P2_SPI_WP	0x2a
#define PS8805_P2_SPI_WP_EN	0x10	/* WP enable bit */
#define PS8805_P2_SPI_WP_DIS	0x00	/* WP disable "bit" */

#define PS8815_P2_SPI_WP	0x2a
#define PS8815_P2_SPI_WP_EN	0x00	/* WP enable "bit" */
#define PS8815_P2_SPI_WP_DIS	0x10	/* WP disable bit */

#define P0_ROM_CTRL		0xef
#define P0_ROM_CTRL_LOAD_DONE	0xc0	/* MTP load done */

#define P1_CHIP_REV_LO		0xf0	/* the 0x03 in "A3" */
#define P1_CHIP_REV_HI		0xf1	/* the 0x0a in "A3" */
#define P1_CHIP_ID_LO		0xf2	/* 0x50 */
#define P1_CHIP_ID_HI		0xf3	/* 0x87 */

#define P2_ALERT_LOW		0x10
#define P2_ALERT_HIGH		0x11
#define P2_WR_FIFO		0x90
#define P2_RD_FIFO		0x91
#define P2_SPI_LEN		0x92
#define P2_SPI_CTRL		0x93
#define P2_SPI_CTRL_NOREAD	0x04
#define P2_SPI_CTRL_FIFO_RESET	0x02
#define P2_SPI_CTRL_TRIGGER	0x01
#define P2_SPI_STATUS		0x9e
#define P2_CLK_CTRL		0xd6

#define P3_VENDOR_ID_LOW	0x00
#define P3_CHIP_WAKEUP		0xa0

#define PS8751_P3_I2C_DEBUG		0xa0
#define PS8751_P3_I2C_DEBUG_DEFAULT	0x31
#define PS8751_P3_I2C_DEBUG_ENABLE	0x30
#define PS8751_P3_I2C_DEBUG_DISABLE	PS8751_P3_I2C_DEBUG_DEFAULT

#define PS8815_P3_I2C_DEBUG		0x9b
#define PS8815_P3_I2C_DEBUG_DEFAULT	0x00
#define PS8815_P3_I2C_DEBUG_ENABLE	0x01
#define PS8815_P3_I2C_DEBUG_DISABLE	0x02

/*
 * bytes of SPI FIFO depth after command overhead
 */

#define PS_FW_RD_CHUNK		16
#define PS_FW_WR_CHUNK		12

#define SPI_CMD_WRITE_STATUS_REG	0x01
#define SPI_CMD_PROG_PAGE		0x02
#define SPI_CMD_READ_DATA		0x03
#define SPI_CMD_WRITE_DISABLE		0x04
#define SPI_CMD_READ_STATUS_REG		0x05
#define SPI_CMD_WRITE_ENABLE		0x06

/*
 * EN25F20:
 *	  64 x   4KB erase sectors
 *	   4 x  64KB erase blocks
 *	1024 x 256B  write pages
 */

#define SPI_CMD_ERASE_SECTOR	0x20		/* sector erase, 4KB */
#define SPI_CMD_READ_DEVICE_ID	0x90
#define SPI_PAGE_SIZE		(1 << 8)	/* 256B, always 2^n */
#define SPI_PAGE_MASK		(SPI_PAGE_SIZE - 1)

#define SPI_STATUS_WIP		0x01
#define SPI_STATUS_WEL		0x02
#define SPI_STATUS_BP		0x0c
#define SPI_STATUS_SRP		0x80

/*
 * period of issuing reads on the primary I2C page to keep the chip awake
 * 1 sec is OK
 * 2 secs is too long
 * chip goes to sleep when I2C is idle for 2 secs
 */

#define USEC_TO_SEC(us)		((us) / 1000000)
#define USEC_TO_MSEC(us)	((us) / 1000)
#define PS_REFRESH_INTERVAL_US	(500 * 1000)		/* 500 ms */
#define PS_SPI_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */
#define PS_WIP_TIMEOUT_US	(1 * 1000 * 1000)	/* 1s */
#define PS_MPU_BOOT_DELAY_MS	(50)
#define PS_RESTART_DELAY_CS	(6)			/* 6cs / 60 ms */

#define PARADE_BINVERSION_OFFSET	0x501c
#define PARADE_CHIPVERSION_OFFSET	0x503a

#if (PS8751_DEBUG >= 2)
#define PARADE_FW_START		0x38000
#else
#define PARADE_FW_START		0x30000
#endif
#define PARADE_FW_END		0x40000
#define PARADE_FW_SECTOR	 0x1000		/* erase sector size */

#define PARADE_TEST_FW_SIZE	0x1000

#define PARADE_VENDOR_ID		0x1DA0
#define PARADE_PS8751_PRODUCT_ID	0x8751
#define PARADE_PS8755_PRODUCT_ID	0x8755
#define PARADE_PS8705_PRODUCT_ID	0x8705
/* TODO(b/171422252): Remove the default device ID */
#define PARADE_PS8705_DEFAULT_DEVICE_ID	0x0001
#define PARADE_PS8705_A2_DEVICE_ID	0x0004
#define PARADE_PS8705_A3_DEVICE_ID	0x0005
#define PARADE_PS8805_BROKEN_PRODUCT_ID	0x8803
#define PARADE_PS8805_PRODUCT_ID	0x8805
#define PARADE_PS8815_PRODUCT_ID	0x8815
#define PARADE_PS8815_A0_DEVICE_ID	0x0001
#define PARADE_PS8815_A1_DEVICE_ID	0x0002
#define PARADE_PS8815_A2_DEVICE_ID	0x0003


struct i2c_write_vec
{
	uint8_t	reg;
	uint8_t	val;
};

enum ps8751_device_state {
	PS8751_DEVICE_MISSING = -2,
	PS8751_DEVICE_NOT_PARADE = -1,
	PS8751_DEVICE_PRESENT = 0,
};

enum parade_chip_type {
	CHIP_PS8751,
	CHIP_PS8755,
	CHIP_PS8805,
	CHIP_PS8815,
	CHIP_PS8705,
};

struct Ps8751
{
	int ec_pd_id;
	int port;

	/* these are cached from chip regs */
	struct {
		uint16_t vendor;
		uint16_t product;
		uint16_t device;
		uint8_t fw_rev;
	} chip;
	uint8_t blob_hw_version;
	enum parade_chip_type chip_type;
	char chip_name[16];
};

struct Ps8751 tcpc_info[CONFIG_USB_PD_PORT_MAX_COUNT];

static const uint8_t erased_bytes[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
_Static_assert(sizeof(erased_bytes) == PS_FW_RD_CHUNK,
	       "erased_bytes initializer size mismatch");

static uint64_t timer_us(uint64_t start_time)
{
	return (get_time().val - start_time);
}

static int write_reg(int port, int page, uint8_t reg,
		       int val)
{
	int rv;

	rv = i2c_write8(tcpc_config[port].i2c_info.port,
			tcpc_config[port].i2c_info.addr_flags - (PAGE_3 - page),
			reg, val);

	return rv;
}

static int read_reg(int port, int page, uint8_t reg,
		       int *val)
{
	int rv;

	rv = i2c_read8(tcpc_config[port].i2c_info.port,
		       tcpc_config[port].i2c_info.addr_flags - (PAGE_3 - page),
		       reg, val);

	return rv;
}


static int write_regs(int port, uint8_t page,
		      const struct i2c_write_vec *cmds,const size_t count)
{
	int i;
	int rv;

	for (i = 0; i < count; i++) {
		rv = write_reg(port, page, cmds->reg, cmds->val);
		if (rv)
			return rv;
		cmds++;
	}
	return EC_SUCCESS;
}


/**
 * wait for SPI interface FIFOs to become ready
 * this means the requested command bytes have been written and
 * result bytes have been captured
 *
 * SPI bus timeout can happen if the SPI CLK isn't
 * running as expected.
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_fifo_wait_busy(struct Ps8751 *me)
{
	int status;
	uint64_t t0_us;

	t0_us = get_time().val;
	do {
		if (read_reg(me->port, PAGE_2, P2_SPI_CTRL, &status))
			return EC_ERROR_UNKNOWN;
		if (timer_us(t0_us) >= PS_SPI_TIMEOUT_US) {
			ccprintf("%s: SPI bus timeout after %ums\n",
			       me->chip_name, USEC_TO_MSEC(PS_SPI_TIMEOUT_US));
			return EC_ERROR_UNKNOWN;
		}
	} while (status & P2_SPI_CTRL_TRIGGER);
	return EC_SUCCESS;
}

/**
 * reset the SPI interface FIFOs
 *
 * @param me device context @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_fifo_reset(struct Ps8751 *me)
{
	if (write_reg(me->port, PAGE_2, P2_SPI_CTRL, P2_SPI_CTRL_FIFO_RESET))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * wake up the chip and enable all the i2c targets (i.e. "pages")
 * - also reset SPI interface FIFOs for good measure
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_wake_i2c(struct Ps8751 *me)
{
	int status;
	int dummy;
	uint8_t debug_reg;
	uint8_t debug_ena;

	ccprintf("call...\n");

	switch (me->chip_type) {
	case CHIP_PS8751:
	case CHIP_PS8755:
	case CHIP_PS8705:
	case CHIP_PS8805:
		debug_reg = PS8751_P3_I2C_DEBUG;
		debug_ena = PS8751_P3_I2C_DEBUG_ENABLE;
		break;
	case CHIP_PS8815:
		debug_reg = PS8815_P3_I2C_DEBUG;
		debug_ena = PS8815_P3_I2C_DEBUG_ENABLE;
		break;
	default:
		ccprintf("Unknown Parade chip_type: %d\n", me->chip_type);
		return EC_ERROR_UNKNOWN;
	}

	status = read_reg(me->port, PAGE_3, P3_CHIP_WAKEUP, &dummy);
	if (status != 0) {
		/* wait for device to wake up... */
		udelay(10 * MSEC);
	}

	/*
	 * this enables 7 additional i2c chip addrs:
	 * 0x10, 0x12, 0x14, 0x18, 0x1a, 0x1c, 0x1f
	 */
	status = write_reg(me->port, PAGE_3, debug_reg, debug_ena);
	if (status == 0)
		status = ps8xxx_spi_fifo_reset(me);
	if (status != 0)
		ccprintf("%s: chip did not wake up!\n", me->chip_name);
	return status;
}

/**
 * turn off extra i2c targets (pages)
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

__maybe_unused static int ps8xxx_hide_i2c(struct Ps8751 *me)
{
	int status;
	int dummy;
	uint8_t debug_reg;
	uint8_t debug_dis;

	switch (me->chip_type) {
	case CHIP_PS8751:
	case CHIP_PS8755:
	case CHIP_PS8705:
	case CHIP_PS8805:
		debug_reg = PS8751_P3_I2C_DEBUG;
		debug_dis = PS8751_P3_I2C_DEBUG_DEFAULT;
		break;
	case CHIP_PS8815:
		debug_reg = PS8815_P3_I2C_DEBUG;
		debug_dis = PS8815_P3_I2C_DEBUG_DEFAULT;
		break;
	default:
		ccprintf("Unknown Parade chip_type: %d\n", me->chip_type);
		return EC_ERROR_UNKNOWN;
	}

	/* make sure chip is awake (is this needed?) */
	status = read_reg(me->port, PAGE_3, P3_CHIP_WAKEUP, &dummy);
	if (status != 0) {
		/* wait for device to wake up... */
		udelay(10 * MSEC);
	}
	status = write_reg(me->port, PAGE_3, debug_reg, debug_dis);
	return status;
}

/**
 * clear the ps8751 TCPCI alerts.  after the MPU has been stopped, the
 * TCPCI regs on page 0 (0x10) need to be used instead of page 3
 * (0x16).
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_clear_alerts(struct Ps8751 *me)
{
	const struct i2c_write_vec am[] = {
		{ P2_ALERT_LOW, 0xff },
		{ P2_ALERT_HIGH, 0xff },
	};
	/* yes, page 0 */
	return write_regs(me->port, PAGE_0, am, ARRAY_SIZE(am));
}

/*
 * protect internal registers
 *
 * this is a special step suggested by parade to protect
 * some internal SPI registers to improve the odds of
 * reflashing a chip with bad firmware.
 */

static int ps8xxx_rom_ctrl(struct Ps8751 *me)
{
	if (me->chip_type == CHIP_PS8751) {
		/* the ps8751 does not support this register */
		return EC_SUCCESS;
	}

	/* mark MTP load done */
	if (write_reg(me->port, PAGE_0, P0_ROM_CTRL, P0_ROM_CTRL_LOAD_DONE))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/*
 * stop the MPU, reset SPI clock
 *
 * if the MPU isn't running properly (i.e. corrupted flash),
 * we need to disable both clocks, then re-enable the SPI clock
 * to be able to access the SPI FIFO to recover the chip.
 *
 * ? disabling the MPU may keep the chip awake!
 */

static int ps8xxx_disable_mpu(struct Ps8751 *me)
{
	/* turn off SPI|MPU clocks */
	if (write_reg(me->port, PAGE_2, P2_CLK_CTRL, 0xc0))
		return EC_ERROR_UNKNOWN;
	/* SPI clock on, MPU clock stays off */
	if (write_reg(me->port, PAGE_2, P2_CLK_CTRL, 0x40))
		return EC_ERROR_UNKNOWN;
	/* clear residual alerts */
	if (ps8xxx_clear_alerts(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * re-enable the MPU clock and reboot it
 *
 * SPI clock stays on
 * we tune MUX_DP_EQ_CONFIGURATION when initializing the chip from the ec
 * parade confirmed this tuning is preserved across this reboot
 *
 * the chip needs about 50ms to come out of reset, so we'll assume it
 * takes a similar amount of time for the MPU to boot up.
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_enable_mpu(struct Ps8751 *me)
{
	/* SPI|MPU clk on */
	if (write_reg(me->port, PAGE_2, P2_CLK_CTRL, 0x00))
		return EC_ERROR_UNKNOWN;
	udelay(PS_MPU_BOOT_DELAY_MS * MSEC);
	if (ps8xxx_wake_i2c(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * send a SPI write-enable cmd
 *
 * WRITE_ENABLE must be issued before every
 * PROGRAM, ERASE, WRITE_STATUS_REGISTER command
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_cmd_enable_writes(struct Ps8751 *me)
{
	static const struct i2c_write_vec we[] = {
		{ P2_WR_FIFO, SPI_CMD_WRITE_ENABLE },
		{ P2_SPI_LEN, 0x00 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};

	if (write_regs(me->port, PAGE_2, we, ARRAY_SIZE(we)))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_fifo_wait_busy(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/*
 * wait for "erase/program command finished" as seen by the ps8751
 */

static int ps8xxx_spi_wait_prog_cmd(struct Ps8751 *me)
{
	int busy;
	uint64_t t0_us;

	t0_us = get_time().val;
	do {
		if (read_reg(me->port, PAGE_2, P2_SPI_STATUS, &busy))
			return EC_ERROR_UNKNOWN;
		if ((busy & 0x3f) == 0x00) {
			/* {chip,sector} erase, program cmd finished */
			return EC_SUCCESS;
		}
	} while (timer_us(t0_us) < PS_WIP_TIMEOUT_US);
	ccprintf("%s: flash prog/erase timeout after %ums\n",
	       me->chip_name, USEC_TO_MSEC(PS_SPI_TIMEOUT_US));
	return EC_ERROR_UNKNOWN;
}

/**
 * read SPI flash status register
 *
 * @param me	device context
 * @return status if ok, -1 on error
 */

static int ps8xxx_spi_cmd_read_status(struct Ps8751 *me, int *status)
{
	static const struct i2c_write_vec rs[] = {
		{ P2_WR_FIFO, SPI_CMD_READ_STATUS_REG },
		{ P2_SPI_LEN, 0x00 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
	};

	if (write_regs(me->port, PAGE_2, rs, ARRAY_SIZE(rs)))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_fifo_wait_busy(me))
		return EC_ERROR_UNKNOWN;
	if (read_reg(me->port, PAGE_2, P2_RD_FIFO, status))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * wait for SPI flash status WIP (write-in-progress) bit to clear
 * this is needed after write-status-reg, any prog, any erase command
 *
 * @param me	device context
 * @return status if ok, -1 on error
 */

static int ps8xxx_spi_wait_wip(struct Ps8751 *me)
{
	int status;
	uint64_t t0_us;

	t0_us = get_time().val;
	do {
		if (ps8xxx_spi_cmd_read_status(me, &status))
			return EC_ERROR_UNKNOWN;
		if (timer_us(t0_us) >= PS_WIP_TIMEOUT_US) {
			ccprintf("%s: WIP timeout after %ums\n",
			       me->chip_name, USEC_TO_MSEC(PS_WIP_TIMEOUT_US));
			return EC_ERROR_UNKNOWN;
		}
	} while (status & SPI_STATUS_WIP);
	return EC_SUCCESS;
}

/**
 * write SPI flash status register
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_cmd_write_status(struct Ps8751 *me, uint8_t val)
{
	const struct i2c_write_vec ws[] = {
		{ P2_WR_FIFO, SPI_CMD_WRITE_STATUS_REG },
		{ P2_WR_FIFO, val },
		{ P2_SPI_LEN, 0x01 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};

	if (ps8xxx_spi_cmd_enable_writes(me) < 0)
		return EC_ERROR_UNKNOWN;

	if (write_regs(me->port, PAGE_2, ws, ARRAY_SIZE(ws)))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_fifo_wait_busy(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_wait_wip(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/*
 * lock the SPI flash
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 *
 * NOTE: keep in sync with ps8xxx_spi_flash_unlock()
 */

static int ps8xxx_spi_flash_lock(struct Ps8751 *me)
{
	int status = 0;
	uint8_t page;
	uint8_t wp_reg;
	uint8_t wp_en;

	if (ps8xxx_spi_cmd_write_status(me, SPI_STATUS_SRP|SPI_STATUS_BP))
		status = -1;

	switch (me->chip_type) {
	case CHIP_PS8751:
		page = PAGE_1;
		wp_reg = PS8751_P1_SPI_WP;
		wp_en = PS8751_P1_SPI_WP_EN;
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		page = PAGE_2;
		wp_reg = PS8805_P2_SPI_WP;
		wp_en = PS8805_P2_SPI_WP_EN;
		break;
	case CHIP_PS8815:
		page = PAGE_2;
		wp_reg = PS8815_P2_SPI_WP;
		wp_en = PS8815_P2_SPI_WP_EN;
		break;
	default:
		ccprintf("Unknown Parade chip_type: %d\n", me->chip_type);
		return EC_ERROR_UNKNOWN;
	}
	/* assert SPI flash WP# */
	if (write_reg(me->port, page, wp_reg, wp_en))
		status = -1;

	return status;
}

/*
 * reset SPI bus cmd FIFOs and unlock the SPI flash...
 *
 * @param me	device context
 *
 * NOTE: call ps8751_disable_mpu() before this so we have
 *	 a functional SPI bus.
 */

static int ps8xxx_spi_flash_unlock(struct Ps8751 *me)
{
	int status;
	uint8_t page;
	uint8_t wp_reg;
	uint8_t wp_dis;

	if (ps8xxx_spi_fifo_reset(me))
		return EC_ERROR_UNKNOWN;

	switch (me->chip_type) {
	case CHIP_PS8751:
		page = PAGE_1;
		wp_reg = PS8751_P1_SPI_WP;
		wp_dis = PS8751_P1_SPI_WP_DIS;
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		page = PAGE_2;
		wp_reg = PS8805_P2_SPI_WP;
		wp_dis = PS8805_P2_SPI_WP_DIS;
		break;
	case CHIP_PS8815:
		page = PAGE_2;
		wp_reg = PS8815_P2_SPI_WP;
		wp_dis = PS8815_P2_SPI_WP_DIS;
		break;
	default:
		ccprintf("Unknown Parade chip_type: %d\n", me->chip_type);
		return EC_ERROR_UNKNOWN;
	}
	/* deassert SPI flash WP# */
	if (write_reg(me->port, page, wp_reg, wp_dis))
		return EC_ERROR_UNKNOWN;

	/* clear the SRP, BP bits */
	if (ps8xxx_spi_cmd_write_status(me, 0x00))
		return EC_ERROR_UNKNOWN;

	if (ps8xxx_spi_cmd_read_status(me, &status))
		return EC_ERROR_UNKNOWN;

	if ((status & (SPI_STATUS_SRP|SPI_STATUS_BP))) {
		ccprintf("%s: could not clear flash status "
		       "SRP|BP (0x%02x)\n", me->chip_name, status);
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/*
 * wait for "erase/program command finished" as seen by the ps8751
 * then, wait for the WIP (write-in-progress) bit to clear on the
 * flash part itself.
 *
 * @param me	device context
 */

static int ps8xxx_spi_wait_rom_ready(struct Ps8751 *me)
{
	if (ps8xxx_spi_wait_prog_cmd(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_wait_wip(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * query the flash ID and see if we support it
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_flash_identify(struct Ps8751 *me)
{
	int high, low;
	uint16_t flash_id;

	static const struct i2c_write_vec read_id[] = {
		{ P2_WR_FIFO, SPI_CMD_READ_DEVICE_ID },
		{ P2_WR_FIFO, 0x00 },
		{ P2_WR_FIFO, 0x00 },
		{ P2_WR_FIFO, 0x00 },
		{ P2_SPI_LEN, 0x13 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
	};
	if (write_regs(me->port, PAGE_2, read_id, ARRAY_SIZE(read_id)))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_fifo_wait_busy(me))
		return EC_ERROR_UNKNOWN;

	if(read_reg(me->port, PAGE_2, P2_RD_FIFO, &high))
		return EC_ERROR_UNKNOWN;
	if(read_reg(me->port, PAGE_2, P2_RD_FIFO, &low))
		return EC_ERROR_UNKNOWN;

	flash_id = (high << 8) | low;

	ccprintf("%s: found SPI flash ID 0x%04x\n", me->chip_name, flash_id);

	/*
	 * these devices must use
	 * SPI_CMD_ERASE_SECTOR of 0x20 with a 4KB erase block
	 * list extracted from parade reference code
	 * (PS8751_PanelConfig.py)
	 */
	switch (flash_id) {
	case 0x1c11:		/* "EN25F20" */
	case 0x1c12:		/* "EN25F40" */
	case 0xc811:		/* "GD25Q20C" */
	case 0xbf43:		/* "25LF020A" */
	case 0xef11:		/* "W25X20" */
	case 0xef15:		/* "W25X32" */
	case 0xc211:		/* "25L2005" */
	case 0xc205:		/* "25L512" */
	case 0x1f44:		/* "25DF041A" */
	case 0x1f43:		/* "25DF021A" */
		break;
	default:
		ccprintf("%s: SPI flash ID 0x%04x not recognized\n",
		       me->chip_name, flash_id);
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/**
 * access the chip periodically so it doesn't fall asleep.  a simple
 * i2c read every second or so from PAGE_3 is sufficient.
 *
 * @param me		device context
 * @param deadline	pointer to time of next needed access
 *			use deadline of 0 for first call
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_keep_awake(struct Ps8751 *me, uint64_t *deadline)
{
	uint64_t now_us = get_time().val;
	int dummy;

	if (now_us >= *deadline) {
		int status = read_reg(me->port, PAGE_3, P3_CHIP_WAKEUP, &dummy);

		*deadline = now_us + PS_REFRESH_INTERVAL_US;

		if (status != 0) {
			ccprintf("%s: chip dozed off!\n", me->chip_name);
			return EC_ERROR_UNKNOWN;
		}
	}
	return EC_SUCCESS;
}

/**
 * get chip hardware version.  result of 0xa3 means it's an A3 chip.
 *
 * @param me		device context
 * @param version	pointer to result version byte
 * @return 0 if ok, -1 on error
 */

static int ps8751_get_hw_version(struct Ps8751 *me, uint8_t *version)
{
	int status;
	int low;
	int high;

	status = read_reg(me->port, PAGE_1, P1_CHIP_REV_LO, &low);
	if (status == 0)
		status = read_reg(me->port, PAGE_1, P1_CHIP_REV_HI, &high);
	if (status < 0) {
		ccprintf("%s: read P1_CHIP_REV_* failed\n", me->chip_name);
		return status;
	}
	*version = (high << 4) | low;

	read_reg(me->port, PAGE_3, 0x5, &high);
	read_reg(me->port, PAGE_3, 0x4, &low);
	ccprintf("ps8805: bcd: 0x%x, high = %x, low = %x\n",
		 (high << 4) | low, high, low);
	cflush();
	return EC_SUCCESS;
}

static int is_corrupted_tcpc(const struct ec_response_pd_chip_info_v1 *const info,
			     const enum parade_chip_type chip)
{
	switch (chip) {
	case CHIP_PS8751:
		return info->vendor_id == 0 && info->product_id == 0;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		return info->vendor_id == PARADE_VENDOR_ID &&
		       info->product_id == PARADE_PS8805_BROKEN_PRODUCT_ID;
	default:
		return EC_SUCCESS;
	}
}

static int is_parade_chip(const struct ec_response_pd_chip_info_v1 *const info,
			  const enum parade_chip_type chip)
{
	if (info->vendor_id != PARADE_VENDOR_ID)
		return EC_SUCCESS;

	switch (chip) {
	case CHIP_PS8751:
		return info->product_id == PARADE_PS8751_PRODUCT_ID;
	case CHIP_PS8755:
		return info->product_id == PARADE_PS8755_PRODUCT_ID;
	case CHIP_PS8705:
		return info->product_id == PARADE_PS8705_PRODUCT_ID;
	case CHIP_PS8805:
		return info->product_id == PARADE_PS8805_PRODUCT_ID;
	case CHIP_PS8815:
		return info->product_id == PARADE_PS8815_PRODUCT_ID;
	default:
		ccprintf("Unknown Parade product_id: 0x%x\n", info->product_id);
		return EC_SUCCESS;
	}
}

/**
 * capture chip (vendor, product, device, rev) IDs
 *
 * @param me	device context
 * @return 0 if ok, -1 on error
 */

static enum ps8751_device_state ps8xxx_capture_device_id(
							struct Ps8751 *me,
							int renew)
{
	struct ec_response_pd_chip_info_v1 info;
	uint16_t vendor;
	uint16_t product;
	uint16_t device;
	uint8_t fw_rev;

	if (me->chip.vendor != 0 && !renew)
		return PS8751_DEVICE_PRESENT;

	if (tcpm_get_chip_info(me->ec_pd_id, renew, &info))
		return EC_ERROR_UNKNOWN;

	vendor  = info.vendor_id;
	product = info.product_id;
	device  = info.device_id;
	fw_rev = info.fw_version_number;

	ccprintf("%s: vendor 0x%04x product 0x%04x "
	       "device 0x%04x fw_rev 0x%02x\n",
	       me->chip_name, vendor, product, device, fw_rev);
	if (is_corrupted_tcpc(&info, me->chip_type)) {
		ccprintf("%s: reports corruption (%4x:%4x)\n",
		       me->chip_name, vendor, product);
	} else if (!is_parade_chip(&info, me->chip_type)) {
		return PS8751_DEVICE_NOT_PARADE;
	}

	me->chip.vendor = vendor;
	me->chip.product = product;
	me->chip.device = device;
	me->chip.fw_rev = fw_rev;

	return PS8751_DEVICE_PRESENT;
}

/**
 * extract the chip version compatibility info from a firmware blob
 *
 * @fw_blob	firmware blob
 * @return chip version (e.g. 0xa3 is an A3 chip)
 */

static uint8_t ps8xxx_blob_hw_version(const uint8_t *fw_blob)
{
	char buf[3];
	char *e;

	buf[0] = fw_blob[PARADE_CHIPVERSION_OFFSET];
	buf[1] = fw_blob[PARADE_CHIPVERSION_OFFSET + 1];
	buf[2] = '\0';
	return strtoi(buf, &e, 16);
}

/**
 * determine if a firmware blob is compatible with the hardware
 *
 * @param me	device context
 * @param fw	proposed firmware blob
 * @return compatibility status (boolean)
 */

static int ps8xxx_is_fw_compatible(struct Ps8751 *me, const uint8_t *fw)
{
	uint8_t hw_rev;
	uint8_t fw_chip_version;

	if (ps8751_get_hw_version(me, &hw_rev) < 0)
		return EC_SUCCESS;

	ccprintf("ps8805: hw ver = 0x%x\n", hw_rev);
	switch (me->chip_type) {
	case CHIP_PS8751:
		fw_chip_version = ps8xxx_blob_hw_version(fw);
		break;
	case CHIP_PS8705:
	case CHIP_PS8755:
	case CHIP_PS8805:
		fw_chip_version = me->blob_hw_version;
		break;
	case CHIP_PS8815:
		fw_chip_version = me->blob_hw_version;
		break;
	default:
		ccprintf("Unknown Parade chip_type: %d\n", me->chip_type);
		return EC_SUCCESS;
	}
	if (hw_rev == fw_chip_version)
		return 1;
	ccprintf("%s: chip rev 0x%02x but firmware for 0x%02x\n",
	       me->chip_name, hw_rev, fw_chip_version);
	return EC_SUCCESS;
}

/**
 * write a SPI command plus a 24 bit addr to the SPI interface FIFO
 *
 * @param me	device context
 * @cmd		SPI command to issue
 * @param a24	SPI command address bits (24)
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_spi_setup_cmd24(struct Ps8751 *me,
					       uint8_t cmd, uint32_t a24)
{
	const struct i2c_write_vec sa[] = {
		{ P2_WR_FIFO, cmd },
		{ P2_WR_FIFO, a24 >> 16 },
		{ P2_WR_FIFO, a24 >>  8 },
		{ P2_WR_FIFO, a24 },
	};
	return write_regs(me->port, PAGE_2, sa, ARRAY_SIZE(sa));
}

/**
 * issue a single flash sector erase command to
 * erase PARADE_FW_SECTOR (4KB) bytes.
 *
 * @param me		device context
 * @param offset	device byte offset, but containing
 *			sector is erased
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_sector_erase(struct Ps8751 *me, uint32_t offset)
{
	static const struct i2c_write_vec se[] = {
		{ P2_SPI_LEN, 0x03 },
		{ P2_SPI_CTRL, P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};

	if (ps8xxx_spi_cmd_enable_writes(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_setup_cmd24(me, SPI_CMD_ERASE_SECTOR, offset))
		return EC_ERROR_UNKNOWN;

	if (write_regs(me->port, PAGE_2, se, ARRAY_SIZE(se)))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_fifo_wait_busy(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_wait_rom_ready(me))
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

/**
 * erase a chunk of flash.  offset and data_size must be sector aligned,
 * which is PARADE_FW_SECTOR (4KB).
 *
 * assumes SPI interface has been enabled for programming.
 *
 * @param me		device context
 * @param offset	device offset, 1st byte to erase
 * @param data_size	number of bytes to erase
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_erase(struct Ps8751 *me,
				     uint32_t offset, uint32_t data_size)
{
	uint32_t end = offset + data_size;
	int rval = 0;
	uint64_t t0_us;

	ccprintf("offset 0x%06x size %u\n", offset, data_size);

	t0_us = get_time().val;
	for (; offset < end; offset += PARADE_FW_SECTOR) {
		if (ps8xxx_sector_erase(me, offset) != EC_SUCCESS)
			rval = -1;
	}
	ccprintf("%s: erased %uKB in %ums\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned)USEC_TO_MSEC(timer_us(t0_us)));
	return rval;
}

/**
 * program flash with new data
 *
 * flash is assumed to be erased
 * the MPU is assumed to be stopped (highly recommended)
 * the SPI bus and flash are write-enabled
 *
 * @param me		device context
 * @param fw_start	flash device offset to program
 * @param data		addr of data to write
 * @param data_size	size of data to write
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_program(struct Ps8751 *me,
				       const uint32_t fw_start,
				       const uint8_t * const data,
				       const int data_size)
{
	uint32_t data_offset;
	uint64_t t0_us;
	int chunk;
	int chunk_mods;
	int bytes_skipped = 0;

	ccprintf("%s: programming %uKB...\n", me->chip_name, data_size >> 10);

	t0_us = get_time().val;
	for (data_offset = 0;
	     data_offset < data_size;
	     data_offset += chunk) {
		int page_offset;

		chunk = MIN(PS_FW_WR_CHUNK, data_size - data_offset);
		/* clip at flash page boundary */
		page_offset = (fw_start + data_offset) & SPI_PAGE_MASK;
		chunk = MIN(chunk, SPI_PAGE_SIZE - page_offset);

		/* any changes in this chunk? */
		chunk_mods = 0;
		for (int i = 0; i < chunk; ++i) {
			if (data[data_offset + i] != 0xff) {
				++chunk_mods;
				break;
			}
		}
		if (chunk_mods == 0) {
			bytes_skipped += chunk;
			continue;
		}

		if (ps8xxx_spi_cmd_enable_writes(me))
			return EC_ERROR_UNKNOWN;

		if (ps8xxx_spi_setup_cmd24(me,
					   SPI_CMD_PROG_PAGE,
					   fw_start + data_offset)) {
			return EC_ERROR_UNKNOWN;
		}
		for (int i = 0; i < chunk; ++i) {
			if (write_reg(me->port, PAGE_2,
				      P2_WR_FIFO, data[data_offset + i]))
				return EC_ERROR_UNKNOWN;
		}

		if (write_reg(me->port, PAGE_2, P2_SPI_LEN, 4 + chunk - 1))
			return EC_ERROR_UNKNOWN;
		if (write_reg(me->port, PAGE_2, P2_SPI_CTRL,
			      P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER))
			return EC_ERROR_UNKNOWN;
		if (ps8xxx_spi_fifo_wait_busy(me))
			return EC_ERROR_UNKNOWN;
		if (ps8xxx_spi_wait_rom_ready(me))
			return EC_ERROR_UNKNOWN;
	}
	ccprintf("%s: programmed %uKB in %us (%uB skipped)\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned int)USEC_TO_SEC(timer_us(t0_us)),
	       bytes_skipped);
	return EC_SUCCESS;
}

/**
 * verify flash content matches given data
 *
 * @param me		device context
 * @param fw_start	flash device offset to verify
 * @param data		addr of data to match
 * @param data_size	size of data to match
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_verify(struct Ps8751 *me,
				      const uint32_t fw_addr,
				      const uint8_t * const data,
				      const size_t data_size)
{
	uint64_t deadline = 0;
	int readback;
	uint64_t t0_us;
	uint32_t data_offset;
	int chunk;

	ccprintf("offset 0x%06x size %zu\n", fw_addr, data_size);

	t0_us = get_time().val;
	for (data_offset = 0;
	     data_offset < data_size;
	     data_offset += chunk) {

		chunk = MIN(PS_FW_RD_CHUNK, data_size - data_offset);

		if (ps8xxx_keep_awake(me, &deadline))
			return EC_ERROR_UNKNOWN;
		if (ps8xxx_spi_setup_cmd24(me, SPI_CMD_READ_DATA,
					   fw_addr + data_offset)) {
			return EC_ERROR_UNKNOWN;
		}

		if (write_reg(me->port, PAGE_2, P2_SPI_LEN,
			      ((chunk - 1) << 4) | (4 - 1)))
			return EC_ERROR_UNKNOWN;
		if (write_reg(me->port, PAGE_2, P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER))
			return EC_ERROR_UNKNOWN;

		if (ps8xxx_spi_fifo_wait_busy(me))
			return EC_ERROR_UNKNOWN;
		for (int i = 0; i < chunk; ++i) {
			if (read_reg(me->port, PAGE_2, P2_RD_FIFO, &readback))
				return EC_ERROR_UNKNOWN;
			if (readback != data[data_offset + i]) {
				ccprintf("%s: mismatch at offset 0x%06x "
				       "0x%02x != 0x%02x (expected)\n",
				       me->chip_name,
				       fw_addr + data_offset + i,
				       readback, data[data_offset + i]);
				return EC_ERROR_UNKNOWN;
			}
		}
	}
	ccprintf("%s: verified %zuKB in %us\n",
	       me->chip_name,
	       data_size >> 10,
	       (unsigned)USEC_TO_SEC(timer_us(t0_us)));
	return EC_SUCCESS;
}

static int ps8xxx_ec_pd_suspend(struct Ps8751 *me)
{
	/*
	 * Need to do somethig equivalent to this call below
	 */
	/* pd_firmware_upgrade_check_power_readiness(); */

	pd_comm_enable(me->port, 0);
	pd_set_suspend(me->port, 1);

	return EC_SUCCESS;
}

static int ps8xxx_ec_pd_resume(struct Ps8751 *me)
{
	pd_set_suspend(me->port, 0);
	pd_comm_enable(me->port, 1);

	return EC_SUCCESS;
}

static void ps8xxx_dump_flash(struct Ps8751 *me,
			      const uint32_t offset_start,
			      const uint32_t size)
{
	uint32_t offset_end  = offset_start + size;
	uint8_t prev_buf[PS_FW_RD_CHUNK];
	int val;
	uint8_t buf[PS_FW_RD_CHUNK];
	uint64_t deadline = 0;
	uint32_t offset;
	int dot3 = 0;
	int i;

	ccprintf("================================"
		 "================================\n{\n");

	for (offset = offset_start;
	     offset < offset_end;
	     offset += sizeof(buf)) {
		static const struct i2c_write_vec trig[] = {
			{ P2_SPI_LEN,
			  ((PS_FW_RD_CHUNK - 1) << 4) | (4 - 1) },
			{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
		};

		if (ps8xxx_keep_awake(me, &deadline))
			break;

		/* construct a SPI read PS_FW_CHUNK bytes @ offset command */
		if (ps8xxx_spi_setup_cmd24(me,
					   SPI_CMD_READ_DATA, offset)) {
			ccprintf("could not set up addr\n");
			break;
		}

		if (write_regs(me->port, PAGE_2, trig, ARRAY_SIZE(trig))) {
			ccprintf("could not issue SPI_CTRL\n");
			break;
		}
		if (ps8xxx_spi_fifo_wait_busy(me))
			return;

		for (i = 0; i < sizeof(buf); ++i) {
			if (read_reg(me->port, PAGE_2, P2_RD_FIFO, &val))
				break;
			buf[i] = val & 0xff;
		}
		if (i != sizeof(buf))
			break;

		if (offset > offset_start &&
		    memcmp(buf, prev_buf, sizeof(buf)) == 0) {
			if (!dot3) {
				ccprintf("...\n");
				dot3 = 1;
			}
		} else {
			memcpy(prev_buf, buf, sizeof(prev_buf));
			ccprintf("0x%06x: ", offset);
			for (i = 0; i < sizeof(buf); ++i)
				ccprintf(" %02x", buf[i]);
			ccprintf("\n");
			dot3 = 0;
		}
	}

	ccprintf("\n}\n================================"
	       "================================\n");
}

/**
 * install a new firmware image, replacing whatever was there before,
 * then verify installed data.
 *
 * the MPU is assumed to be stopped (highly recommended)
 * the SPI bus and flash are write-enabled
 *
 * @param me		device context
 * @param data		addr of firmware blob to install
 * @param data_size	size of firmware blob to install
 * @return 0 if ok, -1 on error
 */

static int ps8xxx_reflash(struct Ps8751 *me, const uint8_t *data,
			  size_t data_size)
{
	int status;

	ccprintf("data %8p len %zu\n", data, data_size);

	status = ps8xxx_erase(me, PARADE_FW_START, data_size);
	if (status != 0) {
		ccprintf("%s: FW erase failed\n", me->chip_name);
		return EC_ERROR_UNKNOWN;
	}
	/*
	 * quick confidence check to see if we modified flash
	 * we'll do a full verify after programming
	 */
	status = ps8xxx_verify(me, PARADE_FW_START,
			       erased_bytes,
			       MIN(data_size, sizeof(erased_bytes)));
	if (status != 0) {
		ccprintf("%s: FW erase verify failed\n", me->chip_name);
		return EC_ERROR_UNKNOWN;
	}

	if (PS8751_DEBUG > 0) {
		ccprintf("start post erase 7s delay...\n");
		udelay(7000 * MSEC);
		ccprintf("end post erase delay\n");
	}

	if (PS8751_DEBUG >= 2)
		ps8xxx_dump_flash(me, PARADE_FW_START,
				  PARADE_FW_END - PARADE_FW_START);
	status = ps8xxx_program(me, PARADE_FW_START, data, data_size);
	if (PS8751_DEBUG >= 2)
		ps8xxx_dump_flash(me, PARADE_FW_START, PARADE_TEST_FW_SIZE);
	if (status != 0) {
		ccprintf("%s: FW program failed\n", me->chip_name);
		return EC_ERROR_UNKNOWN;
	}

	return ps8xxx_verify(me, PARADE_FW_START, data, data_size);
}


static int ps8xxx_halt_and_flash(struct Ps8751 *me,
				 const uint8_t *image, size_t image_size)
{
	int status = -1;

	if (ps8xxx_rom_ctrl(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_disable_mpu(me))
		return EC_ERROR_UNKNOWN;
	if (ps8xxx_spi_flash_unlock(me))
		goto enable_mpu;
	ccprintf("unlock_spi_bus returned\n");
	if (ps8xxx_spi_flash_identify(me) == 0 &&
	    ps8xxx_reflash(me, image, image_size) == 0)
		status = 0;

	if (ps8xxx_spi_flash_lock(me))
		status = -1;

 enable_mpu:
	if (ps8xxx_enable_mpu(me))
		return EC_ERROR_UNKNOWN;
	return status;
}

static void ps8xxx_init(int port)
{
	struct Ps8751 *me = &tcpc_info[port];

	me->port = port;
	me->ec_pd_id = port;
	me->chip_type = CHIP_PS8805;
	me->blob_hw_version = 0xA2;
}

int ps8xxx_update_image(int port, const uint8_t *image, size_t image_size)
{
	struct Ps8751 *me = &tcpc_info[port];
	int status = EC_ERROR_UNKNOWN;
	int timeout;

	ccprintf("call...\n");

	if (image == NULL || image_size == 0)
		return EC_ERROR_INVAL;

	if (ps8xxx_wake_i2c(me))
		goto pd_resume;

	if (!ps8xxx_is_fw_compatible(me, image))
		goto pd_resume;

	ps8xxx_ec_pd_suspend(me);

	if (ps8xxx_halt_and_flash(me, image, image_size) == 0)
		status = EC_SUCCESS;


 pd_resume:
	if (ps8xxx_ec_pd_resume(me))
		status = EC_ERROR_UNKNOWN;

	/* Wait at most ~60ms for reset to occur. */
	timeout = PS_RESTART_DELAY_CS;
	do {
		if (ps8xxx_capture_device_id(me, 1) == PS8751_DEVICE_PRESENT)
			break;

		udelay(10 * MSEC);
		timeout--;
	} while (timeout > 0);

	if (timeout == 0)
		status = EC_ERROR_UNKNOWN;

	return status;
}

static int command_ps8805(int argc, char **argv)
{
	int port = 1;
	struct Ps8751 *me = &tcpc_info[port];
	uint8_t const *image;
	char image_ver[3];
	int len = ARRAY_SIZE(PS8805_Generic_A2_Testing_20210823_bin);

	image = PS8805_Generic_A2_Testing_20210823_bin;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	ps8xxx_init(port);

	if (!strcasecmp(argv[1], "ver")) {
		char *e;
		int image_rev;

		ps8xxx_capture_device_id(me, 1);
		ps8xxx_is_fw_compatible(me, image);

		image_ver[0] = image[0x2030 + 2];
		image_ver[1] = image[0x2030 + 3];
		image_ver[2] = '\0';
		image_rev = strtoi(image_ver, &e, 16);

		ccprintf("image: %x %x %x %x %s 0x%x\n",
			 image[0x2030 + 0],
			 image[0x2030 + 1],
			 image[0x2030 + 2],
			 image[0x2030 + 3],
			 image_ver,
			 image_rev);

		ccprintf("ps8805: chip_fw = 0x%02x, image_ver = 0x%02x, image_size = %d\n",
			 me->chip.fw_rev,
			 image_rev,
			 len);

	} else if (!strcasecmp(argv[1], "up")) {
		ps8xxx_update_image(port, image, len);
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ps8805, command_ps8805,
			"ver",
			"ps8805 fw update");


