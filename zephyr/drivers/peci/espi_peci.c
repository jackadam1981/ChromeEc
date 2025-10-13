/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/peci.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#define DT_DRV_COMPAT intel_espi_peci

#define ESPI_PECI_PCH_ADDR 0x10U

#define PECI_OOB_CMD_CODE 0x01U

#define ESPI_PECI_DATA_BUF_LEN 32

#define ESPI_PECI_DST_ADDR(addr) ((addr << 1) | 0)
#define ESPI_PECI_SRC_ADDR(addr) ((addr << 1) | 1)

#define OOB_PACKET_HEADER_SIZE 4U
#define OOB_PECI_CMD_HDR_SIZE 4U

/* Includes OOB header + PECI response status */
#define OOB_PECI_MIN_RESP_SIZE (OOB_PECI_CMD_HDR_SIZE + 1)

LOG_MODULE_REGISTER(espi_peci, LOG_LEVEL_INF);

struct espi_oob_peci_hdr {
	uint8_t dest_addr;
	uint8_t cmd_code;
	uint8_t byte_cnt;
	uint8_t src_addr;
} __packed;

struct espi_oob_peci_req {
	struct espi_oob_peci_hdr oob_hdr;
	uint8_t addr;
	uint8_t wr_len;
	uint8_t rd_len;
	uint8_t cmd_code;
	uint8_t data[ESPI_PECI_DATA_BUF_LEN];
} __packed;

struct espi_oob_peci_resp {
	struct espi_oob_peci_hdr oob_hdr;
	uint8_t resp_code;
	uint8_t data[ESPI_PECI_DATA_BUF_LEN];
} __packed;

struct espi_peci_data {
	bool enabled;
};

struct espi_peci_config {
	const struct device *espi_dev;
	uint8_t addr;
};

/* As per the PECI specification, originator need to calculate AWFCS
 * (Assured Write Frame Check Sequence) code using below checksum table.
 */
const uint8_t peci_crc_table[] = {
	0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, /* Offset 0-7 */
	0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, /* Offset 8-15 */
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, /* Offset 16-23 */
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, /* Offset 24-31 */
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, /* Offset 32-39 */
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, /* Offset 40-47 */
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, /* Offset 48-55 */
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, /* Offset 56-63 */
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, /* Offset 64-71 */
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, /* Offset 72-79 */
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, /* Offset 80-87 */
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, /* Offset 88-95 */
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, /* Offset 96-103 */
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A, /* Offset 104-111 */
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, /* Offset 112-119 */
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, /* Offset 120-127 */
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, /* Offset 128-135 */
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4, /* Offset 136-143 */
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, /* Offset 144-151 */
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4, /* Offset 152-159 */
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, /* Offset 160-167 */
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, /* Offset 168-175 */
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, /* Offset 176-183 */
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, /* Offset 184-191 */
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, /* Offset 192-199 */
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, /* Offset 200-207 */
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, /* Offset 208-215 */
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, /* Offset 216-223 */
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, /* Offset 224-231 */
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, /* Offset 232-239 */
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, /* Offset 240-247 */
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3 /* Offset 248-255 */
};

#ifdef CONFIG_ESPI_PECI_TRANSFER_DEBUG_PRINT
#define ESPI_PECI_PRINT_BUF_LEN 128
char print_buf[ESPI_PECI_PRINT_BUF_LEN];
int print_buf_offset;
#endif

static uint8_t peci_calc_awfcs(uint8_t *peci_buffer, uint8_t awfcs_len)
{
	uint8_t peci_awfcs = 0x0;
	uint8_t count = 0x0;

	/* Parameter verification */
	if (peci_buffer == NULL) {
		LOG_ERR("%s(): No memory available for peci buf", __func__);
		return -ENOMEM;
	}

	/* Compute AWFCS code */
	for (count = 0; count < awfcs_len; count++) {
		peci_awfcs = peci_awfcs ^ peci_buffer[count];
		peci_awfcs = peci_crc_table[peci_awfcs];
	}

	/* Invert upper bit to prevent mathematical failure that would
	 * cause the Write FCS to always be 0x0.
	 */
	peci_awfcs ^= BIT(7);

	return peci_awfcs;
}

int espi_peci_config(const struct device *dev, uint32_t bitrate)
{
	return 0;
}

