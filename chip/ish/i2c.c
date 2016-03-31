/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C port module for ISH3.0 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "ish_i2c.h"
#include "task.h"
#include "timer.h"
#include "hwtimer.h"
#include "util.h"

#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

#define I2C_FLAG_REPEATED_START_DISABLED	0
#define EVENT_FLAG_I2C_TIMEOUT			TASK_EVENT_CUSTOM(1 << 1)

uint16_t default_hcnt_scl_100[] = { 4000, 4420, 4920, 4400, 4000, 4000, 4300 };
uint16_t default_lcnt_scl_100[] = { 4720, 5180, 4990, 5333, 4700, 5200, 4950 };
uint16_t default_hcnt_scl_400[] = { 600, 820, 1120, 1066, 600, 600, 450 };
uint16_t default_lcnt_scl_400[] = { 1320, 1380, 1300, 1300, 1300, 1200, 1250 };
uint16_t default_hcnt_scl_hs[] = { 160, 300, 160, 166, 175, 150, 162 };
uint16_t default_lcnt_scl_hs[] = { 320, 340, 320, 325, 325, 300, 297 };

unsigned char bus_freq[ISH_I2C_PORT_COUNT];
unsigned char init_freq_mode = I2C_FREQ_120;
i2c_context i2c_ctxs[ISH_I2C_PORT_COUNT];
i2c_bus_info_t board_config[ISH_I2C_PORT_COUNT];
int i2c_wait_for_activity_timeout_counter;

static inline void i2c_mmio_write(uint32_t *base, unsigned char offset,
				  uint32_t data)
{
	REG32((uint32_t) ((unsigned char *)base + offset)) = data;
}

static inline uint32_t i2c_mmio_read(uint32_t *base, unsigned char offset)
{
	return REG32((uint32_t) ((unsigned char *)base + offset));
}

static inline unsigned char i2c_read_byte(uint32_t *addr, unsigned char reg,
					  unsigned char offset)
{
	uint32_t ret = i2c_mmio_read(addr, reg) >> offset;

	return ret & 0xff;
}

void i2c_intr_switch(uint32_t *base, int mode)
{
	switch (mode) {

	case ENABLE_WRITE_INT:
		i2c_mmio_write(base, IC_INTR_MASK, IC_INTR_WRITE_MASK_VAL);
		break;

	case ENABLE_READ_INT:
		i2c_mmio_write(base, IC_INTR_MASK, IC_INTR_READ_MASK_VAL);
		break;

	case DISABLE_INT:
		i2c_mmio_write(base, IC_INTR_MASK, 0);
		/* clear interrupts: TX_ABORT
		 * Because the DW_apb_i2c's TX FIFO is forced into a
		 * flushed/reset state whenever a TX_ABRT event occurs, it
		 * is necessary for software to release the DW_apb_i2c from
		 * this state by reading the IC_CLR_TX_ABRT register before
		 * attempting to write into the TX FIFO
		 */
		i2c_mmio_read(base, IC_CLR_TX_ABRT);
		/* STOP_DET */
		i2c_mmio_read(base, IC_CLR_STOP_DET);
		break;

	default:
		break;
	}
}

