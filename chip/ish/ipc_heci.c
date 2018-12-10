/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IPC module for ISH */

/**
 * IPC - Inter Processor Communication
 * -----------------------------------
 *
 * IPC is a bi-directional doorbell based message passing interface sans
 * session and transport layers, between hardware blocks. ISH uses IPC to
 * communicate with the Host, PMC (Power Management Controller), CSME
 * (Converged Security and Manageability Engine), Audio, Graphics and ISP.
 *
 * Both the initiator and target ends each have a 32-bit doorbell register and
 * 128-byte message regions. In addition, the following register pairs help in
 * synchronizing IPC.
 *
 *  - Peripheral Interrupt Status Register (PISR)
 *  - Peripheral Interrupt Mask Register (PIMR)
 *  - Doorbell Clear Status Register (DB CSR)
 */

#include "registers.h"
#include "console.h"
#include "task.h"
#include "util.h"
#include "ipc_heci.h"
#include "ish_fwst.h"
#include "queue.h"
#include "hooks.h"

#ifdef IPC_HECI_DEBUG
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)
#else
#define CPUTS(outstr)
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

/*
 * comminucation protocol is defined in Linux Documentation
 * <kernel_root>/Documentation/hid/intel-ish-hid.txt
 */

/* MNG commands */
#define MNG_RX_CMPL_ENABLE              0
#define MNG_RX_CMPL_DISABLE             1
#define MNG_RX_CMPL_INDICATION          2
#define MNG_RESET_NOTIFY                3
#define MNG_RESET_NOTIFY_ACK            4
#define MNG_SYNC_FW_CLOCK               5
#define MNG_ILLEGAL_CMD                 0xFF

/* Peripheral Interrupt Satus Register */
#define IPC_PISR_HOST2ISH_BIT		(1<<0)
#define IPC_PISR_PMC2ISH_BIT		(1<<1)
#define IPC_PISR_CSME2ISH_BIT		(1<<2)

/* Peripheral Interrupt Mask Register */
#define IPC_PIMR_HOST2ISH_BIT		(1<<0)
#define IPC_PIMR_PMC2ISH_BIT		(1<<1)
#define IPC_PIMR_CSME2ISH_BIT		(1<<2)

#define IPC_PIMR_ISH2HOST_CLR_BIT	(1<<11)
#define IPC_PIMR_ISH2PMC_CLR_BIT	(1<<12)
#define IPC_PIMR_ISH2CSME_CLR_BIT	(1<<13)

/* Peripheral Interrupt DB(DoorBell) Clear Status Register */
#define IPC_DB_CLR_STS_ISH2HOST_BIT	(1<<0)
#define IPC_DB_CLR_STS_ISH2ISP_BIT	(1<<2)
#define IPC_DB_CLR_STS_ISH2AUDIO_BIT	(1<<3)
#define IPC_DB_CLR_STS_ISH2PMC_BIT	(1<<8)
#define IPC_DB_CLR_STS_ISH2CSME_BIT	(1<<16)

/* Doorbell */
#define IPC_DB_MSG_LENGTH_FIELD		0x3FF
#define IPC_DB_MSG_LENGTH_SHIFT		0
#define IPC_DB_MSG_LENGTH_MASK \
	  (IPC_DB_MSG_LENGTH_FIELD << IPC_DB_MSG_LENGTH_SHIFT)

#define IPC_DB_PROTOCOL_FIELD		0x0F
#define IPC_DB_PROTOCOL_SHIFT		10
#define IPC_DB_PROTOCOL_MASK (IPC_DB_PROTOCOL_FIELD << IPC_DB_PROTOCOL_SHIFT)

#define IPC_DB_CMD_FIELD		0x0F
#define IPC_DB_CMD_SHIFT		16
#define IPC_DB_CMD_MASK			(IPC_DB_CMD_FIELD << IPC_DB_CMD_SHIFT)

#define IPC_DB_BUSY_SHIFT		31
#define IPC_DB_BUSY_MASK		(1 << IPC_DB_BUSY_SHIFT)

#define IPC_DB_MSG_LENGTH(drbl) \
	  (((drbl) & IPC_DB_MSG_LENGTH_MASK) >> IPC_DB_MSG_LENGTH_SHIFT)
#define IPC_DB_PROTOCOL(drbl) \
	  (((drbl) & IPC_DB_PROTOCOL_MASK) >> IPC_DB_PROTOCOL_SHIFT)
