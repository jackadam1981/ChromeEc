/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MAX32660 I2C port module for Chrome EC. */

#include <stdint.h>
#include "common.h"
#include "config_chip.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "task.h"
#include "registers.h"
#include "i2c_api.h"

int init_i2cs(int port);

/* Port address for each I2C */
static mxc_i2c_regs_t *i2c_bus_ports[] = { MXC_I2C0, MXC_I2C1 };

#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS
/* IRQ for each I2C */
static uint32_t i2c_bus_irqs[] = { EC_I2C0_IRQn, EC_I2C1_IRQn };
#endif

/**
 * struct i2c_port_data
 * @out: Output data pointer.
 * @out_size: Output data to transfer, in bytes.
 * @in: Input data pointer.
 * @in_size: Input data to transfer, in bytes.
 * @flags: Flags (I2C_XFER_*).
 * @idx: Index into input/output data.
 * @err: Error code, if any.
 * @timeout_us:	Transaction timeout, or 0 to use default.
 * @task_waiting: Task waiting on port, or TASK_ID_INVALID if none.
 */
struct i2c_port_data {
	const uint8_t *out;
	int out_size;
	uint8_t *in;
	int in_size;
	int flags;
	int idx;
	int err;
	uint32_t timeout_us;
	volatile int task_waiting;
};
static struct i2c_port_data pdata[I2C_PORT_COUNT];

/**
 * chip_i2c_xfer() - Low Level function for I2C Master Reads and Writes.
 * @port: Port to access
 * @slave_addr:	Slave device address
 * @out: Data to send
 * @out_size: Number of bytes to send
 * @in: Destination buffer for received data
 * @in_size: Number of bytes to receive
 * @flags: Flags (see I2C_XFER_* above)
 * 
 * Chip-level function to transmit one block of raw data, then receive one
 * block of raw data.
 *
 * This is a low-level chip-dependent function and should only be called by
 * i2c_xfer().\
 * 
 * Return EC_SUCCESS, or non-zero if error.
 */
int chip_i2c_xfer(int port, const uint16_t slave_addr_flags, const uint8_t *out,
		  int out_size, uint8_t *in, int in_size, int flags)
{
	int xfer_start;
	int xfer_stop;
	int status;
	int ret;

	xfer_start = flags & I2C_XFER_START;
	xfer_stop = flags & I2C_XFER_STOP;

	status = 0;
	ret = EC_SUCCESS;
	if (out_size) {
		status = I2C_Api_MasterWrite(i2c_bus_ports[port],
					     slave_addr_flags, xfer_start,
					     xfer_stop, out, out_size, 1);
		if (status != EC_SUCCESS)
			ret = EC_ERROR_UNKNOWN;
	}
	if (in_size && (status == EC_SUCCESS)) {
		status = I2C_Api_MasterRead(i2c_bus_ports[port],
					    slave_addr_flags, xfer_start,
					    xfer_stop, in, in_size, 0);
		if (status != EC_SUCCESS)
			ret = EC_ERROR_UNKNOWN;
	}
	return ret;
}

/**
 * i2c_get_line_levels() - Read the current digital levels on the I2C pins.
 * @port: Port number to use when reading line levels.
 *
 * Return a byte where bit 0 is the line level of SCL and
 * bit 1 is the line level of SDA.
 */
int i2c_get_line_levels(int port)
{
	/* Retrieve the current levels of SCL and SDA from the control reg. */
	return (i2c_bus_ports[port]->ctrl >> MXC_F_I2C_CTRL_SCL_POS) & 0x03;
}

/**
 * i2c_set_timeout()
 * @port: Port number to set timeout for.
 * @timeout: Timeout duration in microseconds.
 */
void i2c_set_timeout(int port, uint32_t timeout)
{
	pdata[port].timeout_us = timeout ? timeout : I2C_TIMEOUT_DEFAULT_US;
}

/**
 * i2c_init() - Initialize the I2C ports used on device.
 */
static void i2c_init(void)
{
	int i;
	int port;

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);

	/* Initialize all I2C ports used. */
	for (i = 0; i < i2c_ports_used; i++) {
		port = i2c_ports[i].port;
		I2C_Api_Init(i2c_bus_ports[port], i2c_ports[i].kbps * 1000);
		i2c_set_timeout(i, 0);
	}

#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS
	/* Initialize the I2C Slave */
	init_i2cs(I2C_PORT_EC);
#endif
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_INIT_I2C);

/**
 *  I2C Slave Implentation
 */
#ifdef CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS

/**
 * Buffer for received host command packets (including prefix byte on request,
 * and result/size on response).  After any protocol-specific headers, the
 * buffers must be 32-bit aligned.
 */
static uint8_t host_buffer_padded[I2C_MAX_HOST_PACKET_SIZE + 4 +
				  CONFIG_I2C_EXTRA_PACKET_SIZE] __aligned(4);