int espi_peci_disable(const struct device *dev)
{
	struct espi_peci_data *data = dev->data;

	data->enabled = false;

	return 0;
}

int espi_peci_enable(const struct device *dev)
{
	struct espi_peci_data *data = dev->data;

	data->enabled = true;

	return 0;
}

#define ESPI_PECI_OOB_RX_BUF_LEN 32

static int ecpi_peci_send(const struct device *dev, struct peci_msg *msg)
{
	const struct espi_peci_config *config = dev->config;
	struct espi_oob_peci_req oob_peci_req;
	struct espi_oob_peci_hdr *oob_hdr = &oob_peci_req.oob_hdr;
	struct espi_oob_packet oob_pckt;

	oob_hdr->dest_addr = ESPI_PECI_DST_ADDR(msg->flags);
	oob_hdr->cmd_code = PECI_OOB_CMD_CODE;

	/*TODO: Set read message lenght accordingly */
	if (msg->cmd_code == PECI_CMD_PING) {
		oob_peci_req.rd_len = 0;
		oob_peci_req.wr_len = 0;
	} else {
		oob_peci_req.rd_len = msg->rx_buffer.len;
		oob_peci_req.wr_len = msg->tx_buffer.len + 1;
	}

	oob_hdr->byte_cnt = OOB_PACKET_HEADER_SIZE + oob_peci_req.wr_len;
	oob_hdr->src_addr = ESPI_PECI_SRC_ADDR(config->addr);
	oob_peci_req.addr = msg->addr;
	oob_peci_req.cmd_code = msg->cmd_code;

	/* Tx length includes peci cmd code. So copy len-1 byte as data */
	if (msg->tx_buffer.len > 1) {
		memcpy(oob_peci_req.data, msg->tx_buffer.buf,
		       msg->tx_buffer.len);
	}

	oob_pckt.buf = (uint8_t *)&oob_peci_req;
	oob_pckt.len = OOB_PACKET_HEADER_SIZE + oob_hdr->byte_cnt - 1;

	if (oob_peci_req.cmd_code == PECI_CMD_WR_PKG_CFG0) {
		oob_pckt.buf[oob_pckt.len - 1] =
			peci_calc_awfcs(oob_pckt.buf, oob_pckt.len - 1);
	}
#ifdef CONFIG_ESPI_PECI_TRANSFER_DEBUG_PRINT
	print_buf_offset = 0;
	for (int i = 0; i < oob_pckt.len; i++) {
		print_buf_offset +=
			snprintf(&print_buf[print_buf_offset],
				 sizeof(print_buf) - print_buf_offset, " %02x",
				 oob_pckt.buf[i]);
		if (print_buf_offset >= ESPI_PECI_PRINT_BUF_LEN) {
			break;
		}
	}
	LOG_INF("Tx Data = %s ", print_buf);
#endif
	return espi_send_oob(config->espi_dev, &oob_pckt);
}

static int ecpi_peci_receive(const struct device *dev, struct peci_msg *msg)
{
	const struct espi_peci_config *config = dev->config;
	struct espi_oob_peci_resp *oob_peci_req;
	struct espi_oob_packet oob_pckt;
	uint8_t oob_rx_buf[ESPI_PECI_OOB_RX_BUF_LEN];
	int ret;

	oob_pckt.len = ESPI_PECI_OOB_RX_BUF_LEN;
	oob_pckt.buf = oob_rx_buf;

	ret = espi_receive_oob(config->espi_dev, &oob_pckt);
	if (ret) {
		LOG_ERR("PECI OOB Rxn failed %d", ret);
		return ret;
	}
#ifdef CONFIG_ESPI_PECI_TRANSFER_DEBUG_PRINT
	print_buf_offset = 0;
	for (int i = 0; i < oob_pckt.len; i++) {
		print_buf_offset +=
			snprintf(&print_buf[print_buf_offset],
				 sizeof(print_buf) - print_buf_offset, " %02x",
				 oob_pckt.buf[i]);
		if (print_buf_offset >= ESPI_PECI_PRINT_BUF_LEN) {
			break;
		}
	}
	LOG_INF("Rx Data = %s ", print_buf);
#endif
	if (oob_pckt.len < OOB_PECI_MIN_RESP_SIZE) {
		LOG_ERR("Received packet too small %d", oob_pckt.len);
		return -EIO;
	}

	oob_peci_req = (struct espi_oob_peci_resp *)oob_pckt.buf;
	switch (oob_peci_req->resp_code) {
	case 0x80:
	case 0x81:
		return -ETIME;
	case 0x90:
		return -EINVAL;
	case 0x91:
		return -ENETDOWN;
	case 0x40:
		break;
	default:
		LOG_ERR("Unknown return code %d", oob_peci_req->resp_code);
		return -EIO;
	}

	/* Copy command response data only */
	if (oob_pckt.len > OOB_PECI_MIN_RESP_SIZE && msg->rx_buffer.buf) {
		memcpy(msg->rx_buffer.buf, oob_rx_buf + OOB_PECI_MIN_RESP_SIZE,
		       oob_pckt.len - OOB_PECI_MIN_RESP_SIZE);
	}
	msg->rx_buffer.len = oob_pckt.len - OOB_PECI_MIN_RESP_SIZE;

	return 0;
}