#define IPC_DB_CMD(drbl) \
	  (((drbl) & IPC_DB_CMD_MASK) >> IPC_DB_CMD_SHIFT)
#define IPC_DB_BUSY(drbl)		(!!((drbl) & IPC_DB_BUSY_MASK))

#define IPC_BUILD_DB(length, proto, cmd, busy) \
	(((busy) << IPC_DB_BUSY_SHIFT) | ((cmd) << IPC_DB_CMD_SHIFT) | \
	((proto) << IPC_DB_PROTOCOL_SHIFT) | \
		((length) << IPC_DB_MSG_LENGTH_SHIFT))

#define IPC_BUILD_MNG_DB(cmd, length) \
	IPC_BUILD_DB(length, IPC_PROTOCOL_MNG, cmd, 1)

#define IPC_BUILD_HECI_DB(length) \
	IPC_BUILD_DB(length, IPC_PROTOCOL_HECI, 0, 1)

#define IPC_MSG_MAX_SIZE		0x80
#define IPC_HOST_TX_MSG_QUEUE_SIZE	8
#define IPC_PMC_TX_MSG_QUEUE_SIZE	2
#define IPC_HOST_MNG_RX_QUEUE_SIZE	2
#define IPC_HOST_HECI_RX_QUEUE_SIZE	2

#define IPC_HANDLE_PEER_ID_OFFSET	4
#define IPC_HANDLE_PROTOCOL_OFFSET	0
#define IPC_HANDLE_PROTOCOL_MASK	0x0F
#define IPC_BUILD_HANDLE(peer_id, protocol) \
	((ipc_handle_t)(((peer_id) << IPC_HANDLE_PEER_ID_OFFSET) | (protocol)))
#define IPC_BUILD_MNG_HANDLE(peer_id) \
	IPC_BUILD_HANDLE((peer_id), IPC_PROTOCOL_MNG)
#define IPC_BUILD_HOST_MNG_HANDLE() IPC_BUILD_MNG_HANDLE(IPC_PEER_ID_HOST)
#define IPC_HANDLE_PEER_ID(handle) \
	((uint32_t)(handle) >> IPC_HANDLE_PEER_ID_OFFSET)
#define IPC_HANDLE_PROTOCOL(handle) \
	((uint32_t)(handle) & IPC_HANDLE_PROTOCOL_MASK)
#define IPC_HANDLE_PEER_IF_CTX(handle) \
	(&ipc_peer_ctxs[IPC_HANDLE_PEER_ID(handle)])
#define IPC_IS_VALID_HANDLE(handle) \
	(IPC_HANDLE_PEER_ID(handle) < IPC_PEERS_COUNT && \
	    IPC_HANDLE_PROTOCOL(handle) < IPC_PROTOCOL_COUNT)

struct ipc_msg {
	uint32_t drbl;
	uint8_t payload[IPC_MSG_MAX_SIZE];
} __packed;

struct ipc_rst_payload {
	uint16_t reset_id;
	uint16_t reserved;
};

struct ipc_oob_msg {
	uint32_t address;
	uint32_t length;
};

struct ipc_msg_event {
	task_id_t task_id;	 /* task that needs event for its non-blocking read */
	task_id_t task_id_block; /* task that is in blocking read */
	uint32_t event;
	uint8_t enabled;
	uint8_t read_in_use;	 /* indicate pending read */
	uint8_t wake_up_read;	 /* indicate need to wake up blocked task */

	struct mutex read_lock;
	struct queue rx_queue;
};

/* IPC interface context */
struct ipc_if_ctx {
	uint32_t in_msg_reg;
	uint32_t out_msg_reg;
	uint32_t in_drbl_reg;
	uint32_t out_drbl_reg;
	uint32_t clr_busy_bit;
	uint32_t pimr_2ish_bit;
	uint32_t pimr_2host_clearing_bit;
	uint8_t irq_in;
	uint8_t irq_clr;
	uint16_t reset_id;
	struct ipc_msg_event msg_events[IPC_PROTOCOL_COUNT];
	struct mutex lock;
	struct mutex write_lock;

	struct queue tx_queue;
	uint8_t is_tx_ipc_busy;
	uint8_t initialized;
};

