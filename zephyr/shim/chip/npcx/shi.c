/*
 * Copyright 2021 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcx_cros_shi

/*
 * Shimmed SHI driver for Chrome EC.
 * TODO: Move to cros_shi driver step by step]
 * This uses Input/Output buffer to handle SPI transmission and reception.
 */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "system.h"
#include "util.h"

#include <arch/arm/aarch32/cortex_m/cmsis.h>
#include <assert.h>
#include <dt-bindings/clock/npcx_clock.h>
#include <drivers/clock_control.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <kernel.h>
#include <soc.h>
#include <soc/nuvoton_npcx/reg_def_cros.h>
#include "soc_miwu.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(cros_shi, LOG_LEVEL_ERR);

#define SHI_NODE                DT_NODELABEL(shi)
#define SHI_VER_CTRL_PH         DT_PHANDLE_BY_IDX(SHI_NODE, ver_ctrl, 0)
#define SHI_VER_CTRL_ALT_FILED(f) \
				DT_PHA_BY_IDX(SHI_VER_CTRL_PH, alts, 0, f)

/* Device config */
struct cros_shi_npcx_config {
	/* Serial Host Interface (SHI) base address */
	uintptr_t base;
	/* clock configuration */
	struct npcx_clk_cfg clk_cfg;
	/* pinmux configuration */
	const uint8_t alts_size;
	const struct npcx_alt *alts_list;
	/* SHI IRQ */
	int irq;
	struct npcx_wui shi_cs_wui;
};

static const struct npcx_alt cros_shi_alts[] = NPCX_DT_ALT_ITEMS_LIST(0);

static const struct cros_shi_npcx_config cros_shi_cfg = {
	.base = DT_INST_REG_ADDR(0),
	.clk_cfg = NPCX_DT_CLK_CFG_ITEM(0),
	.alts_size = ARRAY_SIZE(cros_shi_alts),
	.alts_list = cros_shi_alts,
	.irq = DT_INST_IRQN(0),
	.shi_cs_wui = NPCX_DT_WUI_ITEM_BY_NAME(0, shi_cs_wui),
};

#define CPUTS(outstr)
#define CPRINTS(format, args...) printk(format, ## args)
#define CPRINTF(format, args...) printk(format, ## args)

#define DEBUG_SHI 0
#if !(DEBUG_SHI)
#define DEBUG_CPUTS(...)
#define DEBUG_CPRINTS(...)
#define DEBUG_CPRINTF(...)
#else
#define DEBUG_CPUTS(...)
#define DEBUG_CPRINTS(format, args...) printk(format, ## args)
#define DEBUG_CPRINTF(format, args...) printk(format, ## args)
#endif


/* SHI Bus definition */
#define SHI_OBUF_FULL_SIZE  128                    /* Full output buffer size */
#define SHI_IBUF_FULL_SIZE  128                    /* Full input buffer size  */
/* Configure the IBUFLVL2 = the size of V3 protocol header */
#define SHI_IBUFLVL2_THRESHOLD (sizeof(struct ec_host_request))
#define SHI_OBUF_HALF_SIZE  (SHI_OBUF_FULL_SIZE/2) /* Half output buffer size */
#define SHI_IBUF_HALF_SIZE  (SHI_IBUF_FULL_SIZE/2) /* Half input buffer size  */

/* TODO: Remove this later */
#define NPCX_SHI_BASE_ADDR               0x4000F000

/* Start address of SHI output buffer */
#define SHI_OBUF_START_ADDR  (volatile uint8_t  *)(NPCX_SHI_BASE_ADDR + 0x020)
/* Middle address of SHI output buffer */
#define SHI_OBUF_HALF_ADDR   (SHI_OBUF_START_ADDR + SHI_OBUF_HALF_SIZE)
/* Top address of SHI output buffer */
#define SHI_OBUF_FULL_ADDR   (SHI_OBUF_START_ADDR + SHI_IBUF_FULL_SIZE)
/*
 * Valid offset of SHI output buffer to write.
 * When SIMUL bit is set, IBUFPTR can be used instead of OBUFPTR
 */
#define SHI_OBUF_VALID_OFFSET ((shi_read_buf_pointer() + \
			SHI_OUT_PREAMBLE_LENGTH) % SHI_OBUF_FULL_SIZE)
/* Start address of SHI input buffer */
/* TODO: Remove this later */
#define SHI_IBUF_START_ADDR               0x4000F0A0
/* Current address of SHI input buffer */
#define SHI_IBUF_CUR_ADDR    (SHI_IBUF_START_ADDR + shi_read_buf_pointer())

/*
 * Timeout to wait for SHI request packet
 *
 * This affects the slowest SPI clock we can support.  A delay of 8192 us
 * permits a 512-byte request at 500 KHz, assuming the master starts sending
 * bytes as soon as it asserts chip select.  That's as slow as we would
 * practically want to run the SHI interface, since running it slower
 * significantly impacts firmware update times.
 */
#define SHI_CMD_RX_TIMEOUT_US 8192

/* Timeout for glitch case. Make sure it will exceed 8 SPI clocks */
#define SHI_GLITCH_TIMEOUT_US 10000

/*
 * The AP blindly clocks back bytes over the SPI interface looking for a
 * framing byte.  So this preamble must always precede the actual response
 * packet.
 */