void i2c_init_transaction(i2c_context *ctx, uint32_t *base,
			  unsigned char slave_addr, unsigned char flags)
{
	uint32_t con_value;

	/* disable interrupts */
	i2c_intr_switch(base, DISABLE_INT);

	i2c_mmio_write(base, IC_ENABLE, IC_ENABLE_DISABLE);
	i2c_mmio_write(base, IC_TAR, (slave_addr << IC_TAR_OFFSET) |
		       TAR_SPECIAL_VAL | IC_10BITADDR_MASTER_VAL);

	/* set Clock SCL Count */
	switch (ctx->speed) {

	case I2C_SPEED_STD:
		i2c_mmio_write(base, IC_SS_SCL_HCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].std_speed.hcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_SS_SCL_LCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].std_speed.lcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_SDA_HOLD,
		       NS_2_COUNTERS(board_config[ctx->bus].std_speed.sda_hold,
					     clk_in[bus_freq[ctx->bus]]));
		break;

	case I2C_SPEED_FAST:
		i2c_mmio_write(base, IC_FS_SCL_HCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].fast_speed.hcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_FS_SCL_LCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].fast_speed.lcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_SDA_HOLD,
		       NS_2_COUNTERS(board_config[ctx->bus].fast_speed.sda_hold,
					     clk_in[bus_freq[ctx->bus]]));
		break;

	case I2C_SPEED_HIGH:
		i2c_mmio_write(base, IC_HS_SCL_HCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].high_speed.hcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_HS_SCL_LCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].high_speed.lcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_SDA_HOLD,
		       NS_2_COUNTERS(board_config[ctx->bus].high_speed.sda_hold,
					     clk_in[bus_freq[ctx->bus]]));

		i2c_mmio_write(base, IC_FS_SCL_HCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].fast_speed.hcnt,
					     clk_in[bus_freq[ctx->bus]]));
		i2c_mmio_write(base, IC_FS_SCL_LCNT,
		       NS_2_COUNTERS(board_config[ctx->bus].fast_speed.lcnt,
					     clk_in[bus_freq[ctx->bus]]));
		break;

	default:
		break;
	}

	/* in SPT HW we need to sync between I2C clock and data signals */
	con_value = i2c_mmio_read(base, IC_CON);

	if (flags & I2C_FLAG_REPEATED_START_DISABLED)
		con_value &= ~IC_RESTART_EN_VAL;
	else
		con_value |= IC_RESTART_EN_VAL;

	i2c_mmio_write(base, IC_CON, con_value);
	i2c_mmio_write(base, IC_FS_SPKLEN, spkln[bus_freq[ctx->bus]]);
	i2c_mmio_write(base, IC_HS_SPKLEN, spkln[bus_freq[ctx->bus]]);
	i2c_mmio_write(base, IC_ENABLE, IC_ENABLE_ENABLE);
}

void i2c_write_buffer(uint32_t *base, unsigned char len,
		      unsigned char *buffer, ssize_t *cur_index,
		      ssize_t total_len)
{
	int i;
	uint16_t out;

	for (i = 0; i < len; i++) {
		++(*cur_index);
		out = (buffer[i] << DATA_CMD_DAT_OFFSET) | DATA_CMD_WRITE_VAL;
		if (*cur_index == total_len)
			out |= DATA_CMD_STOP_VAL;
		i2c_mmio_write(base, IC_DATA_CMD, out);
	}
}

void i2c_write_read_commands(uint32_t *base, unsigned char len)
{
	int i;

	for (i = 0; i < len - 1; i++)
		i2c_mmio_write(base, IC_DATA_CMD, DATA_CMD_READ_VAL);

	i2c_mmio_write(base, IC_DATA_CMD,
		       DATA_CMD_READ_VAL | DATA_CMD_STOP_VAL);
}

static int wait_tsk_id;