/* Array of peer contexts */
static struct ipc_if_ctx ipc_peer_ctxs[IPC_PEERS_COUNT] = {
	[IPC_PEER_ID_HOST] = {
		.in_msg_reg = IPC_HOST2ISH_MSG_REGS,
		.out_msg_reg = IPC_ISH2HOST_MSG_REGS,
		.in_drbl_reg = IPC_HOST2ISH_DOORBELL,
		.out_drbl_reg = IPC_ISH2HOST_DOORBELL,
		.clr_busy_bit = IPC_DB_CLR_STS_ISH2HOST_BIT,
		.pimr_2ish_bit = IPC_PIMR_HOST2ISH_BIT,
		.pimr_2host_clearing_bit = IPC_PIMR_ISH2HOST_CLR_BIT,
		.irq_in = ISH_IPC_HOST2ISH_IRQ,
		.irq_clr = ISH_IPC_ISH2HOST_CLR_IRQ,
		.msg_events[IPC_PROTOCOL_MNG] = {
			.rx_queue = QUEUE_NULL(IPC_HOST_MNG_RX_QUEUE_SIZE,
					       struct ipc_msg),
		},
		.msg_events[IPC_PROTOCOL_HECI] = {
			.rx_queue = QUEUE_NULL(IPC_HOST_HECI_RX_QUEUE_SIZE,
					       struct ipc_msg),
		},
		.tx_queue = QUEUE_NULL(IPC_HOST_TX_MSG_QUEUE_SIZE,
				       struct ipc_msg),
	},
	/* Other peers (PMC, CSME, etc) to be added when required */
};

static inline struct ipc_if_ctx *ipc_get_if_ctx(const uint32_t peer_id)
{
	return &ipc_peer_ctxs[peer_id];
}

static inline void ipc_enable_pimr_db_interrupt(const struct ipc_if_ctx *ctx)
{
	REG32(IPC_PIMR) |= ctx->pimr_2ish_bit;
}

static inline void ipc_disable_pimr_db_interrupt(const struct ipc_if_ctx *ctx)
{
	REG32(IPC_PIMR) &= ~ctx->pimr_2ish_bit;
}

static inline void ipc_enable_pimr_clearing_interrupt(
						const struct ipc_if_ctx *ctx)
{
	REG32(IPC_PIMR) |= ctx->pimr_2host_clearing_bit;
}

static inline void ipc_disable_pimr_clearing_interrupt(
						const struct ipc_if_ctx *ctx)
{
	REG32(IPC_PIMR) &= ~ctx->pimr_2host_clearing_bit;
}

static void write_payload_and_ring_drbl(const struct ipc_if_ctx *ctx,
					uint32_t drbl,
					const uint8_t *payload,
					size_t payload_size)
{
	uint32_t msg_idx = 0;

	/* write in 32-bits unit */
	while (payload_size >= sizeof(uint32_t)) {
		REG32(ctx->out_msg_reg + msg_idx) =
			*(uint32_t *)(payload + msg_idx);
		msg_idx += sizeof(uint32_t);
		payload_size -= sizeof(uint32_t);
	}

	/* write leftovers in 8-bits unit */
	while (payload_size) {
		REG8(ctx->out_msg_reg + msg_idx) =
			*(uint8_t *)(payload + msg_idx);
		msg_idx++;
		payload_size--;
	}

	REG32(ctx->out_drbl_reg) = drbl;
}


static int ipc_write_raw(struct ipc_if_ctx *ctx, uint32_t drbl,
			 const uint8_t *payload, size_t payload_size)
{
	struct queue *q = &ctx->tx_queue;
	struct ipc_msg *msg;
	size_t tail, space;
	int res = 0;

	mutex_lock(&ctx->write_lock);

	ipc_disable_pimr_clearing_interrupt(ctx);
	if (ctx->is_tx_ipc_busy) {
		space = queue_space(q);
		if (space) {
			tail = q->state->tail & (q->buffer_units - 1);
			msg = (struct ipc_msg *)q->buffer + tail;
			msg->drbl = drbl;
			memcpy(msg->payload, payload, payload_size);
			queue_advance_tail(q, 1);
		} else {
			res = IPC_ERR_QUEUE_FULL;
		}

		ipc_enable_pimr_clearing_interrupt(ctx);
		goto write_unlock;
	}
	ipc_enable_pimr_clearing_interrupt(ctx);

	ctx->is_tx_ipc_busy = 1;
	write_payload_and_ring_drbl(ctx, drbl, payload, payload_size);

write_unlock:
	mutex_unlock(&ctx->write_lock);
	return res;
}