#define SHI_OUT_PREAMBLE_LENGTH 2
/*
 * Space allocation of the past-end status byte (EC_SPI_PAST_END) in the out_msg
 * buffer.
 */
#define EC_SPI_PAST_END_LENGTH 1
/*
 * Space allocation of the frame status byte (EC_SPI_FRAME_START) in the out_msg
 * buffer.
 */
#define EC_SPI_FRAME_START_LENGTH 1

/*
 * Offset of output parameters needs to account for pad and framing bytes and
 * one last past-end byte at the end so any additional bytes clocked out by
 * the AP will have a known and identifiable value.
 */
#define SHI_PROTO3_OVERHEAD (EC_SPI_PAST_END_LENGTH + EC_SPI_FRAME_START_LENGTH)


#ifdef NPCX_SHI_BYPASS_OVER_256B
/* The boundary which SHI will output invalid data on MISO. */
#define SHI_BYPASS_BOUNDARY 256
/* Increase FRAME_START_LENGTH in case shi outputs invalid FRAME_START byte */
#undef  EC_SPI_FRAME_START_LENGTH
#define EC_SPI_FRAME_START_LENGTH 2
#endif

/*
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size, and 512 bytes
 * of flash data:
 *  sizeof(ec_host_request):          8
 *  sizoef(ec_params_flash_write):    8
 *  payload                         512
 *
 */
#define SHI_MAX_REQUEST_SIZE 0x220

#ifdef NPCX_SHI_BYPASS_OVER_256B
/* Make sure SHI_MAX_RESPONSE_SIZE won't exceed 256 bytes */
#define SHI_MAX_RESPONSE_SIZE (160 + sizeof(struct ec_host_response))
BUILD_ASSERT(SHI_MAX_RESPONSE_SIZE <= SHI_BYPASS_BOUNDARY);
#else
#define SHI_MAX_RESPONSE_SIZE 0x220
#endif

/*
 * Our input and output msg buffers. These must be large enough for our largest
 * message, including protocol overhead.  The pointers after the protocol
 * overhead, as passed to the host command handler, must be 32-bit aligned.
 */
#define SHI_OUT_START_PAD (4 * (EC_SPI_FRAME_START_LENGTH / 4 + 1))
#define SHI_OUT_END_PAD (4 * (EC_SPI_PAST_END_LENGTH / 4 + 1))
static uint8_t out_msg_padded[SHI_OUT_START_PAD +
			      SHI_MAX_RESPONSE_SIZE +
			      SHI_OUT_END_PAD] __aligned(4);
static uint8_t * const out_msg =
	out_msg_padded + SHI_OUT_START_PAD - EC_SPI_FRAME_START_LENGTH;
static uint8_t in_msg[SHI_MAX_REQUEST_SIZE] __aligned(4);

/* Parameters used by host protocols */
static struct host_packet shi_packet;

enum shi_state {
	/* SHI not enabled (initial state, and when chipset is off) */
	SHI_STATE_DISABLED = 0,
	/* Ready to receive next request */
	SHI_STATE_READY_TO_RECV,
	/* Receiving request */
	SHI_STATE_RECEIVING,
	/* Processing request */
	SHI_STATE_PROCESSING,
	/* Canceling response since CS deasserted and output NOT_READY byte */
	SHI_STATE_CNL_RESP_NOT_RDY,
#ifdef NPCX_SHI_BYPASS_OVER_256B
	/* Keep output buffer as PROCESSING byte until reaching 256B boundary */
	SHI_STATE_WAIT_ALIGNMENT,
#endif
	/* Sending response */
	SHI_STATE_SENDING,
	/* Received data is valid. */
	SHI_STATE_BAD_RECEIVED_DATA,
};

volatile enum shi_state state;

/* SHI bus parameters */
struct shi_bus_parameters {
	uint8_t *rx_msg;          /* Entry pointer of msg rx buffer   */
	uint8_t *tx_msg;          /* Entry pointer of msg tx buffer   */
	volatile uint8_t *rx_buf; /* Entry pointer of receive buffer  */
	volatile uint8_t *tx_buf; /* Entry pointer of transmit buffer */
	uint16_t sz_received;     /* Size of received data in bytes   */
	uint16_t sz_sending;      /* Size of sending data in bytes    */
	uint16_t sz_request;      /* request bytes need to receive    */
	uint16_t sz_response;     /* response bytes need to receive   */
	uint64_t rx_deadline;     /* deadline of receiving            */
	uint8_t  pre_ibufstat;    /* Previous IBUFSTAT value          */
#ifdef NPCX_SHI_BYPASS_OVER_256B
	uint16_t bytes_in_256b;   /* Sent bytes in 256 bytes boundary */
#endif
} shi_params;

/* Forward declaration */
static void shi_reset_prepare(void);
static void shi_bad_received_data(void);
static void shi_fill_out_status(uint8_t status);
static void shi_write_half_outbuf(void);
static void shi_write_first_pkg_outbuf(uint16_t szbytes);
static int shi_read_inbuf_wait(uint16_t szbytes);
static uint8_t shi_read_buf_pointer(void);

/* TODO: Remove this later */
static inline void interrupt_disable(void)
{
	__asm__("cpsid i");
}

static inline void interrupt_enable(void)
{
	__asm__("cpsie i");
}

/*****************************************************************************/
/* V3 protocol layer functions */