int chip_i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
		  uint8_t *in, int in_size, int flags)
{
	i2c_req req;
	uint16_t abort_reason;
	int error, i;
	uint8_t tmp_buf[out_size];
	uint32_t *base;
	i2c_context *ctx;
	ssize_t curr_index = 0;
	ssize_t total_len = 0;
	uint64_t expire_ts;

	memcpy(tmp_buf, out, out_size);
	req.abort_reason = &abort_reason;
	req.error = &error;
	/* function interface specifies an 8-bit slave addr: convert it to
	 * a 7-bit addr to meet the expectations of the driver code.
	 */
	req.slave_addr = (uint8_t)slave_addr >> 1;

	req.operation = (in_size > 0) ? I2C_READ : I2C_WRITE;
	if (req.operation == I2C_WRITE) {
		req.w_len = (uint8_t) out_size - 1;
		req.w_data = &tmp_buf[1];
		req.r_len = 0;
		req.r_data = NULL;
#ifdef ISH_DEBUG
		CPRINTF("I2C_WRITE len: %d [", req.w_len);
		for (i = 0; i < req.w_len; i++)
			CPRINTF("0x%0x ", req.w_data[i]);
		CPUTS("]\n");
#endif
	} else {
		req.r_len = (uint8_t) in_size;
		req.r_data = in;
		req.w_data = NULL;
		req.w_len = 0;
	}
	req.next = NULL;

	req.extra_w_len = 1;
	req.extra_w_data = &tmp_buf[0];

	base = i2c_ctxs[port].base;
	ctx = &(i2c_ctxs[port]);
	ctx->current_req = &req;
	ctx->error_flag = 0;
	wait_tsk_id = task_get_current();

	total_len = ctx->current_req->extra_w_len
		+ ctx->current_req->w_len
		+ ctx->current_req->r_len;

	i2c_init_transaction(ctx, base, ctx->current_req->slave_addr,
			     ctx->current_req->flags);
	/* write extra data */
	i2c_write_buffer(base, ctx->current_req->extra_w_len,
			 ctx->current_req->extra_w_data, &curr_index,
			 total_len);
	/* write W data */
	i2c_write_buffer(base, ctx->current_req->w_len,
			 ctx->current_req->w_data, &curr_index, total_len);

	if (ctx->current_req->r_len > 0) {
		/* write R commands */
		i2c_write_read_commands(base, ctx->current_req->r_len);

		/* set rx_theshold */
		i2c_mmio_write(base, IC_RX_TL, ctx->current_req->r_len - 1);
	}
	/* enable interrupts */
	i2c_intr_switch(base, mask_arr[ctx->current_req->operation]);

	task_wait_event_mask(EVENT_FLAG_I2C_TIMEOUT, -1);

	if (((ctx->interrupts & M_TX_ABRT) == 0)
	    && (ctx->current_req->r_len > 0)) {
		/* read data */
		for (i = 0; i < ctx->current_req->r_len; i++) {
			ctx->current_req->r_data[i] =
			    i2c_read_byte(base, IC_DATA_CMD, 0);
		}
	} else if ((ctx->interrupts & M_TX_ABRT) != 0) {
		ctx->error_flag = 1;
		*(ctx->current_req->error) = -1;
	}

	ctx->reason = 0;
	ctx->interrupts = 0;
	/* do not disable device before master is idle */
	expire_ts = __hw_clock_source_read() + I2C_TSC_TIMEOUT;

	while (i2c_mmio_read(base, IC_STATUS) &
	       (1 << IC_STATUS_MASTER_ACTIVITY)) {

		if (__hw_clock_source_read() >= expire_ts) {
			i2c_wait_for_activity_timeout_counter++;
			break;
		}
	}
	i2c_mmio_write(base, IC_ENABLE, IC_ENABLE_DISABLE);

#ifdef ISH_DEBUG
	if (req.operation == I2C_READ) {
		CPRINTF("I2C read len: %d [", req.r_len);
		for (i = 0; i < req.r_len; i++)
			CPRINTF("0x%0x ", req.r_data[i]);
		CPUTS("]\n");
	}
#endif
	return 0;
}

void i2c_initial_board_config(i2c_context *ctx)
{
	uint8_t bus = ctx->bus;

	board_config[bus].bus_id = bus;
	board_config[bus].std_speed.hcnt = default_hcnt_scl_100[bus_freq[bus]];
	board_config[bus].std_speed.lcnt = default_lcnt_scl_100[bus_freq[bus]];
	board_config[bus].std_speed.sda_hold = DEFAULT_SDA_HOLD;
	board_config[bus].fast_speed.hcnt = default_hcnt_scl_400[bus_freq[bus]];
	board_config[bus].fast_speed.lcnt = default_lcnt_scl_400[bus_freq[bus]];
	board_config[bus].fast_speed.sda_hold = DEFAULT_SDA_HOLD;
	board_config[bus].high_speed.hcnt = default_hcnt_scl_hs[bus_freq[bus]];
	board_config[bus].high_speed.lcnt = default_lcnt_scl_hs[bus_freq[bus]];
	board_config[bus].high_speed.sda_hold = DEFAULT_SDA_HOLD;
}