static int ipc_send_notify(const ipc_handle_t handle)
{
	struct ipc_rst_payload *ipc_rst;
	struct ipc_if_ctx *ctx;
	struct ipc_msg msg;

	ctx = IPC_HANDLE_PEER_IF_CTX(handle);
	ctx->reset_id = (uint16_t)ish_fwst_get_reset_id();
	ipc_rst = (struct ipc_rst_payload *)msg.payload;
	ipc_rst->reset_id = ctx->reset_id;

	msg.drbl = IPC_BUILD_MNG_DB(MNG_RESET_NOTIFY, sizeof(*ipc_rst));
	ipc_write_raw(ctx, msg.drbl, msg.payload, IPC_DB_MSG_LENGTH(msg.drbl));

	return 0;
}

static int ipc_send_cmpl_indication(struct ipc_if_ctx *ctx)
{
	struct ipc_msg msg;

	msg.drbl = IPC_BUILD_MNG_DB(MNG_RX_CMPL_INDICATION, 0);
	ipc_write_raw(ctx, msg.drbl, msg.payload, IPC_DB_MSG_LENGTH(msg.drbl));

	return 0;
}

static int ipc_get_protocol_data(const struct ipc_msg *msg,
				 const uint32_t protocol,
				 uint8_t *buf, const size_t buf_size)
{
	int len = 0, payload_size;
	uint8_t *src = NULL, *dest = NULL;
	uint32_t drbl_val;

	drbl_val = msg->drbl;
	payload_size = IPC_DB_MSG_LENGTH(drbl_val);

	if (payload_size > IPC_MAX_PAYLOAD_SIZE)
		return IPC_ERR_INVALID_MSG;

	CPRINTF("ipc p=%d, db=0x%0x, payload_size=%d\n", protocol, drbl_val,
		IPC_DB_MSG_LENGTH(drbl_val));

	switch (protocol) {
	case IPC_PROTOCOL_BOOT:
		break;
	case IPC_PROTOCOL_HECI:
		/* copy only payload which is a heci packet */
		len = payload_size;
		src = (uint8_t *)msg->payload;
		dest = buf;
		break;
	case IPC_PROTOCOL_MCTP:
		break;
	case IPC_PROTOCOL_MNG:
		/* copy including doorbell which forms a ipc packet */
		len = payload_size + sizeof(drbl_val);
		src = (uint8_t *)msg->payload;

		*((uint32_t *)buf) = drbl_val;
		dest = buf + sizeof(drbl_val);
		break;
	case IPC_PROTOCOL_ECP:
		/* TODO: EC protocol */
		break;
	}

	if (len > buf_size)
		return IPC_ERR_TOO_SMALL_BUFFER;

	memcpy(dest, src, payload_size);

	return len;
}

static void handle_msg_recv_interrupt(const uint32_t peer_id)
{
	struct ipc_if_ctx *ctx;
	struct ipc_msg *msg;
	struct queue *q;
	uint32_t drbl_val, payload_size, protocol, invalid_msg = 0;
	size_t tail;

	ctx = ipc_get_if_ctx(peer_id);
	ipc_disable_pimr_db_interrupt(ctx);

	drbl_val = REG32(ctx->in_drbl_reg);
	protocol = IPC_DB_PROTOCOL(drbl_val);
	payload_size = IPC_DB_MSG_LENGTH(drbl_val);

	if (payload_size > IPC_MSG_MAX_SIZE) {
		invalid_msg = 1;
		goto error;
	}

	if (!ctx->msg_events[protocol].enabled) {
		invalid_msg = 2;
		goto error;
	}

	q = &ctx->msg_events[protocol].rx_queue;
	if (!queue_space(q)) {
		invalid_msg = 3; /* TODO: talk to ipc_read() to read out */
		goto error;
	}

	/* write to per-protocol queue */
	tail = q->state->tail & (q->buffer_units - 1);
	msg = (struct ipc_msg *)q->buffer + tail;

	msg->drbl = drbl_val;
	memcpy(msg->payload, (void *)ctx->in_msg_reg, payload_size);
	queue_advance_tail(q, 1);

	/* done with IPC H/W */
	REG32(ctx->in_drbl_reg) = 0;

	/*
	 * send event if there's blocked read
	 * and then see if user asked event. e.g) to support nonblocking read
	 */
	if (ctx->msg_events[protocol].wake_up_read) {
		task_wake(ctx->msg_events[protocol].task_id_block);
		ctx->msg_events[protocol].wake_up_read = 0;
	} else if (ctx->msg_events[protocol].event) {
		task_set_event(ctx->msg_events[protocol].task_id,
			       ctx->msg_events[protocol].event, 0);
	}

	return;

error:	
	if (invalid_msg)
		CPRINTS("discard msg : %d\n", invalid_msg);
}