/**
 * Called to send a response back to the host.
 *
 * Some commands can continue for a while. This function is called by
 * host_command when it completes.
 *
 */
static void shi_send_response_packet(struct host_packet *pkt)
{
	/*
	 * Disable interrupts. This routine is not called from interrupt
	 * context and buffer underrun will likely occur if it is
	 * preempted after writing its initial reply byte. Also, we must be
	 * sure our state doesn't unexpectedly change, in case we're expected
	 * to take RESP_NOT_RDY actions.
	 */
	interrupt_disable();
	if (state == SHI_STATE_PROCESSING) {
		/* Append our past-end byte, which we reserved space for. */
		((uint8_t *) pkt->response)[pkt->response_size + 0] =
			EC_SPI_PAST_END;

		/* Computing sending bytes of response */
		shi_params.sz_response =
			pkt->response_size + SHI_PROTO3_OVERHEAD;

		/* Start to fill output buffer with msg buffer */
		shi_write_first_pkg_outbuf(shi_params.sz_response);
#ifdef NPCX_SHI_BYPASS_OVER_256B
		/*
		 * If response package is over 256B boundary,
		 * keep sending PROCESSING byte
		 */
		if (state != SHI_STATE_WAIT_ALIGNMENT) {
#endif
			/* Transmit the reply */
			state = SHI_STATE_SENDING;
			DEBUG_CPRINTF("SND-");
#ifdef NPCX_SHI_BYPASS_OVER_256B
		}
#endif
	}
	/*
	 * If we're not processing, then the AP has already terminated the
	 * transaction, and won't be listening for a response.
	 * Reset state machine for next transaction.
	 */
	else if (state == SHI_STATE_CNL_RESP_NOT_RDY) {
		shi_reset_prepare();
		DEBUG_CPRINTF("END\n");
	} else
		DEBUG_CPRINTS("Unexpected state %d in response handler", state);
	interrupt_enable();
}

void shi_handle_host_package(void)
{
	uint16_t sz_inbuf_int = shi_params.sz_request / SHI_IBUF_HALF_SIZE;
	uint16_t cnt_inbuf_int = shi_params.sz_received / SHI_IBUF_HALF_SIZE;
	if (sz_inbuf_int - cnt_inbuf_int)
		/* Need to receive data from buffer */
		return;
	else {
		uint16_t remain_bytes = shi_params.sz_request
					- shi_params.sz_received;

		/* Read remaining bytes from input buffer directly */
		if (!shi_read_inbuf_wait(remain_bytes))
			return shi_bad_received_data();
		/* Move to processing state immediately */
		state = SHI_STATE_PROCESSING;
		DEBUG_CPRINTF("PRC-");
	}
	/* Fill output buffer to indicate we`re processing request */
	shi_fill_out_status(EC_SPI_PROCESSING);

	/* Set up parameters for host request */
	shi_packet.send_response = shi_send_response_packet;

	shi_packet.request = in_msg;
	shi_packet.request_temp = NULL;
	shi_packet.request_max = sizeof(in_msg);
	shi_packet.request_size = shi_params.sz_request;


#ifdef NPCX_SHI_BYPASS_OVER_256B
	/* Move FRAME_START to second byte */
	out_msg[0] = EC_SPI_PROCESSING;
	out_msg[1] = EC_SPI_FRAME_START;
#else
	/* Put FRAME_START in first byte */
	out_msg[0] = EC_SPI_FRAME_START;
#endif
	shi_packet.response = out_msg + EC_SPI_FRAME_START_LENGTH;

	/* Reserve space for frame start and trailing past-end byte */
	shi_packet.response_max = SHI_MAX_RESPONSE_SIZE;
	shi_packet.response_size = 0;
	shi_packet.driver_result = EC_RES_SUCCESS;

	/* Go to common-layer to handle request */
	host_packet_receive(&shi_packet);
}

/* Parse header for version of spi-protocol */
static void shi_parse_header(void)
{
	/* We're now inside a transaction */
	state = SHI_STATE_RECEIVING;
	DEBUG_CPRINTF("RV-");

	/* Setup deadline time for receiving */
	shi_params.rx_deadline = k_uptime_get() + SHI_CMD_RX_TIMEOUT_US;

	/* Wait for version, command, length bytes */
	if (!shi_read_inbuf_wait(3))
		return shi_bad_received_data();

	if (in_msg[0] == EC_HOST_REQUEST_VERSION) {
		/* Protocol version 3 */
		struct ec_host_request *r = (struct ec_host_request *) in_msg;
		int pkt_size;
		/*
		 * If request is over 32 bytes,
		 * we need to modified the algorithm again.
		 */
		ASSERT(sizeof(*r) < SHI_IBUF_HALF_SIZE);

		/* Wait for the rest of the command header */
		if (!shi_read_inbuf_wait(sizeof(*r) - 3))
			return shi_bad_received_data();

		/* Check how big the packet should be */
		pkt_size = host_request_expected_size(r);
		if (pkt_size == 0 || pkt_size > sizeof(in_msg))
			return shi_bad_received_data();

		/* Computing total bytes need to receive */
		shi_params.sz_request = pkt_size;

		shi_handle_host_package();
	} else {
		/* Invalid version number */
		return shi_bad_received_data();
	}
}