int espi_peci_transfer(const struct device *dev, struct peci_msg *msg)
{
	struct espi_peci_data *data = dev->data;
	int ret;

	if (!data->enabled) {
		return 0;
	}

	ret = ecpi_peci_send(dev, msg);
	if (ret) {
		LOG_ERR("PECI OOB Txn failed %d", ret);
		return ret;
	}

	return ecpi_peci_receive(dev, msg);
}

struct peci_driver_api espi_peci_api = {
	.config = espi_peci_config,
	.disable = espi_peci_disable,
	.enable = espi_peci_enable,
	.transfer = espi_peci_transfer,
};

void espi_oob_peci_callback(const struct device *dev, struct espi_callback *cb,
			    struct espi_event espi_evt)
{
}

static int espi_peci_init(const struct device *dev)
{
	const struct espi_peci_config *config = dev->config;
	static struct espi_callback peci_cb;

	espi_init_callback(&peci_cb, &espi_oob_peci_callback,
			   ESPI_BUS_EVENT_OOB_RECEIVED);
	espi_add_callback(config->espi_dev, &peci_cb);
	return 0;
}

#define ESPI_PECI_DEFINE(inst)                                           \
	static struct espi_peci_data espi_peci_data_##inst;              \
	static const struct espi_peci_config espi_peci_config_##inst = { \
		.espi_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),            \
		.addr = DT_INST_PROP(inst, peci_addr),                   \
	};                                                               \
	DEVICE_DT_INST_DEFINE(inst, espi_peci_init, NULL,                \
			      &espi_peci_data_##inst,                    \
			      &espi_peci_config_##inst, POST_KERNEL,     \
			      CONFIG_PDC_DRIVER_INIT_PRIORITY,           \
			      &espi_peci_api);

DT_INST_FOREACH_STATUS_OKAY(ESPI_PECI_DEFINE)

#ifdef CONFIG_PLATFORM_EC_ESPI_PECI_CONSOLE
#define peci_dev DEVICE_DT_GET(DT_NODELABEL(ec_peci))

static int cmd_espi_peci_ping(const struct shell *sh, size_t argc, char **argv)
{
	char *e;
	struct peci_msg msg;
	uint8_t ping_resp[2];

	msg.flags = strtoul(argv[1], &e, 0);
	if (*e) {
		return -EINVAL;
	}
	msg.addr = strtoul(argv[2], &e, 0);
	if (*e) {
		return -EINVAL;
	}
	msg.cmd_code = PECI_CMD_PING;
	msg.tx_buffer.len = 0;
	msg.rx_buffer.len = 2;
	msg.rx_buffer.buf = ping_resp;

	espi_peci_enable(peci_dev);

	if (espi_peci_transfer(peci_dev, &msg)) {
		return -EIO;
	}

	shell_fprintf(sh, SHELL_INFO, "Rx Data = [");
	for (int i = 0; i < msg.rx_buffer.len; i++) {
		shell_fprintf(sh, SHELL_INFO, " 0x%X ", msg.rx_buffer.buf[i]);
	}
	shell_fprintf(sh, SHELL_INFO, "]\n");

	return 0;
}