static void handle_busy_clear_interrupt(const uint32_t peer_id)
{
	struct ipc_if_ctx *ctx;
	struct ipc_msg *msg;
	struct queue *q;
	size_t head;

	ctx = ipc_get_if_ctx(peer_id);
	/*
	 * No need to use sync mechanism here since the accesing the queue
	 * happens only when either this IRQ is disabled or
	 * in ISR context(here) of this IRQ.
	 */
	if (!queue_is_empty(&ctx->tx_queue)) {
		q = &ctx->tx_queue;
		head = q->state->head & (q->buffer_units - 1);
		msg = (struct ipc_msg *)(q->buffer + head * q->unit_bytes);
		write_payload_and_ring_drbl(ctx, msg->drbl, msg->payload,
					    IPC_DB_MSG_LENGTH(msg->drbl));
		queue_advance_head(q, 1);
	} else {
		ctx->is_tx_ipc_busy = 0;
	}

	REG32(IPC_BUSY_CLEAR) = ctx->clr_busy_bit;
}

/**
 * IPC interrupts are received by the FW when a) Host SW rings doorbell and
 * b) when Host SW clears doorbell busy bit [31].
 *
 * Doorbell Register (DB) bits
 * ----+-------+--------+-----------+--------+------------+--------------------
 *  31 | 30 29 |  28-20 |19 18 17 16| 15 14  | 13 12 11 10| 9 8 7 6 5 4 3 2 1 0
 * ----+-------+--------+-----------+--------+------------+--------------------
 * Busy|Options|Reserved|  Command  |Reserved|   Protocol |    Message Length
 * ----+-------+--------+-----------+--------+------------+--------------------
 *
 * ISH Peripheral Interrupt Status Register:
 *  Bit 0 - If set, indicates interrupt was caused by setting Host2ISH DB
 *
 * ISH Peripheral Interrupt Mask Register
 *  Bit 0 - If set, mask interrupt caused by Host2ISH DB
 *
 * ISH Peripheral DB Clear Status Register
 *  Bit 0 - If set, indicates interrupt was caused by clearing Host2ISH DB
 */
static void ipc_host2ish_isr(void)
{
	uint32_t pisr = REG32(IPC_PISR);
	uint32_t pimr = REG32(IPC_PIMR);

	if ((pisr & IPC_PISR_HOST2ISH_BIT) && (pimr & IPC_PIMR_HOST2ISH_BIT))
		handle_msg_recv_interrupt(IPC_PEER_ID_HOST);
}
DECLARE_IRQ(ISH_IPC_HOST2ISH_IRQ, ipc_host2ish_isr);

static void ipc_host2ish_busy_clear_isr(void)
{
	uint32_t busy_clear = REG32(IPC_BUSY_CLEAR);
	uint32_t pimr = REG32(IPC_PIMR);

	if ((busy_clear & IPC_DB_CLR_STS_ISH2HOST_BIT) &&
	    (pimr & IPC_PIMR_ISH2HOST_CLR_BIT))
		handle_busy_clear_interrupt(IPC_PEER_ID_HOST);
}
DECLARE_IRQ(ISH_IPC_ISH2HOST_CLR_IRQ, ipc_host2ish_busy_clear_isr);