/*****************************************************************************/
/* IC specific low-level driver */
/* This routine fills out all SHI output buffer with status byte */
static void shi_fill_out_status(uint8_t status)
{
	uint8_t start, end;
	uint8_t *fill_ptr;
	uint8_t *fill_end;
	uint8_t *obuf_end;

	/* Disable interrupts in case the interfere by the other interrupts */
	interrupt_disable();

	/*
	 * Fill out output buffer with status byte and leave a gap for PREAMBLE.
	 * The gap guarantees the synchronization. The critical section should
	 * be done within this gap. No racing happens.
	 */
	start = SHI_OBUF_VALID_OFFSET;
	end = ((start + SHI_OBUF_FULL_SIZE - SHI_OUT_PREAMBLE_LENGTH)
	       % SHI_OBUF_FULL_SIZE);

	fill_ptr = (uint8_t *)SHI_OBUF_START_ADDR + start;
	fill_end = (uint8_t *)SHI_OBUF_START_ADDR + end;
	obuf_end = (uint8_t *)SHI_OBUF_START_ADDR + SHI_OBUF_FULL_SIZE;
	while (fill_ptr != fill_end) {
		*(fill_ptr++) = status;
		if (fill_ptr == obuf_end)
			fill_ptr = (uint8_t *)SHI_OBUF_START_ADDR;
	}

	/* End of critical section */
	interrupt_enable();
}

 /*
  * This routine configures at which level the Input Buffer Half Full 2(IBHF2))
  * event triggers an interrupt to core.
  */
static void shi_sec_ibf_int_enable(int enable)
{
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);

	if (enable) {
		/* Setup IBUFLVL2 threshold and enable it */
		inst->SHICFG5 |= BIT(NPCX_SHICFG5_IBUFLVL2DIS);
		SET_FIELD(inst->SHICFG5, NPCX_SHICFG5_IBUFLVL2_FIELD,
				SHI_IBUFLVL2_THRESHOLD);
		inst->SHICFG5 &= ~BIT(NPCX_SHICFG5_IBUFLVL2DIS);
		/* Enable IBHF2 event */
		inst->EVENABLE2 |= BIT(NPCX_EVENABLE2_IBHF2EN);
	} else {
		/* Disable IBHF2 event first */
		inst->EVENABLE2 &= ~BIT(NPCX_EVENABLE2_IBHF2EN);
		/* Disable IBUFLVL2 and set threshold back to zero */
		inst->SHICFG5 |= BIT(NPCX_SHICFG5_IBUFLVL2DIS);
		SET_FIELD(inst->SHICFG5, NPCX_SHICFG5_IBUFLVL2_FIELD, 0);
	}
}

/*
 * This routine write SHI next half output buffer from msg buffer
 */
static void shi_write_half_outbuf(void)
{
	const uint8_t size = MIN(SHI_OBUF_HALF_SIZE,
				 shi_params.sz_response -
				 shi_params.sz_sending);
	uint8_t *obuf_ptr = (uint8_t *)shi_params.tx_buf;
	const uint8_t *obuf_end = obuf_ptr + size;
	uint8_t *msg_ptr = shi_params.tx_msg;

	/* Fill half output buffer */
	while (obuf_ptr != obuf_end)
		*(obuf_ptr++) = *(msg_ptr++);

	shi_params.sz_sending += size;
	shi_params.tx_buf = obuf_ptr;
	shi_params.tx_msg = msg_ptr;
}


/*
 * This routine write SHI output buffer from msg buffer over halt of it.
 * It make sure we have enought time to handle next operations.
 */
static void shi_write_first_pkg_outbuf(uint16_t szbytes)
{
	uint8_t size, offset;
	uint8_t *obuf_ptr;
	uint8_t *obuf_end;
	uint8_t *msg_ptr;

#ifdef NPCX_SHI_BYPASS_OVER_256B
	/*
	 * If response package is across 256 bytes boundary,
	 * bypass needs to extend PROCESSING bytes after reaching the boundary.
	 */
	if (shi_params.bytes_in_256b + SHI_OBUF_FULL_SIZE + szbytes
	    > SHI_BYPASS_BOUNDARY) {
		state = SHI_STATE_WAIT_ALIGNMENT;
		/* Set pointer of output buffer to the start address */
		shi_params.tx_buf = SHI_OBUF_START_ADDR;
		DEBUG_CPRINTF("WAT-");
		return;
	}
#endif

	/* Start writing at our current OBUF position */
	offset = SHI_OBUF_VALID_OFFSET;
	obuf_ptr = (uint8_t *)SHI_OBUF_START_ADDR + offset;
	msg_ptr = shi_params.tx_msg;

	/* Fill up to OBUF mid point, or OBUF end */
	size = MIN(SHI_OBUF_HALF_SIZE - (offset % SHI_OBUF_HALF_SIZE),
					szbytes - shi_params.sz_sending);
	obuf_end = obuf_ptr + size;
	while (obuf_ptr != obuf_end)
		*(obuf_ptr++) = *(msg_ptr++);

	/* Track bytes sent for later accounting */
	shi_params.sz_sending += size;

	/* Write data to beginning of OBUF if we've reached the end */
	if (obuf_ptr == SHI_OBUF_FULL_ADDR)
		obuf_ptr = (uint8_t *)SHI_OBUF_START_ADDR;

	/* Fill next half output buffer */
	size = MIN(SHI_OBUF_HALF_SIZE, szbytes - shi_params.sz_sending);
	obuf_end = obuf_ptr + size;
	while (obuf_ptr != obuf_end)
		*(obuf_ptr++) = *(msg_ptr++);

	/* Track bytes sent / last OBUF position written for later accounting */
	shi_params.sz_sending += size;
	shi_params.tx_buf = obuf_ptr;
	shi_params.tx_msg = msg_ptr;
}