static int cmd_espi_peci_get_dib(const struct shell *sh, size_t argc,
				 char **argv)
{
	char *e;
	struct peci_msg msg;
	uint8_t rx_buf[8] = { 0 };

	msg.flags = strtoul(argv[1], &e, 0);
	if (*e) {
		return -EINVAL;
	}
	msg.addr = strtoul(argv[2], &e, 0);
	if (*e) {
		return -EINVAL;
	}
	msg.cmd_code = PECI_CMD_GET_DIB;
	msg.tx_buffer.len = 0;
	msg.rx_buffer.len = 8;
	msg.rx_buffer.buf = rx_buf;

	espi_peci_enable(peci_dev);

	if (espi_peci_transfer(peci_dev, &msg)) {
		return -EIO;
	}

	shell_fprintf(sh, SHELL_INFO, "Rx Data = [");
	for (int i = 0; i < msg.rx_buffer.len; i++) {
		shell_fprintf(sh, SHELL_INFO, " 0x%X ", msg.rx_buffer.buf[i]);
	}
	shell_fprintf(sh, SHELL_INFO, "]\n");

	return 0;
}

#ifdef CONFIG_PLATFORM_EC_ESPI_PECI_CONSOLE_XFER_RAW
static int cmd_espi_peci_xfer_raw(const struct shell *sh, size_t argc,
				  char **argv)
{
	const struct espi_peci_config *config = peci_dev->config;
	char *e;
	int ret;
	uint8_t rx_buf[32] = { 0 };
	uint8_t tx_buf[32] = { 0 };
	struct espi_oob_packet req_pckt;
	struct espi_oob_packet resp_pckt;

	for (int i = 1; i < argc; i++) {
		tx_buf[i - 1] = strtoul(argv[i], &e, 0);
		if (*e) {
			return -EINVAL;
		}
	}
	req_pckt.buf = (uint8_t *)&tx_buf;
	req_pckt.len = argc - 1;

	espi_peci_enable(peci_dev);

	print_buf_offset = 0;
	for (int i = 0; i < req_pckt.len; i++) {
		print_buf_offset +=
			snprintf(&print_buf[print_buf_offset],
				 sizeof(print_buf) - print_buf_offset, " %02x",
				 req_pckt.buf[i]);
		if (print_buf_offset >= ESPI_PECI_PRINT_BUF_LEN) {
			break;
		}
	}
	shell_fprintf(sh, SHELL_INFO, "Tx Data = %s\n", print_buf);
	ret = espi_send_oob(config->espi_dev, &req_pckt);
	if (ret) {
		shell_fprintf(sh, SHELL_ERROR, "OOB Txn failed %d", ret);
		return ret;
	}

	resp_pckt.buf = (uint8_t *)&rx_buf;
	resp_pckt.len = 32;
	ret = espi_receive_oob(config->espi_dev, &resp_pckt);
	if (ret) {
		shell_fprintf(sh, SHELL_ERROR, "OOB Rxn failed %d", ret);
		return ret;
	}
	print_buf_offset = 0;
	for (int i = 0; i < resp_pckt.len; i++) {
		print_buf_offset +=
			snprintf(&print_buf[print_buf_offset],
				 sizeof(print_buf) - print_buf_offset, " %02x",
				 resp_pckt.buf[i]);
		if (print_buf_offset >= ESPI_PECI_PRINT_BUF_LEN) {
			break;
		}
	}
	shell_fprintf(sh, SHELL_INFO, "Rx Data = %s\n", print_buf);
	return 0;
}
#endif /* CONFIG_PLATFORM_EC_ESPI_PECI_CONSOLE_XFER_RAW */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_espi_peci_cmds,
	SHELL_CMD_ARG(ping, NULL,
		      "Ping device in PECI bus\n"
		      "Usage: espi_peci ping <OOB addr> <PECI addr>",
		      cmd_espi_peci_ping, 3, 0),
	SHELL_CMD_ARG(get_dib, NULL,
		      "Get DIB from PECI device\n"
		      "Usage: espi_peci get_dib <OOB addr> <PECI addr>",
		      cmd_espi_peci_get_dib, 3, 0),
#ifdef CONFIG_PLATFORM_EC_ESPI_PECI_CONSOLE_XFER_RAW
	SHELL_CMD_ARG(xfer_raw, NULL,
		      "Send raw data in PECI bus\n"
		      "Usage: espi_peci xfer_raw [data]",
		      cmd_espi_peci_xfer_raw, 6, 12),
#endif
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(espi_peci, &sub_espi_peci_cmds, "PD commands (deprecated)",
		   NULL);
#endif /* CONFIG_PLATFORM_EC_ESPI_PECI_CONSOLE */