int ipc_write(const ipc_handle_t handle, const void *buf, const size_t buf_size)
{
	int ret;
	struct ipc_if_ctx *ctx;
	uint32_t drbl = 0;
	const uint8_t *payload = NULL;
	int payload_size;
	uint32_t protocol;

	if (!IPC_IS_VALID_HANDLE(handle))
		return IPC_ERR_INVALID_HANDLE;

	protocol = IPC_HANDLE_PROTOCOL(handle);
	ctx = IPC_HANDLE_PEER_IF_CTX(handle);

	if (ctx->initialized == 0)
		return IPC_ERR_IPC_IS_NOT_READY;

	if (!ctx->msg_events[protocol].enabled)
		return IPC_ERR_INVALID_HANDLE;

	switch (protocol) {
	case IPC_PROTOCOL_BOOT:
		break;
	case IPC_PROTOCOL_HECI:
		drbl = IPC_BUILD_HECI_DB(buf_size);
		payload = buf;
		break;
	case IPC_PROTOCOL_MCTP:
		break;
	case IPC_PROTOCOL_MNG:
		drbl = ((struct ipc_msg *)buf)->drbl;
		payload = ((struct ipc_msg *)buf)->payload;
		break;
	case IPC_PROTOCOL_ECP:
		/* TODO : EC protocol */
		break;
	}

	payload_size = IPC_DB_MSG_LENGTH(drbl);
	if (payload_size > IPC_MSG_MAX_SIZE)
		return IPC_ERR_TOO_BIG_MSG;

	ret = ipc_write_raw(ctx, drbl, payload, payload_size);
	if (ret)
		return ret;

	return buf_size;
}

ipc_handle_t ipc_open(const enum ipc_peer_id peer_id,
		      const enum ipc_protocol protocol,
		      const uint32_t event)
{
	struct ipc_if_ctx *ctx;

	if (protocol >= IPC_PROTOCOL_COUNT ||
	    peer_id >= IPC_PEERS_COUNT)
		return IPC_INVALID_HANDLE;

	ctx = ipc_get_if_ctx(peer_id);
	mutex_lock(&ctx->lock);
	if (ctx->msg_events[protocol].enabled) {
		mutex_unlock(&ctx->lock);
		return IPC_INVALID_HANDLE;
	}

	ctx->msg_events[protocol].task_id = task_get_current();
	ctx->msg_events[protocol].enabled = 1;
	ctx->msg_events[protocol].event = event;
	queue_init(&ctx->msg_events[protocol].rx_queue);

	if (peer_id == IPC_PEER_ID_HOST &&
	    protocol == IPC_PROTOCOL_HECI && ish_fwst_is_ilup_set())
		ish_fwst_set_hup();

	if (ctx->initialized == 0) {
		task_enable_irq(ctx->irq_in);
		task_enable_irq(ctx->irq_clr);

		ipc_enable_pimr_db_interrupt(ctx);
		ipc_enable_pimr_clearing_interrupt(ctx);

		ctx->initialized = 1;
	}
	mutex_unlock(&ctx->lock);

	return IPC_BUILD_HANDLE(peer_id, protocol);
}

static void handle_mng_commands(const ipc_handle_t handle,
				const struct ipc_msg *msg)
{
	struct ipc_rst_payload *ipc_rst;
	struct ipc_if_ctx *ctx;
	uint32_t peer_id = IPC_HANDLE_PEER_ID(handle);

	ctx = IPC_HANDLE_PEER_IF_CTX(handle);

	switch (IPC_DB_CMD(msg->drbl)) {
	case MNG_RX_CMPL_ENABLE:
	case MNG_RX_CMPL_DISABLE:
	case MNG_RX_CMPL_INDICATION:
	case MNG_RESET_NOTIFY:
		CPRINTS("msg not handled %d\n", IPC_DB_CMD(msg->drbl));
		break;
	case MNG_RESET_NOTIFY_ACK:
		ipc_rst = (struct ipc_rst_payload *)msg->payload;
		if (peer_id == IPC_PEER_ID_HOST &&
		    ipc_rst->reset_id == ctx->reset_id) {
			ish_fwst_set_ilup();
			if (ctx->msg_events[IPC_PROTOCOL_HECI].enabled)
				ish_fwst_set_hup();
		}

		break;
	case MNG_SYNC_FW_CLOCK:
		/* TODO: if there's data to host requires timestamp
		 * we need to implement this
		 */
		CPRINTS("sync fw clock");
		break;
	}
}

/*
 * read out one message from queue
 */
static int do_ipc_read(struct ipc_if_ctx *ctx, struct queue *q,
		       const uint32_t protocol, uint8_t *buf,
		       const size_t buf_size)
{
	struct ipc_msg *msg;
	size_t head;
	int len;

	head = q->state->head & (q->buffer_units - 1);
	msg = (struct ipc_msg *)(q->buffer + head * q->unit_bytes);

	len = ipc_get_protocol_data(msg, protocol, buf, buf_size);

	queue_advance_head(q, 1);

	if (queue_is_empty(q))
		ipc_send_cmpl_indication(ctx);

	return len;
}