static uint8_t *const host_buffer = host_buffer_padded + 2;
static uint8_t params_copy[I2C_MAX_HOST_PACKET_SIZE] __aligned(4);
static struct host_packet i2c_packet;

static i2c_req_t req_slave;
volatile int ec_pending_response = 0;

void mockup_process_host_command(i2c_req_t *req);

/**
 * i2c_send_response_packet() - Send the responze packet to get processed.
 * @pkt: Packet to send.
 */
static void i2c_send_response_packet(struct host_packet *pkt)
{
	int size = pkt->response_size;
	uint8_t *out = host_buffer;

	/* Ignore host command in-progress */
	if (pkt->driver_result == EC_RES_IN_PROGRESS)
		return;

	/* Write result and size to first two bytes. */
	*out++ = pkt->driver_result;
	*out++ = size;

	/* host_buffer data range */
	req_slave.tx_len = size + 2;

	/* Call the handler for transmition of response packet. */
	I2C_Api_Handler(i2c_bus_ports[I2C_PORT_EC]);
}

/**
 * i2c_process_command() - Process the command in the i2c host buffer 
 */
static void i2c_process_command(void)
{
	char *buff = host_buffer;

	i2c_packet.send_response = i2c_send_response_packet;
	i2c_packet.request = (const void *)(&buff[1]);
	i2c_packet.request_temp = params_copy;
	i2c_packet.request_max = sizeof(params_copy);
	/* Don't know the request size so pass in the entire buffer */
	i2c_packet.request_size = I2C_MAX_HOST_PACKET_SIZE;

	/*
	 * Stuff response at buff[2] to leave the first two bytes of
	 * buffer available for the result and size to send over i2c.  Note
	 * that this 2-byte offset and the 2-byte offset from host_buffer
	 * add up to make the response buffer 32-bit aligned.
	 */
	i2c_packet.response = (void *)(&buff[2]);
	i2c_packet.response_max = I2C_MAX_HOST_PACKET_SIZE;
	i2c_packet.response_size = 0;

	if (*buff >= EC_COMMAND_PROTOCOL_3) {
		i2c_packet.driver_result = EC_RES_SUCCESS;
	} else {
		/* Only host command protocol 3 is supported. */
		i2c_packet.driver_result = EC_RES_INVALID_HEADER;
	}

	host_packet_receive(&i2c_packet);
}

/**
 * i2c_chip_callback() - Async Callback from I2C Slave driver.
 * @req:   Request currently being processed.
 * @error: Error from async driver, EC_SUCCESS if no error.
 */
void i2c_chip_callback(i2c_req_t *req, int error)
{
	/* check if there was a host command (I2C master write) */
	if (req->direction == I2C_TRANSFER_DIRECTION_MASTER_WRITE) {
		req->tx_len = -1; /* nothing to send yet */

		/* process incoming host command here */
		req->rx_data = host_buffer;
		req->tx_data = host_buffer;
		i2c_process_command();

		/* set the rx buffer for next host command */
		req->rx_data = host_buffer;
	}

	req->addr = CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS;
	req->rx_len = I2C_MAX_HOST_PACKET_SIZE;
	req->callback = i2c_chip_callback;
}

/**
 * I2C0_IRQHandler() - Async Handler for I2C Slave driver.
 */
void I2C0_IRQHandler(void)
{
	I2C_Api_Handler(i2c_bus_ports[0]);
}

/**
 * I2C1_IRQHandler() - Async Handler for I2C Slave driver.
 */
void I2C1_IRQHandler(void)
{
	I2C_Api_Handler(i2c_bus_ports[1]);
}

DECLARE_IRQ(EC_I2C0_IRQn, I2C0_IRQHandler, 1);
DECLARE_IRQ(EC_I2C1_IRQn, I2C1_IRQHandler, 1);

/**
 * init_i2cs() - Async Handler for I2C Slave driver.
 * @port: I2C port number to initialize.
 */
int init_i2cs(int port)
{
	int error;

	/*	I2C_Api_Shutdown(i2c_bus_ports[port]); */
	if ((error = I2C_Api_Init(i2c_bus_ports[port], I2C_STD_MODE)) !=
		EC_SUCCESS) {
		while (1)
			;
	}
	/* Prepare SlaveAsync */
	req_slave.addr = CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS;
	req_slave.tx_data = host_buffer; /* transmitted to host */
	req_slave.tx_len = -1; /* Nothing to send. */
	req_slave.rx_data = host_buffer; /* received from host */
	req_slave.rx_len = I2C_MAX_HOST_PACKET_SIZE;
	req_slave.restart = 0;
	req_slave.callback = i2c_chip_callback;

	if ((error = I2C_Api_SlaveAsync(i2c_bus_ports[port], &req_slave)) !=
		EC_SUCCESS) {
		while (1)
			;
	}

	task_enable_irq(i2c_bus_irqs[port]);
	return 0;
}

#endif /* CONFIG_HOSTCMD_I2C_SLAVE_ADDR_FLAGS */