/* This routine copies SHI half input buffer data to msg buffer */
static void shi_read_half_inbuf(void)
{
	/*
	 * Copy to read buffer until reaching middle/top address of
	 * input buffer or completing receiving data
	 */
	do {
		/* Restore data to msg buffer */
		*(shi_params.rx_msg++) = *(shi_params.rx_buf++);
		shi_params.sz_received++;
	} while (shi_params.sz_received % SHI_IBUF_HALF_SIZE
		&& shi_params.sz_received != shi_params.sz_request);
}

/*
 * This routine read SHI input buffer to msg buffer until
 * we have received a certain number of bytes
 */
static int shi_read_inbuf_wait(uint16_t szbytes)
{
	uint16_t i;

	/* Copy data to msg buffer from input buffer */
	for (i = 0; i < szbytes; i++, shi_params.sz_received++) {
		/*
		 * If input buffer pointer equals pointer which wants to read,
		 * it means data is not ready.
		 */
		while (shi_params.rx_buf == (uint8_t *)SHI_IBUF_CUR_ADDR) {
			if (k_uptime_get() > shi_params.rx_deadline)
				return 0;
		}
		/* Restore data to msg buffer */
		*(shi_params.rx_msg++) = *(shi_params.rx_buf++);
	}
	return 1;
}

/* Read pointer of input or output buffer by consecutive reading */
static uint8_t shi_read_buf_pointer(void)
{
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);
	uint8_t stat;

	/* Wait for two consecutive equal values are read */
	do {
		stat = inst->IBUFSTAT;
	} while (stat != inst->IBUFSTAT);

	return stat;
}

/* This routine handles shi recevied unexcepted data */
static void shi_bad_received_data(void)
{
	uint16_t i;

	/* State machine mismatch, timeout, or protocol we can't handle. */
	shi_fill_out_status(EC_SPI_RX_BAD_DATA);
	state = SHI_STATE_BAD_RECEIVED_DATA;

	CPRINTF("BAD-");
	CPRINTF("in_msg=[");
	for (i = 0; i < shi_params.sz_received; i++)
		CPRINTF("%02x ", in_msg[i]);
	CPRINTF("]\n");

	/* Reset shi's state machine for error recovery */
	shi_reset_prepare();

	DEBUG_CPRINTF("END\n");
}


/*
 * Avoid spamming the console with prints every IBF / IBHF interrupt, if
 * we find ourselves in an unexpected state.
 */
static int last_error_state = -1;

static void log_unexpected_state(char *isr_name)
{
#if !(DEBUG_SHI)
	if (state != last_error_state)
		CPRINTS("Unexpected state %d in %s ISR", state, isr_name);
#endif
	last_error_state = state;
}

static void shi_handle_cs_assert(void)
{
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);

	/* If not enabled, ignore glitches on SHI_CS_L */
	if (state == SHI_STATE_DISABLED)
		return;

	/* SHI V2 module filters cs glitch by hardware automatically */

	/* NOT_READY should be sent and there're no spi transaction now. */
	if (state == SHI_STATE_CNL_RESP_NOT_RDY)
		return;

	/* Chip select is low = asserted */
	if (state != SHI_STATE_READY_TO_RECV) {
		/* State machine should be reset in EVSTAT_EOR ISR */
		CPRINTS("Unexpected state %d in CS ISR", state);
		return;
	}

	DEBUG_CPRINTF("CSL-");

	/*
	 * Clear possible EOR event from previous transaction since it's
	 * irrelevant now that CS is re-asserted.
	 */
	inst->EVSTAT = BIT(NPCX_EVSTAT_EOR);

	/* Do not deep sleep during SHI transaction */
	disable_sleep(SLEEP_MASK_SPI);

}