static int ipc_check_read_validity(const struct ipc_if_ctx *ctx,
				   const uint32_t protocol)
{
	if (ctx->initialized == 0)
		return IPC_ERR_IPC_IS_NOT_READY;

	if (!ctx->msg_events[protocol].enabled)
		return IPC_ERR_INVALID_HANDLE;

	return 0;
}

int ipc_read(const ipc_handle_t handle, void *buf, const size_t buf_size,
	     int timeout_us)
{
	struct ipc_if_ctx *ctx;
	struct queue *q;
	uint32_t events, protocol;
	int ret = 0;

	if (!IPC_IS_VALID_HANDLE(handle))
		return IPC_ERR_INVALID_HANDLE;

	protocol = IPC_HANDLE_PROTOCOL(handle);
	ctx = IPC_HANDLE_PEER_IF_CTX(handle);

	ret = ipc_check_read_validity(ctx, protocol);
	if (ret)
		return ret;

	/* serialize : only one context per protocol */
	mutex_lock(&ctx->msg_events[protocol].read_lock);

	/*
	 * check if msg is already available in the queue.
	 * should be done with irq disabled.
	 */
	q = &ctx->msg_events[protocol].rx_queue;
	ipc_disable_pimr_db_interrupt(ctx);

	if (!queue_is_empty(q)) {
		/* enable irq if msg is already available */
		ipc_enable_pimr_db_interrupt(ctx);

		ret = do_ipc_read(ctx, q, protocol, buf, buf_size);

		goto queue_read_done;
	}

	/* return immediately if it's nonblocking */
	if (!timeout_us) {
		ret = IPC_ERR_MSG_NOT_AVAILABLE;
		goto error_irq;
	}

	/* only one task can wait for new message. */
	if (ctx->msg_events[protocol].read_in_use) {
		ret = IPC_ERR_IPC_READ_IN_USE;
		goto error_irq;
	}

	/* prepare to block */
	ctx->msg_events[protocol].read_in_use = 1;
	ctx->msg_events[protocol].wake_up_read = 1;
	ctx->msg_events[protocol].task_id_block = task_get_current();

	/* enable irq to get new message */
	ipc_enable_pimr_db_interrupt(ctx);
	mutex_unlock(&ctx->msg_events[protocol].read_lock);

	events = task_wait_event(timeout_us);

	if (!(events & TASK_EVENT_WAKE)) {
		ret = IPC_ERR_MSG_NOT_AVAILABLE;
		goto error;
	}

	ret = do_ipc_read(ctx, q, protocol, buf, buf_size);

error:
	ctx->msg_events[protocol].read_in_use = 0;

	return ret;

queue_read_done:
error_irq:
	ipc_enable_pimr_db_interrupt(ctx);
	mutex_unlock(&ctx->msg_events[protocol].read_lock);

	return ret;
}

/* event flag for MNG msg */
#define EVENT_FLAG_BIT_MNG_MSG			TASK_EVENT_CUSTOM(1)

/*
 * This task handles MNG messages
 */
void ipc_mng_task(void)
{
	int payload_size;
	struct ipc_msg msg;
	ipc_handle_t handle;

	/*
	 * open IPC for MNG protocol.
	 * event to help non-blocking read is not needed.
	 */
	handle = ipc_open(IPC_PEER_ID_HOST, IPC_PROTOCOL_MNG, 0);

	ASSERT(handle != IPC_INVALID_HANDLE);

	ipc_send_notify(handle);

	while (1) {
		payload_size = ipc_read(handle, &msg, sizeof(msg), -1);

		/* allow doorbell with any payload */
		if (payload_size < 0)
			continue; /* TODO: retry several and exit */

		/* handle MNG commands */
		handle_mng_commands(handle, &msg);
	}
}

void ipc_init(void)
{
	int i;
	struct ipc_if_ctx *ctx;

	/* TODO: initialize full ipc_peer_ctxs. for now,
	 * support only IPC_PEER_ID_HOST
	 */
	/* for (i = 0; i < IPC_PEERS_COUNT; i++) { */
	for (i = 0; i <= IPC_PEER_ID_HOST; i++) {
		ctx = ipc_get_if_ctx(i);
		queue_init(&ctx->tx_queue);
	}
}
DECLARE_HOOK(HOOK_INIT, ipc_init, HOOK_PRIO_DEFAULT);