void i2c_interrupt_handler(void *param)
{
	uint32_t bus_number = (uint32_t) param;

	/* check interrupts */
	i2c_ctxs[bus_number].interrupts = i2c_mmio_read(
			i2c_ctxs[bus_number].base, IC_INTR_STAT);
	i2c_ctxs[bus_number].reason = (uint16_t) i2c_mmio_read(
			i2c_ctxs[bus_number].base, IC_TX_ABRT_SOURCE);

	/* disable interrupts */
	i2c_intr_switch(i2c_ctxs[bus_number].base, DISABLE_INT);
	task_set_event(wait_tsk_id, EVENT_FLAG_I2C_TIMEOUT, 0);
}

void i2c_isr_bus0(void)
{
	i2c_interrupt_handler((void *)0);
}
DECLARE_IRQ(ISH_I2C0_IRQ, i2c_isr_bus0);

void i2c_isr_bus1(void)
{
	i2c_interrupt_handler((void *)1);
}
DECLARE_IRQ(ISH_I2C1_IRQ, i2c_isr_bus1);

void i2c_isr_bus2(void)
{
	i2c_interrupt_handler((void *)2);
}
DECLARE_IRQ(ISH_I2C2_IRQ, i2c_isr_bus2);

void i2c_init_hardware(i2c_context *ctx)
{
	uint32_t *base = ctx->base;

	/* disable interrupts */
	i2c_intr_switch(base, DISABLE_INT);
	i2c_mmio_write(base, IC_ENABLE, IC_ENABLE_DISABLE);
	i2c_mmio_write(base, IC_CON, (MASTER_MODE_VAL
				| speed_val_arr[ctx->speed]
				| IC_RESTART_EN_VAL
				| IC_SLAVE_DISABLE_VAL));

	i2c_mmio_write(base, IC_FS_SPKLEN, spkln[bus_freq[ctx->bus]]);
	i2c_mmio_write(base, IC_HS_SPKLEN, spkln[bus_freq[ctx->bus]]);

	/* get RX_FIFO and TX_FIFO depth */
	ctx->max_rx_depth = i2c_read_byte(base, IC_COMP_PARAM_1,
			RX_BUFFER_DEPTH_OFFSET) + 1;
	ctx->max_tx_depth = i2c_read_byte(base, IC_COMP_PARAM_1,
			TX_BUFFER_DEPTH_OFFSET) + 1;

}

static void i2c_init(void)
{
	i2c_ctxs[0].bus = 0;
	i2c_ctxs[1].bus = 1;
	i2c_ctxs[2].bus = 2;

	i2c_ctxs[0].base = (uint32_t *) ISH_I2C0_BASE;
	i2c_ctxs[1].base = (uint32_t *) ISH_I2C1_BASE;
	i2c_ctxs[2].base = (uint32_t *) ISH_I2C2_BASE;

	i2c_ctxs[0].speed = I2C_SPEED_FAST;
	i2c_ctxs[1].speed = I2C_SPEED_FAST;
	i2c_ctxs[2].speed = I2C_SPEED_FAST;

	i2c_initial_board_config(&(i2c_ctxs[0]));
	i2c_initial_board_config(&(i2c_ctxs[1]));
	i2c_initial_board_config(&(i2c_ctxs[2]));

	bus_freq[0] = init_freq_mode;
	bus_freq[1] = init_freq_mode;
	bus_freq[2] = init_freq_mode;

	i2c_init_hardware(&(i2c_ctxs[0]));
	i2c_init_hardware(&(i2c_ctxs[1]));
	i2c_init_hardware(&(i2c_ctxs[2]));

	task_enable_irq(ISH_I2C0_IRQ);
	task_enable_irq(ISH_I2C1_IRQ);
	task_enable_irq(ISH_I2C2_IRQ);

	CPRINTS("Done i2c_init");
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_INIT_I2C);

int i2c_port_to_controller(int port)
{
	return -1;
}

int i2c_get_line_levels(int port)
{
	return 0;
}