/* This routine handles all interrupts of this module */
static void shi_int_handler(const struct device *dev)
{
	ARG_UNUSED(dev);
	uint8_t stat_reg;
	uint8_t stat2_reg;
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);

	/* Read status register and clear interrupt status early */
	stat_reg = inst->EVSTAT;
	inst->EVSTAT = stat_reg;
	stat2_reg = inst->EVSTAT2;

	/* SHI CS pin is asserted in EVSTAT2 */
	if (IS_BIT_SET(stat2_reg, NPCX_EVSTAT2_CSNFE)) {
		/* clear CSNFE bit */
		inst->EVSTAT2 = BIT(NPCX_EVSTAT2_CSNFE);
		DEBUG_CPRINTF("CSNFE-");
		/*
		 * BUSY bit is set when SHI_CS is asserted. If not, leave it for
		 * SHI_CS de-asserted event.
		 */
		if (!IS_BIT_SET(inst->SHICFG2, NPCX_SHICFG2_BUSY)) {
			DEBUG_CPRINTF("CSNB-");
			return;
		}
		shi_handle_cs_assert();
	}

	/*
	 * End of data for read/write transaction. ie SHI_CS is deasserted.
	 * Host completed or aborted transaction
	 */
	/*
	 * EOR has the limitation that it will not be set even if the SHI_CS is
	 * deasserted without SPI clocks. The new SHI module introduce the
	 * CSNRE bit which will be set when SHI_CS is deasserted regardless of
	 * SPI clocks.
	 */
	if (IS_BIT_SET(stat2_reg, NPCX_EVSTAT2_CSNRE)) {
		/* Clear pending bit of CSNRE */
		inst->EVSTAT2 = BIT(NPCX_EVSTAT2_CSNRE);
		/*
		 * We're not in proper state.
		 * Mark not ready to abort next transaction
		 */
		DEBUG_CPRINTF("CSH-");
		/*
		 * If the buffer is still used by the host command.
		 * Change state machine for response handler.
		 */
		if (state == SHI_STATE_PROCESSING) {
			/*
			 * Mark not ready to prevent the other
			 * transaction immediately
			 */
			shi_fill_out_status(EC_SPI_NOT_READY);

			state = SHI_STATE_CNL_RESP_NOT_RDY;

			/*
			 * Disable SHI interrupt, it will remain disabled
			 * until shi_send_response_packet() is called and
			 * CS is asserted for a new transaction.
			 */
			irq_disable(cros_shi_cfg.irq); /* Instead of  task_disable_irq(NPCX_IRQ_SHI) */


			DEBUG_CPRINTF("CNL-");
			return;
		/* Next transaction but we're not ready */
		} else if (state == SHI_STATE_CNL_RESP_NOT_RDY)
			return;

		/* Error state for checking*/
		if (state != SHI_STATE_SENDING)
			log_unexpected_state("CSNRE");

		/* reset SHI and prepare to next transaction again */
		shi_reset_prepare();
		DEBUG_CPRINTF("END\n");
		return;
	}

	/*
	 * Indicate input/output buffer pointer reaches the half buffer size.
	 * Transaction is processing.
	 */
	if (IS_BIT_SET(stat_reg, NPCX_EVSTAT_IBHF)) {
		if (state == SHI_STATE_RECEIVING) {
			/* Read data from input to msg buffer */
			shi_read_half_inbuf();
			return shi_handle_host_package();
		} else if (state == SHI_STATE_SENDING) {
			/* Write data from msg buffer to output buffer */
			if (shi_params.tx_buf == SHI_OBUF_START_ADDR +
					SHI_OBUF_FULL_SIZE) {
				/* Write data from bottom address again */
				shi_params.tx_buf = SHI_OBUF_START_ADDR;
				return shi_write_half_outbuf();
			} else /* ignore it */
				return;
		} else if (state == SHI_STATE_PROCESSING) {
			/* Wait for host to handle request */
		}
#ifdef NPCX_SHI_BYPASS_OVER_256B
		else if (state == SHI_STATE_WAIT_ALIGNMENT) {
			/*
			 * If pointer of output buffer will reach 256 bytes
			 * boundary soon, start to fill response data.
			 */
			if (shi_params.bytes_in_256b == SHI_BYPASS_BOUNDARY -
					SHI_OBUF_FULL_SIZE) {
				state = SHI_STATE_SENDING;
				DEBUG_CPRINTF("SND-");
				return shi_write_half_outbuf();
			}
		}
#endif
		else
			/* Unexpected status */
			log_unexpected_state("IBHF");
	}

	/*
	 * The size of input buffer reaches the size of
	 * protocol V3 header(=8) after CS asserted.
	 */
	if (IS_BIT_SET(stat2_reg, NPCX_EVSTAT2_IBHF2)) {
		/* Clear IBHF2 */
		inst->EVSTAT2 = BIT(NPCX_EVSTAT2_IBHF2);
		DEBUG_CPRINTF("HDR-");
		/* Disable second IBF interrupt and start to parse header */
		shi_sec_ibf_int_enable(0);
		shi_parse_header();
	}

	/*
	 * Indicate input/output buffer pointer reaches the full buffer size.
	 * Transaction is processing.
	 */
	if (IS_BIT_SET(stat_reg, NPCX_EVSTAT_IBF)) {
#ifdef NPCX_SHI_BYPASS_OVER_256B
		/* Record the sent bytes within 256B boundary */
		shi_params.bytes_in_256b = (shi_params.bytes_in_256b +
				SHI_OBUF_FULL_SIZE) % SHI_BYPASS_BOUNDARY;
#endif
		if (state == SHI_STATE_RECEIVING) {
			/* read data from input to msg buffer */
			shi_read_half_inbuf();
			/* Read to bottom address again */
			shi_params.rx_buf = (uint8_t *)SHI_IBUF_START_ADDR;
			return shi_handle_host_package();
		} else if (state == SHI_STATE_SENDING)
			/* Write data from msg buffer to output buffer */
			if (shi_params.tx_buf == SHI_OBUF_START_ADDR +
					SHI_OBUF_HALF_SIZE)
				return shi_write_half_outbuf();
			else /* ignore it */
				return;
		else if (state == SHI_STATE_PROCESSING
#ifdef NPCX_SHI_BYPASS_OVER_256B
				|| state == SHI_STATE_WAIT_ALIGNMENT
#endif
				)
			/* Wait for host handles request */
			return;
		else
			/* Unexpected status */
			log_unexpected_state("IBF");
	}
}


/*****************************************************************************/
/* Hook functions for chipset and initialization */

/*
 * Reset SHI bus and prepare next transaction
 * Please make sure it is executed when there're no spi transactions
 */
static void shi_reset_prepare(void)
{
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);
	uint16_t i;

	/* We no longer care about SHI interrupts, so disable them. */
	irq_disable(cros_shi_cfg.irq); /* Instead of  task_disable_irq(NPCX_IRQ_SHI) */

	/* Disable SHI unit to clear all status bits */
	inst->SHICFG1 &= ~BIT(NPCX_SHICFG1_EN);

	/* Initialize parameters of next transaction */
	shi_params.rx_msg = in_msg;
	shi_params.tx_msg = out_msg;
	shi_params.rx_buf = (uint8_t *)SHI_IBUF_START_ADDR;
	shi_params.tx_buf = (uint8_t *)SHI_OBUF_HALF_ADDR;
	shi_params.sz_received = 0;
	shi_params.sz_sending = 0;
	shi_params.sz_request = 0;
	shi_params.sz_response = 0;
#ifdef NPCX_SHI_BYPASS_OVER_256B
	shi_params.bytes_in_256b = 0;
#endif
	/* Record last IBUFSTAT for glitch case */
	shi_params.pre_ibufstat = shi_read_buf_pointer();

	/*
	 * Fill output buffer to indicate we`re
	 * ready to receive next transaction.
	 */
	for (i = 1; i < SHI_OBUF_FULL_SIZE; i++)
		inst->OBUF[i] = EC_SPI_RECEIVING;
	inst->OBUF[0] = EC_SPI_OLD_READY;

	/* Enable SHI & WEN functionality */
	inst->SHICFG1 = 0x85;

	/* Ready to receive */
	state = SHI_STATE_READY_TO_RECV;
	last_error_state = -1;

	/* Setup second IBF interrupt and enable SHI's interrupt */
	shi_sec_ibf_int_enable(1);
	irq_enable(cros_shi_cfg.irq); /* Instead of task_enable_irq(NPCX_IRQ_SHI) */

	/* Allow deep sleep at the end of SHI transaction */
	enable_sleep(SLEEP_MASK_SPI);

	DEBUG_CPRINTF("RDY-");
}

static int shi_enable(const struct device *unused)
{
	shi_reset_prepare();

	npcx_miwu_irq_disable(&cros_shi_cfg.shi_cs_wui);
	/* Configure pin-mux from GPIO to SHI. */
	npcx_pinctrl_mux_configure(cros_shi_cfg.alts_list, cros_shi_cfg.alts_size, 1);

	NVIC_ClearPendingIRQ(DT_INST_IRQN(0));
	npcx_miwu_irq_enable(&cros_shi_cfg.shi_cs_wui);
	irq_enable(DT_INST_IRQN(0));

	printk("CROS SHI enable\n");

	/*
	 * If CS is already asserted prior to enabling our GPIO interrupt then
	 * we have missed the falling edge and we need to handle the
	 * deassertion interrupt.
	 */
	irq_enable(cros_shi_cfg.irq); /* Instead of task_enable_irq(NPCX_IRQ_SHI) */

	return 0;
}

/* TODO: Implement HOOK_CHIPSET_RESUME_INIT/HOOK_CHIPSET_RESUME later */
#if 0
#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
DECLARE_HOOK(HOOK_CHIPSET_RESUME_INIT, shi_enable, HOOK_PRIO_DEFAULT);
#else
DECLARE_HOOK(HOOK_CHIPSET_RESUME, shi_enable, HOOK_PRIO_DEFAULT);
#endif
#else
SYS_INIT(shi_enable, POST_KERNEL, 20);
#endif

/* TODO: Implement _sysjump/shi_disable later */
#if 0
static void shi_reenable_on_sysjump(void)
{
#if !(DEBUG_SHI)
	if (system_jumped_late() && chipset_in_state(CHIPSET_STATE_ON))
#endif
		shi_enable();
}
/* Call hook after chipset sets initial power state */
DECLARE_HOOK(HOOK_INIT,
	     shi_reenable_on_sysjump,
	     HOOK_PRIO_INIT_CHIPSET + 1);

/* Disable SHI bus */
static void shi_disable(void)
{
	state = SHI_STATE_DISABLED;

	irq_disable(cros_shi_cfg.irq); /* Instead of  task_disable_irq(NPCX_IRQ_SHI) */

	/* Disable SHI_CS_L interrupt */
	gpio_disable_interrupt(GPIO_SHI_CS_L);

	/* Restore SHI_CS_L back to default state */
	gpio_reset(GPIO_SHI_CS_L);

	/*
	 * Mux SHI related pins
	 * SHI_SDI SHI_SDO SHI_CS# SHI_SCLK are selected to GPIO
	 */
	CLEAR_BIT(NPCX_DEVALT(ALT_GROUP_C), NPCX_DEVALTC_SHI_SL);

	/*
	 * Allow deep sleep again in case CS dropped before ec was
	 * informed in hook function and turn off SHI's interrupt in time.
	 */
	enable_sleep(SLEEP_MASK_SPI);
}
#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND_COMPLETE, shi_disable, HOOK_PRIO_DEFAULT);
#else
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, shi_disable, HOOK_PRIO_DEFAULT);
#endif
DECLARE_HOOK(HOOK_SYSJUMP, shi_disable, HOOK_PRIO_DEFAULT);
#endif

static int shi_init(const struct device *unused)
{
	struct shi_reg *const inst = (struct shi_reg *)(cros_shi_cfg.base);
	const struct device *const clk_dev =
		device_get_binding(NPCX_CLK_CTRL_NAME);
		int ret;

	const struct npcx_alt shi_ver_ctrl[] = {
		{
			.group = SHI_VER_CTRL_ALT_FILED(group),
			.bit = SHI_VER_CTRL_ALT_FILED(bit),
			.inverted = SHI_VER_CTRL_ALT_FILED(inv)
		}
	};

	/* Enable the new version of SHI hardware module. */
	npcx_pinctrl_mux_configure(shi_ver_ctrl, 1, 1);


	/* Turn on shi device clock first */
	ret = clock_control_on(clk_dev,
			       (clock_control_subsys_t *)&cros_shi_cfg.clk_cfg);
	if (ret < 0) {
		LOG_ERR("Turn on sHI clock fail %d", ret);
	}


	/*
	 * SHICFG1 (SHI Configuration 1) setting
	 * [7] - IWRAP	= 1: Wrap input buffer to the first address
	 * [6] - CPOL	= 0: Sampling on rising edge and output on falling edge
	 * [5] - DAS	= 0: return STATUS reg data after Status command
	 * [4] - AUTOBE	= 0: Automatically update the OBES bit in STATUS reg
	 * [3] - AUTIBF	= 0: Automatically update the IBFS bit in STATUS reg
	 * [2] - WEN    = 0: Enable host write to input buffer
	 * [1] - Reserved 0
	 * [0] - ENABLE	= 0: Disable SHI at the beginning
	 */
	inst->SHICFG1 = 0x80;

	/*
	 * SHICFG2 (SHI Configuration 2) setting
	 * [7] - Reserved 0
	 * [6] - REEVEN = 0: Restart events are not used
	 * [5] - Reserved 0
	 * [4] - REEN   = 0: Restart transactions are not used
	 * [3] - SLWU   = 0: Seem-less wake-up is enabled by default
	 * [2] - ONESHOT= 0: WEN is cleared at the end of a write transaction
	 * [1] - BUSY   = 0: SHI bus is busy 0: idle.
	 * [0] - SIMUL	= 1: Turn on simultaneous Read/Write
	 */
	inst->SHICFG2 = 0x01;

	/*
	 * EVENABLE (Event Enable) setting
	 * [7] - IBOREN = 0: Input buffer overrun interrupt enable
	 * [6] - STSREN = 0: status read interrupt disable
	 * [5] - EOWEN  = 0: End-of-Data for Write Transaction Interrupt Enable
	 * [4] - EOREN  = 1: End-of-Data for Read Transaction Interrupt Enable
	 * [3] - IBHFEN = 1: Input Buffer Half Full Interrupt Enable
	 * [2] - IBFEN  = 1: Input Buffer Full Interrupt Enable
	 * [1] - OBHEEN = 0: Output Buffer Half Empty Interrupt Enable
	 * [0] - OBEEN  = 0: Output Buffer Empty Interrupt Enable
	 */
	inst->EVENABLE = 0x1C;

	/*
	 * EVENABLE2 (Event Enable 2) setting
	 * [2] - CSNFEEN = 1: SHI_CS Falling Edge Interrupt Enable
	 * [1] - CSNREEN = 1: SHI_CS Rising Edge Interrupt Enable
	 * [0] - IBHF2EN = 0: Input Buffer Half Full 2 Interrupt Enable
	 */
	inst->EVENABLE2 = 0x06;

	/* Clear SHI events status register */
	inst->EVSTAT = 0XFF;

	/* SHI interrupt configuration */
	npcx_miwu_interrupt_configure(&cros_shi_cfg.shi_cs_wui, NPCX_MIWU_MODE_EDGE,
				      NPCX_MIWU_TRIG_LOW);
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    shi_int_handler, NULL, 0);

	return 0;
}
/* Call hook before chipset sets initial power state and calls resume hooks */
SYS_INIT(shi_init, POST_KERNEL, 10);

/**
 * Get protocol information
 */
static enum ec_status shi_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	memset(r, 0, sizeof(*r));
	r->protocol_versions = BIT(3);
	r->max_request_packet_size = SHI_MAX_REQUEST_SIZE;
	r->max_response_packet_size = SHI_MAX_RESPONSE_SIZE;
	r->flags = EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO, shi_get_protocol_info,
EC_VER_MASK(0));


/* SHI register structure check */
NPCX_REG_SIZE_CHECK(shi_reg, 0x120);
NPCX_REG_OFFSET_CHECK(shi_reg, SHICFG1, 0x001);
NPCX_REG_OFFSET_CHECK(shi_reg, EVENABLE, 0x005);
NPCX_REG_OFFSET_CHECK(shi_reg, IBUFSTAT, 0x00a);
NPCX_REG_OFFSET_CHECK(shi_reg, EVENABLE2, 0x010);
NPCX_REG_OFFSET_CHECK(shi_reg, OBUF, 0x020);
NPCX_REG_OFFSET_CHECK(shi_reg, IBUF, 0x0a0);
