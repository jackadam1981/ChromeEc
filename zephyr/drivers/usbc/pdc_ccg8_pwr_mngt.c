/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Infineon CCG8 Power Delivery Controller Driver
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/smf.h>

#include <drivers/pdc_ccg8_pwr_mngt.h>

#define DT_DRV_COMPAT infineon_pdc_ccg8

#define DEBUG 0

#define OPM_FLAGS_RESPONSE_PENDING      0
#define OPM_FLAGS_UCSI_DATA_PENDING     1
#define OPM_FLAGS_CANCEL_COMMAND	2

#define OPM_INTERRPT		BIT(0)
#define PORT0_INTERRUPT		BIT(1)
#define PORT1_INTERRUPT		BIT(2)
#define UCSI_READ_INTERRUPT	BIT(7)

union hpi_pd_response_t {
	struct {
		uint32_t type		: 1;
		uint32_t code		: 7;
		uint32_t len		: 24;
	};
	uint32_t raw_value;
};

enum ucsi_command_t {
	NO_COMMAND                      = 0x00, /* not part of the ucsi spec */
	PPM_RESET                       = 0x01,
	CANCEL                          = 0x02,
	CONNECTOR_RESET                 = 0x03,
	ACK_CC_CI                       = 0x04,
	SET_NOTIFICATION_ENABLE         = 0x05,
	GET_CAPABILITY                  = 0x06,
	GET_CONNECTOR_CAPABILITY        = 0x07,
	SET_CCOM                        = 0x08,
	SET_UOR                         = 0x09,
	SET_PDM                         = 0x0A,
	SET_PDR                         = 0x0B,
	GET_ALTERNATE_MODES             = 0x0C,
	GET_CAM_SUPPORTED               = 0x0D,
	GET_CURRENT_CAM                 = 0x0E,
	SET_NEW_CAM                     = 0x0F,
	GET_PDOS                        = 0x10,
	GET_CABLE_PROPERTY              = 0x11,
	GET_CONNECTOR_STATUS            = 0x12,
	GET_ERROR_STATUS                = 0x13,
	SET_POWER_LEVEL			= 0x14,
	GET_PD_MESSAGE			= 0x15,
	GET_ATTENTION_VDO		= 0x16,
	GET_CAM_CS			= 0x18,
	LPM_FW_UPDATE_REQUEST		= 0x19,
	SECURITY_REQUEST		= 0x1A,
	SET_RETIMER_MODE		= 0x1B,
	SET_SINK_PATH			= 0x1C,
};

enum hpi_reset_cmd_t {
	RESET_I2C	= 0x00,
	RESET_DEVICE,
};

enum hpi_control_cmd_t {
	START_UCSI      = 0x01,
	STOP_UCSI,
	SILENCE_UCSI,
	SIGNAL_CONNECT_EVENT_TO_THE_OS,
};

enum hpi_pd_control_cmd_t {
	SET_TC_DEFAULT_PROFILE,
	SET_TC_1_5A_PROFILE,
	SET_TC_3_0A_PROFILE,
	TRIGGER_DR_SWAP			= 0x05,
	TRIGGER_PR_SWAP			= 0x06,
	PORT_DISABLE			= 0x11,
};

enum opm_state_t {
	OPM_INIT,
	OPM_IDLE,
	OPM_WAIT_ACK,
	OPM_WAIT_COMMAND_COMPLETION,
	OPM_CANCEL,
	CMD_PPM_RESET,
	CMD_CONNECTOR_RESET,
	CMD_SET_NOTIFICATION_ENABLE,
	CMD_GET_CAPABILITY,
	CMD_GET_CONNECTOR_CAPABILITY,
	CMD_SET_CCOM,
	CMD_SET_UOR,
	CMD_SET_PDM,
	CMD_SET_PDR,
	CMD_GET_ALTERNATE_MODES,
	CMD_GET_CAM_SUPPORTED,
	CMD_GET_CURRENT_CAM,
	CMD_SET_NEW_CAM,
	CMD_GET_PDOS,
	CMD_GET_CABLE_PROPERTY,
	CMD_GET_CONNECTOR_STATUS,
	CMD_GET_ERROR_STATUS,
	CMD_SET_POWER_LEVEL,
        CMD_GET_PD_MESSAGE,
        CMD_GET_ATTENTION_VDO,
        CMD_GET_CAM_CS,
        CMD_LPM_FW_UPDATE_REQUEST,
        CMD_SECURITY_REQUEST,
        CMD_SET_RETIMER_MODE,
        CMD_SET_SINK_PATH,
};

static const struct smf_state opm_states[];

//LOG_MODULE_REGISTER(INTEL_ALTMODE, LOG_LEVEL_ERR);

struct ucsi_data {
	struct smf_ctx ctx;
	const struct device *dev;
	atomic_t flags;
	union notification_enable_t notification_enable;
        union cci_event_t cci;
        enum ucsi_command_t command;
        uint16_t counter;
        uint16_t response;
	union hpi_pd_response_t pd_response[2];
        uint16_t version;
        uint8_t msg_in[16];
        uint8_t msg_out[16];
        uint8_t local_state;
	union cc_operation_mode_t ccom;
	union uor_t uor;
	union pdr_t pdr;
        enum port_t port;
	enum port_reset_t reset_type;
	struct pdo_t *pdos;

	bool partner_pdo;
	enum pdo_offset_t pdo_offset;
	uint8_t pdo_num;
	enum power_role_t prole;
	enum source_caps_t source_caps_type;

	bool snk_fet_enable;

	struct connector_status_t *conn_status;
	struct error_status_t *error_status;

        uint8_t *device_caps;
        uint8_t *port_caps[2];

	uint32_t pdo[2];

	union hpi_pd_status_t pd_status;
	union hpi_tc_status_t tc_status;
};

struct pdc_power_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/*
	 * PD interrupt to wake the task to configure alternate modes. There
	 * can be individual Interrupt pin for each PD port or all the PD
	 * interrupts can be muxed to single GPIO. This helps to keep common
	 * code for single port / dual port PD solutions offered by different
	 * PD vendors.
	 */
	struct gpio_dt_spec int_gpio;
};

struct pdc_power_data {
	const struct device *dev;
	struct k_work work;
	struct ucsi_data ucsi;
	struct gpio_callback gpio_cb;
	pdc_port_handler_cb_t port_handler_cb;
	pdc_cci_handler_cb_t cci_handler_cb;
	uint32_t x;
};

#if DEBUG
/* List of human readable state names for console debugging */
static const char *const opm_state_names[] = {
	[OPM_INIT] = "INIT",
	[OPM_IDLE] = "IDLE",
	[OPM_WAIT_ACK] = "WAIT_ACK",
	[OPM_WAIT_COMMAND_COMPLETION] = "WAIT_COMMAND_COMPLETION",
	[OPM_CANCEL] = "CANCEL",
	[CMD_PPM_RESET] = "PPM_RESET",
	[CMD_CONNECTOR_RESET] = "CONNECTOR_RESET",
	[CMD_SET_NOTIFICATION_ENABLE] = "SET_NOTIFICATION_ENABLE",
	[CMD_GET_CAPABILITY] = "GET_CAPABILITY",
	[CMD_GET_CONNECTOR_CAPABILITY] = "GET_CONNECTOR_CAPABILITY",

	[CMD_SET_CCOM] = "SET_CCOM",
	[CMD_SET_UOR] = "SET_UOR",
	[CMD_SET_PDM] = "SET_PDM",
	[CMD_SET_PDR] = "SET_PDR",
	[CMD_GET_ALTERNATE_MODES] = "GET_ALTERNATE_MODES",
	[CMD_GET_CAM_SUPPORTED] = "GET_CAM_SUPPORTED",
	[CMD_GET_CURRENT_CAM] = "GET_CURRENT_CAM",
	[CMD_SET_NEW_CAM] = "SET_NEW_CAM",
	[CMD_GET_PDOS] = "GET_PDOS",
	[CMD_GET_CABLE_PROPERTY] = "GET_CABLE_PROPERTY",
	[CMD_GET_CONNECTOR_STATUS] = "GET_CONNECTOR_STATUS",
	[CMD_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
	[CMD_SET_POWER_LEVEL] = "SET_POWER_LEVEL",
        [CMD_GET_PD_MESSAGE] = "GET_PD_MESSAGE",
        [CMD_GET_ATTENTION_VDO] = "GET_ATTENTION_VDO",
        [CMD_GET_CAM_CS] = "GET_CAM_CS",
        [CMD_LPM_FW_UPDATE_REQUEST] = "LPM_FW_UPDATE_REQUEST",
        [CMD_SECURITY_REQUEST] = "SECURITY_REQUEST",
        [CMD_SET_RETIMER_MODE] = "SET_RETIMER_MODE",
        [CMD_SET_SINK_PATH] = "SET_SINK_PATH",
};
#endif

void init_start_handler(struct k_timer *unused);
K_TIMER_DEFINE(init_timer, init_start_handler, NULL);

static void set_opm_state(struct ucsi_data *opm, const enum opm_state_t next_state)
{
	smf_set_state(SMF_CTX(opm), &opm_states[next_state]);
}

static enum opm_state_t get_opm_state(struct ucsi_data *opm)
{
	return opm->ctx.current - &opm_states[0];
}

static void print_current_opm_state(struct ucsi_data *opm)
{
#if DEBUG
	printk("OPM: %s\n", opm_state_names[get_opm_state(opm)]);
#endif
}


static int ccgx_hpi_reset(const struct device *dev, enum hpi_reset_cmd_t cmd)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
	uint8_t buf[4];

	buf[0] = 0x08;
	buf[1] = 0x00;
	buf[2] = 'R'; /* CCGx valid signature */
	buf[3] = cmd;

	msg.buf = buf;
	msg.len = 4;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int ccgx_hpi_set_sink_path(const struct device *dev, enum port_t port, bool ctrl, bool en)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
	uint8_t buf[3];

	buf[0] = 0x32;
	if (port) {
		buf[1] = 0x20;
	} else {
		buf[1] = 0x10;
	}

	if (ctrl) {
		buf[2] = en ? 0x03 : 0x01;
	} else {
		buf[2] = 0x00;
	}

	msg.buf = buf;
	msg.len = 3;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int ccgx_hpi_pdport_enable(const struct device *dev, bool en)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
	uint8_t buf[3];

	buf[0] = 0x2c;
	buf[1] = 0x00;
	buf[2] = en ? 0x03 : 0x00;

	msg.buf = buf;
	msg.len = 3;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);

}

static int ccgx_hpi_wo_control(const struct device *dev, enum hpi_control_cmd_t cmd)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
	uint8_t buf[3];

	buf[0] = 0x39;
	buf[1] = 0x00;
	buf[2] = cmd;

	msg.buf = buf;
	msg.len = 3;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int ccgx_hpi_ro_dev_response(const struct device *dev, uint16_t *response)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x7e;
	buf[1] = 0x00;

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		(uint8_t *)response, 2);
}

static int ccgx_hpi_ro_pd_response(const struct device *dev, uint8_t port, uint32_t *response)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x00;
	if (port) {
		buf[1] = 0x24;
	} else {
		buf[1] = 0x14;
	}

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		(uint8_t *)response, 4);
}

static int ccgx_hpi_ro_vbus_voltage(const struct device *dev, uint8_t port, uint16_t *vbus)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x0D;
	if (port) {
		buf[1] = 0x20;
	} else {
		buf[1] = 0x10;
	}

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		(uint8_t *)vbus, 2);
}

static int ccgx_hpi_ro_pd_status(const struct device *dev, uint8_t port, uint32_t *status)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x08;
	if (port) {
		buf[1] = 0x20;
	} else {
		buf[1] = 0x10;
	}

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		status, 4);
}

static int ccgx_hpi_ro_tc_status(const struct device *dev, uint8_t port, uint8_t *status)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x0C;
	if (port) {
		buf[1] = 0x20;
	} else {
		buf[1] = 0x10;
	}

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		status, 1);
}

static int ccgx_hpi_rd_interrupt(const struct device *dev, uint8_t *interrupt)
{
	const struct pdc_power_config *cfg = dev->config;
        uint8_t buf[2];

        if (interrupt == NULL) {
                return -1;
        }

        buf[0] = 0x06;
        buf[1] = 0x00;

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		interrupt, 1);
}

static int ccgx_hpi_clr_interrupt(const struct device *dev, uint8_t interrupt)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
        uint8_t buf[3];

        /* Clear the interrupt register */
        buf[0] = 0x06;
        buf[1] = 0x00;
        buf[2] = interrupt;

        msg.buf = buf;
        msg.len = 3;
        msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int ccgx_hpi_ro_current_pdo(const struct device *dev, uint8_t port, uint32_t *pdo)
{
	const struct pdc_power_config *cfg = dev->config;
	uint8_t buf[2];

	buf[0] = 0x010;
	if (port) {
		buf[1] = 0x20;
	} else {
		buf[1] = 0x10;
	}

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		buf, 2,
		pdo, 4);
}

static int pdc_ucsi_cci(const struct device *dev, uint32_t *cci)
{
	const struct pdc_power_config *cfg = dev->config;
        uint8_t buf[2];

        buf[0] = 0x04;
        buf[1] = 0xf0;

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
                buf, 2,
                (uint8_t *)cci, 4);
}

static int pdc_ucsi_control(const struct device *dev, enum ucsi_command_t cmd, uint8_t dlen, uint8_t *data)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *dat = dev->data;
        struct ucsi_data *ucsi = &dat->ucsi;
	struct i2c_msg msg;
        uint8_t buf[10] = {0};

        ucsi->cci.raw_value = 0;

        buf[0] = 0x08;
        buf[1] = 0xf0;
        buf[2] = cmd;
        buf[3] = dlen;

        if (data != NULL) {
                memcpy(&buf[4], data, 6);
        }

        msg.buf = buf;
        msg.len = 10;
        msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;
 
        return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int pdc_ucsi_cancel(const struct device *dev)
{
        return pdc_ucsi_control(dev, CANCEL, 0, NULL);
}

static int pdc_ucsi_set_sink_path(const struct device *dev, enum port_t port, bool en)
{
	uint8_t data[6] = {0};

	data[0] = (port + 1) | (en << 7);

        return pdc_ucsi_control(dev, SET_SINK_PATH, 0, data);
}

static int pdc_ucsi_set_notification_enable(const struct device *dev, uint16_t bits)
{
        uint8_t data[6] = {0};

        *(uint16_t *)data = bits;

        return pdc_ucsi_control(dev, SET_NOTIFICATION_ENABLE, 0, data);
}

static int pdc_ucsi_get_capability(const struct device *dev)
{
        return pdc_ucsi_control(dev, GET_CAPABILITY, 0, NULL);
}

static int pdc_ucsi_get_connector_capability(const struct device *dev, uint8_t port)
{
        uint8_t data[6] = {0};

        data[0] = port + 1;

        return pdc_ucsi_control(dev, GET_CONNECTOR_CAPABILITY, 0, data);
}

static int pdc_ucsi_get_connector_status(const struct device *dev, uint8_t port)
{
	uint8_t data[6] = {0};

	data[0] = port + 1;

	return pdc_ucsi_control(dev, GET_CONNECTOR_STATUS, 0, data);
}

static int pdc_ucsi_get_error_status(const struct device *dev, uint8_t port)
{
	uint8_t data[6] = {0};

	data[0] = port + 1;

	return pdc_ucsi_control(dev, GET_ERROR_STATUS, 0, data);
}

static int pdc_ucsi_connector_reset(const struct device *dev, uint8_t port, enum port_reset_t type)
{
        uint8_t data[6] = {0};

        data[0] = port + 1;
	data[0] |= (type) ? 0 : 0x80;

        return pdc_ucsi_control(dev, CONNECTOR_RESET, 0, data);
}

static int pdc_ucsi_set_ccom(const struct device *dev, uint8_t port, union cc_operation_mode_t ccom)
{
	uint8_t data[6] = {0};

	*(uint16_t *)&data[0] = (port + 1) | (ccom.raw_value << 7);

	return pdc_ucsi_control(dev, SET_CCOM, 0, data);
}

static int pdc_ucsi_set_uor(const struct device *dev, uint8_t port, union uor_t uor)
{
	uint8_t data[6] = {0};

	*(uint16_t *)&data[0] = (port + 1) | (uor.raw_value << 7);

	return pdc_ucsi_control(dev, SET_UOR, 0, data);
}

static int pdc_ucsi_set_pdr(const struct device *dev, uint8_t port, union pdr_t pdr)
{
	uint8_t data[6] = {0};

	*(uint16_t *)&data[0] = (port + 1) | (pdr.raw_value << 7);

	return pdc_ucsi_control(dev, SET_PDR, 0, data);
}


static int pdc_ucsi_ack_cc_ci(const struct device *dev, enum ack_cc_ci_t bits)
{
        uint8_t data[6] = {0};

	data[0] = bits;

        return pdc_ucsi_control(dev, ACK_CC_CI, 0, data);
}

static int pdc_ucsi_ppm_reset(const struct device *dev)
{
        uint8_t data[6] = {0};

        return pdc_ucsi_control(dev, PPM_RESET, 0, data);
}

static int pdc_ucsi_get_pdos(const struct device *dev, enum port_t port,
			     bool partner_pdo, enum pdo_offset_t offset,
			     uint8_t num, enum power_role_t prole,
			     enum source_caps_t sc)
{
	uint8_t data[6] = {0};

	data[0] = port | (partner_pdo) ? 0x80 : 0;
	data[1] = offset;
	data[2] = (sc << 3) | (prole << 2) | num;

	return pdc_ucsi_control(dev, GET_PDOS, 0, data);
}

static int pdc_ucsi_msg_out(const struct device *dev, uint8_t dlen, uint8_t *data)
{
	const struct pdc_power_config *cfg = dev->config;
	struct i2c_msg msg;
        uint8_t buf[18] = {0};

        buf[0] = 0x20;
        buf[1] = 0xf0;

        if (data != NULL) {
                if (dlen > 16) {
                        return -1;
                }

                memcpy(&buf[2], data, dlen);
        }

        msg.buf = buf;
        msg.len = 18;
        msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;
 
        return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int pdc_ucsi_msg_in(const struct device *dev, uint8_t *data)
{
	const struct pdc_power_config *cfg = dev->config;
        uint8_t buf[2];

        buf[0] = 0x10;
        buf[1] = 0xf0;

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
                buf, 2,
                data, 16);
}

void trigger_handler(struct k_work *work)
{
	k_work_submit(work);
}

static void opm_init_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;
	struct pdc_power_data *data = opm->dev->data;
	uint8_t interrupt;

	print_current_opm_state(opm);

	ccgx_hpi_rd_interrupt(opm->dev, &interrupt);
	ccgx_hpi_clr_interrupt(opm->dev, interrupt);

	opm->command = NO_COMMAND;
	opm->response = 0;
	opm->local_state = 0;
	opm->counter = 0;
	trigger_handler(&data->work);	
}

static void opm_init_run(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	switch (opm->local_state) {
	/* HACK: let PDC Alt Mode driver start first before starting this driver */
	case 0:
		opm->counter++;
		if (opm->counter == 1000) {
			opm->counter = 0;
			opm->local_state++;
		}
		break;
	case 1:
		/* Disable the PDC ports */
		ccgx_hpi_pdport_enable(opm->dev, 0);
		opm->local_state++;
		break;
	case 2:
		/* Wait until the PDC ports are disabled */
		if (atomic_test_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING)) {
			atomic_clear_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING);
			if (opm->response == 0x02) {
				opm->local_state++;
			} else {
				/* port disable command failed. Try again */
				opm->local_state = 0;
			}
		}
		break;
	case 3:
		/* Reset the the PDC */
		ccgx_hpi_reset(opm->dev, RESET_DEVICE);
		opm->local_state++;
		break;
	case 4:
		/* Wait until the PDC is reset */
		if (atomic_test_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING)) {
			atomic_clear_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING);
			if (opm->response == 0x80) {
				/* Reset Complete, and ports are enabled */
				opm->local_state++;
			} else {
				/* reset failed. Try again */
				opm->local_state = 0;
			}
		}
		break;
	case 5:
		/* State UCSI */
		ccgx_hpi_wo_control(opm->dev, START_UCSI);
		opm->local_state++;
		break;
	case 6:
		/* Wait until UCSI starts */
		if (atomic_test_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING)) {
			atomic_clear_bit(&opm->flags, OPM_FLAGS_RESPONSE_PENDING);
			if (opm->response == 0x02 || /* SUCCESS */
			    opm->response == 0x15) { /* Already started */
			    	printk("\n**PDC UCSI Started\n\n");
				set_opm_state(opm, OPM_IDLE);
				return;
			} else { /* Command failed */
	//			printk("\n**Failed to start PDC: %x\n", opm->response);
				/* Try again */
				opm->counter = 4;
			}
		}
		opm->counter++;
		if (opm->counter >= 4) {
			/* Try again */
			opm->local_state = 5;
			return;
		}
		break;
	}
}

static void opm_idle_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;
	print_current_opm_state(o);

	opm->command = NO_COMMAND;
}

static void opm_idle_run(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        switch (opm->command) {
        case NO_COMMAND:
                break;
        case PPM_RESET:
                set_opm_state(opm, CMD_PPM_RESET);
                return;
        case CANCEL:
                break;
        case CONNECTOR_RESET:
		set_opm_state(opm, CMD_CONNECTOR_RESET);
                break;
        case ACK_CC_CI:
                break;
        case SET_NOTIFICATION_ENABLE:
                set_opm_state(opm, CMD_SET_NOTIFICATION_ENABLE);
                return;
        case GET_CAPABILITY:
                set_opm_state(opm, CMD_GET_CAPABILITY);
                break;
        case GET_CONNECTOR_CAPABILITY:
                set_opm_state(opm, CMD_GET_CONNECTOR_CAPABILITY);
                break;
	case SET_CCOM:
		set_opm_state(opm, CMD_SET_CCOM);
                break;
        case SET_UOR:
		set_opm_state(opm, CMD_SET_UOR);
                break;
        case SET_PDM:
                break;
        case SET_PDR:
		set_opm_state(opm, CMD_SET_PDR);
                break;
        case GET_ALTERNATE_MODES:
                break;
        case GET_CAM_SUPPORTED:
                break;
        case GET_CURRENT_CAM:
                break;
        case SET_NEW_CAM:
                break;
        case GET_PDOS:
                break;
        case GET_CABLE_PROPERTY:
                break;
        case GET_CONNECTOR_STATUS:
		set_opm_state(opm, CMD_GET_CONNECTOR_STATUS);
                break;
        case GET_ERROR_STATUS:
                break;
        case SET_POWER_LEVEL:
		break;
	case GET_PD_MESSAGE:
		break;
	case GET_ATTENTION_VDO:
		break;
	case GET_CAM_CS:
		break;
	case LPM_FW_UPDATE_REQUEST:
		break;
	case SECURITY_REQUEST:
		break;
	case SET_RETIMER_MODE:
		break;
	case SET_SINK_PATH:
		set_opm_state(opm, CMD_SET_SINK_PATH);
		break;
	}
}

static void opm_wait_ack_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        print_current_opm_state(opm);

        /* write 16 zeros to msg out */
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_ack_cc_ci(opm->dev, COMMAND_COMPLETED_ACK);
}

static void opm_wait_ack_run(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	if (!atomic_test_and_clear_bit(&opm->flags, OPM_FLAGS_UCSI_DATA_PENDING)) {
		return;
	}

	if (opm->cci.acknowledge_command) {
		set_opm_state(opm, OPM_IDLE);
	}
}

static void opm_wait_command_completion_run(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	/*
	 * Wait until data is received from the PDC
	 */
	if (!atomic_test_and_clear_bit(&opm->flags, OPM_FLAGS_UCSI_DATA_PENDING)) {
		return;
	}

        /*
         * Receive Notification from PDC with CCI Command Completed Indicator set
         * and Cancel Completed Indicator set if the command was a Cancel Command
         */
        if (opm->cci.command_completed) {
                set_opm_state(opm, OPM_WAIT_ACK);
        }
        /*
         * If PPM responded with Busy Indicator and OPM determines that it needs to cancel
         */
        else if (opm->cci.busy &
			atomic_test_and_clear_bit(&opm->flags, OPM_FLAGS_CANCEL_COMMAND)) {
		set_opm_state(opm, OPM_CANCEL);
        }
        /*
         * Delay Completion – Receive Notification from PPM with CCI Busy Indicator set
         */
        /* Stay here */
}

static void opm_cancel_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        print_current_opm_state(opm);

	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_cancel(opm->dev);
}

static void cmd_ppm_reset_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);

	/* write 16 zeros to msg out */
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	/* reset PPM */
	pdc_ucsi_ppm_reset(opm->dev);
}

static void cmd_ppm_reset_run(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        /* wait for ppm reset to complete */
        if (opm->cci.raw_value & 0x08000000) {
                set_opm_state(opm, OPM_IDLE);
                return;
        }
}

static void cmd_set_notification_enable_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        /* write 16 zeros to msg out */
        pdc_ucsi_msg_out(opm->dev, 0, NULL);
        pdc_ucsi_set_notification_enable(opm->dev, opm->notification_enable.raw_value);
}

static void cmd_get_capability_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        print_current_opm_state(opm);

        /* write 16 zeros to msg out */
        pdc_ucsi_msg_out(opm->dev, 0, NULL);

        /* send get capability message */
        pdc_ucsi_get_capability(opm->dev);
}

static void cmd_get_capability_exit(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        memcpy(opm->device_caps, opm->msg_in, 16);
}

static void cmd_get_connector_capability_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        print_current_opm_state(opm);
        /* write 16 zeros to msg out */
        pdc_ucsi_msg_out(opm->dev, 0, NULL);
        pdc_ucsi_get_connector_capability(opm->dev, opm->port);
}

static void cmd_connector_capability_exit(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

        memcpy(opm->port_caps[opm->port], opm->msg_in, 16);
}

static void cmd_connector_reset_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_connector_reset(opm->dev, opm->port, opm->reset_type);
}

static void cmd_set_ccom_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_set_ccom(opm->dev, opm->port, opm->ccom);
}

static void cmd_set_uor_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_set_uor(opm->dev, opm->port, opm->uor);
}

static void cmd_set_pdr_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_set_pdr(opm->dev, opm->port, opm->pdr);
}

static void cmd_set_pdm_entry(void *o)
{}

static void cmd_get_alternate_modes_entry(void *o)
{}
static void cmd_get_cam_supported_entry(void *o)
{}
static void cmd_get_current_cam_entry(void *o)
{}
static void cmd_set_new_cam_entry(void *o)
{}

static void cmd_get_pdos_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_get_pdos(opm->dev,
			  opm->port,
                          opm->partner_pdo,
			  opm->pdo_offset,
                          opm->pdo_num,
			  opm->prole,
                          opm->source_caps_type);
}

static void cmd_get_pdos_exit(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	memcpy(opm->pdos, opm->msg_in, 16);
}

static void cmd_get_cable_property_entry(void *o)
{}

static void cmd_get_connector_status_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_get_connector_status(opm->dev, opm->port);
}

static void cmd_get_connector_status_exit(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	memcpy(opm->conn_status, opm->msg_in, 16);
}

static void cmd_get_error_status_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);

	pdc_ucsi_get_error_status(opm->dev, opm->port);
}

static void cmd_get_error_status_exit(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	memcpy(opm->error_status, opm->msg_in, 16);
}


static void cmd_set_power_level_entry(void *o)
{}
static void cmd_get_pd_message_entry(void *o)
{}
static void cmd_get_attention_vdo_entry(void *o)
{}
static void cmd_get_cam_cs_entry(void *o)
{}
static void cmd_lpm_fw_update_request_entry(void *o)
{}
static void cmd_security_request_entry(void *o)
{}
static void cmd_set_retimer_mode_entry(void *o)
{}

static void cmd_set_sink_path_entry(void *o)
{
	struct ucsi_data *opm = (struct ucsi_data *)o;

	print_current_opm_state(opm);
	pdc_ucsi_msg_out(opm->dev, 0, NULL);
	pdc_ucsi_set_sink_path(opm->dev, opm->port, opm->snk_fet_enable);
}


/* Populate state table */
static const struct smf_state opm_states[] = {
	/* Parent States */
        [OPM_WAIT_COMMAND_COMPLETION] = SMF_CREATE_STATE(
		NULL,
		opm_wait_command_completion_run,
		NULL,
		NULL),
        /* Normal States */
        [OPM_INIT] = SMF_CREATE_STATE(
		opm_init_entry,
		opm_init_run,
		NULL,
		NULL),
        [OPM_IDLE] = SMF_CREATE_STATE(
		opm_idle_entry,
		opm_idle_run,
		NULL,
		NULL),
        [OPM_WAIT_ACK] = SMF_CREATE_STATE(
		opm_wait_ack_entry,
		opm_wait_ack_run,
		NULL,
		NULL),
	[OPM_CANCEL] = SMF_CREATE_STATE(
		opm_cancel_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
	[CMD_PPM_RESET] = SMF_CREATE_STATE(
		cmd_ppm_reset_entry,
		cmd_ppm_reset_run,
		NULL,
		NULL),
        [CMD_CONNECTOR_RESET] = SMF_CREATE_STATE(
		cmd_connector_reset_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_NOTIFICATION_ENABLE] = SMF_CREATE_STATE(
                cmd_set_notification_enable_entry,
		NULL,
                NULL,
                &opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CAPABILITY] = SMF_CREATE_STATE(
		cmd_get_capability_entry,
		NULL,
                cmd_get_capability_exit,
               	&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CONNECTOR_CAPABILITY] = SMF_CREATE_STATE(
                cmd_get_connector_capability_entry,
		NULL,
                cmd_connector_capability_exit,
                &opm_states[OPM_WAIT_COMMAND_COMPLETION]),

        [CMD_SET_CCOM] = SMF_CREATE_STATE(
		cmd_set_ccom_entry,
                NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
	[CMD_SET_UOR] = SMF_CREATE_STATE(
                cmd_set_uor_entry,
                NULL,
		NULL,
                &opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_PDM] = SMF_CREATE_STATE(
                cmd_set_pdm_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_PDR] = SMF_CREATE_STATE(
                cmd_set_pdr_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_ALTERNATE_MODES] = SMF_CREATE_STATE(
                cmd_get_alternate_modes_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CAM_SUPPORTED] = SMF_CREATE_STATE(
                cmd_get_cam_supported_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
	[CMD_GET_CURRENT_CAM] = SMF_CREATE_STATE(
                cmd_get_current_cam_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_NEW_CAM] = SMF_CREATE_STATE(
                cmd_set_new_cam_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_PDOS] = SMF_CREATE_STATE(
                cmd_get_pdos_entry,
                NULL,
                cmd_get_pdos_exit,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CABLE_PROPERTY] = SMF_CREATE_STATE(
                cmd_get_cable_property_entry,
                NULL,
                NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CONNECTOR_STATUS] = SMF_CREATE_STATE(
                cmd_get_connector_status_entry,
                NULL,
                cmd_get_connector_status_exit,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_ERROR_STATUS] = SMF_CREATE_STATE(
		cmd_get_error_status_entry,
                NULL,
		cmd_get_error_status_exit,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
	[CMD_SET_POWER_LEVEL] = SMF_CREATE_STATE(
		cmd_set_power_level_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_PD_MESSAGE] = SMF_CREATE_STATE(
		cmd_get_pd_message_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_ATTENTION_VDO] = SMF_CREATE_STATE(
		cmd_get_attention_vdo_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_GET_CAM_CS] = SMF_CREATE_STATE(
		cmd_get_cam_cs_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_LPM_FW_UPDATE_REQUEST] = SMF_CREATE_STATE(
		cmd_lpm_fw_update_request_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SECURITY_REQUEST] = SMF_CREATE_STATE(
		cmd_security_request_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_RETIMER_MODE] = SMF_CREATE_STATE(
		cmd_set_retimer_mode_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
        [CMD_SET_SINK_PATH] = SMF_CREATE_STATE(
		cmd_set_sink_path_entry,
		NULL,
		NULL,
		&opm_states[OPM_WAIT_COMMAND_COMPLETION]),
};

static void ccgx_ucsi_handler(struct k_work *item)
{
	struct pdc_power_data *data = CONTAINER_OF(item, struct pdc_power_data, work);
	struct ucsi_data *opm = &data->ucsi;
	uint8_t interrupt;

	/* Read the interrupt register */
	ccgx_hpi_rd_interrupt(opm->dev, &interrupt);

	if (interrupt) {
		/*  A new response is available in the Device specific RESPONSE register. */
		if (interrupt & OPM_INTERRPT) {
			/* Read device response register */
			ccgx_hpi_ro_dev_response(opm->dev, &opm->response);
			/* Signal the state machine of the pending response */
			atomic_set_bit(&data->ucsi.flags, OPM_FLAGS_RESPONSE_PENDING);
		}

		/* A new response is available in the PORT_0 specific PD_RESPONSE register. */
		if (interrupt & PORT0_INTERRUPT) {
			/* Read the Port 0 PD response register */
			ccgx_hpi_ro_pd_response(opm->dev, 0, &opm->pd_response[0].raw_value);
			/* Read the Port 0 TC Status register */
			ccgx_hpi_ro_tc_status(opm->dev, 0, &opm->tc_status.raw_value);
			/* Read the Port 0 PD Status register */
			ccgx_hpi_ro_pd_status(opm->dev, 0, &opm->pd_status.raw_value);
			/* Read the Port 0 current PDO */
			ccgx_hpi_ro_current_pdo(opm->dev, 0, &opm->pdo[0]);

			/* Call the TCPMv3 port handler */
			if (data->port_handler_cb) {
				data->port_handler_cb(PORT0);
			}
		}

		/* A new response is available in the PORT_1 specific PD_RESPONSE register. */
		if (interrupt & PORT1_INTERRUPT) {
			/* Read the Port 1 PD response register */
			ccgx_hpi_ro_pd_response(opm->dev, 1, &opm->pd_response[1].raw_value);
			/* Read the Port 1 TC Status register */
			ccgx_hpi_ro_tc_status(opm->dev, 1, &opm->tc_status.raw_value);
			/* Read the Port 1 PD Status register */
			ccgx_hpi_ro_pd_status(opm->dev, 1, &opm->pd_status.raw_value);
			/* Read the Port 1 current PDO */
			ccgx_hpi_ro_current_pdo(opm->dev, 1, &opm->pdo[1]);

			/* Call the TCPMv3 port handler */
			if (data->port_handler_cb) {
				data->port_handler_cb(PORT1);
			}
		}

		/* New data available on the UCSI CCI and MESSAGE_IN regions. */
		if (interrupt & UCSI_READ_INTERRUPT) {
			/* Read the USB Type-C Command Status and Connector Change Indication */
			pdc_ucsi_cci(opm->dev, &opm->cci.raw_value);
			/* Read the Message In data */
			pdc_ucsi_msg_in(opm->dev, opm->msg_in);

			if (data->cci_handler_cb) {
				data->cci_handler_cb(opm->cci.connector_change - 1, opm->cci);
			}

			/* Signal the state machine of the pending UCSI Read */
			atomic_set_bit(&data->ucsi.flags, OPM_FLAGS_UCSI_DATA_PENDING);
		}
	}

	/* Clear the interrupts */
	ccgx_hpi_clr_interrupt(opm->dev, interrupt);

	/* Run the state machine */
	smf_run_state(SMF_CTX(opm));

	/* Rerun this handler if the state machine isn't idle or there is another pending interrupt */
	if ((get_opm_state(opm) != OPM_IDLE) ) {
		trigger_handler(&data->work);
	}
}

static int ccgx_set_handler_cb(const struct device *dev,
					pdc_cci_handler_cb_t cci_cb,
					pdc_port_handler_cb_t port_cb)
{
	struct pdc_power_data *data = dev->data;

	data->cci_handler_cb = cci_cb;
	data->port_handler_cb = port_cb;

	return 0;
}

static int ccgx_reset(const struct device *dev)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->command = PPM_RESET;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_cancel(const struct device *dev)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) == OPM_WAIT_COMMAND_COMPLETION) {
		atomic_set_bit(&opm->flags, OPM_FLAGS_CANCEL_COMMAND);
	}

	return 0;
}

static int ccgx_port_reset(const struct device *dev, enum port_t port, enum port_reset_t type)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->port = port;
	opm->reset_type = type;
	opm->command = CONNECTOR_RESET;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_set_sink_path(const struct device *dev, enum port_t port, bool en)
{
#if 0
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->snk_fet_enable = en;
	opm->command = SET_SINK_PATH;
	trigger_handler(&data->work);

	return 0;
#endif
	/* FIXME: Calling the HPI command because UCSI didn't work */
	return ccgx_hpi_set_sink_path(dev, port, true, en);
}

static int ccgx_set_notification_enable(const struct device *dev, union notification_enable_t bits)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->notification_enable.raw_value = bits.raw_value;
	opm->command = SET_NOTIFICATION_ENABLE;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_get_capability(const struct device *dev, struct device_capability_t *caps)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->device_caps = (uint8_t *)caps;
	opm->command = GET_CAPABILITY;
	trigger_handler(&data->work);
	return 0;

}

static int ccgx_get_port_capability(const struct device *dev, enum port_t port, union port_capability_t *caps)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->port = port;
	opm->port_caps[port] = (uint8_t *)caps;
	opm->command = GET_CONNECTOR_CAPABILITY;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_get_connector_status(const struct device *dev, enum port_t port, struct connector_status_t *cs)
{
        struct pdc_power_data *data = dev->data;
        struct ucsi_data *opm = &data->ucsi;

        if (get_opm_state(opm) != OPM_IDLE) {
                return -EBUSY;
        }

        opm->port = port;
        opm->conn_status = cs;
        opm->command = GET_CONNECTOR_STATUS;
	trigger_handler(&data->work);
        return 0;
}

static int ccgx_get_error_status(const struct device *dev, enum port_t port, struct error_status_t *es)
{
        struct pdc_power_data *data = dev->data;
        struct ucsi_data *opm = &data->ucsi;

        if (get_opm_state(opm) != OPM_IDLE) {
                return -EBUSY;
        }

        opm->port = port;
        opm->error_status = es;
        opm->command = GET_ERROR_STATUS;
	trigger_handler(&data->work);
        return 0;
}

static int ccgx_get_pdos(const struct device *dev, enum port_t port,
			     bool partner_pdo, enum pdo_offset_t offset,
			     uint8_t num, enum power_role_t prole,
			     enum source_caps_t sc, struct pdo_t *pdos)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	if (num > 3) {
		return -EDOM;
	}

	opm->port = port;
	opm->partner_pdo = partner_pdo;
	opm->pdo_offset = offset;
	opm->pdo_num = num;
	opm->prole = prole;
	opm->source_caps_type = sc;
	opm->pdos = pdos;

	trigger_handler(&data->work);
	return 0;
}

static int ccgx_read_vbus(const struct device *dev, enum port_t port, uint16_t *vbus)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	return ccgx_hpi_ro_vbus_voltage(opm->dev, opm->port, vbus);
}

static int ccgx_set_ccom(const struct device *dev, uint8_t port, union cc_operation_mode_t ccom)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->port = port;
	opm->ccom = ccom;
	opm->command = SET_CCOM;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_set_uor(const struct device *dev, uint8_t port, union uor_t uor)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->port = port;
	opm->uor = uor;
	opm->command = SET_UOR;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_set_pdr(const struct device *dev, uint8_t port, union pdr_t pdr)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	if (get_opm_state(opm) != OPM_IDLE) {
		return -EBUSY;
	}

	opm->port = port;
	opm->pdr = pdr;
	opm->command = SET_PDR;
	trigger_handler(&data->work);
	return 0;
}

static int ccgx_get_tc_status(const struct device *dev, enum port_t port, uint8_t *status)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	*status = opm->tc_status.raw_value;
	return 0;
}

static int ccgx_get_pd_status(const struct device *dev, enum port_t port, uint32_t *status)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	*status = opm->pd_status.raw_value;
	return 0;
}

static int ccgx_get_current_pdo(const struct device *dev, enum port_t port, uint32_t *pdo)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *opm = &data->ucsi;

	*pdo = opm->pdo[port];
	return 0;
}

static const struct pdc_power_driver_api_t pdc_power_driver_api = {
	.reset = ccgx_reset,
	.cancel = ccgx_cancel,
	.port_reset = ccgx_port_reset,
	.set_notification_enable = ccgx_set_notification_enable,
	.get_capability = ccgx_get_capability,
	.get_port_capability = ccgx_get_port_capability,
	.set_ccom = ccgx_set_ccom,
	.set_uor = ccgx_set_uor,
	.set_pdr = ccgx_set_pdr,
	.set_sink_path = ccgx_set_sink_path,
	.get_connector_status = ccgx_get_connector_status,
	.get_pdos = ccgx_get_pdos,
	.get_error_status = ccgx_get_error_status,

	.read_vbus = ccgx_read_vbus,
	.get_pd_status = ccgx_get_pd_status,
	.get_tc_status = ccgx_get_tc_status,
	.get_current_pdo = ccgx_get_current_pdo,
	.set_handler_cb = ccgx_set_handler_cb,
};

static void pdc_power_gpio_callback(const struct device *dev,
				     struct gpio_callback *cb, uint32_t pins)
{
	struct pdc_power_data *data =
		CONTAINER_OF(cb, struct pdc_power_data, gpio_cb);

	trigger_handler(&data->work);
}

static struct pdc_power_data pdc_power_data_0;

static int pdc_power_init(const struct device *dev)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *data = dev->data;
	int rv;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		printk("I2C is not ready\n");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		printk("GPIO is not ready\n");
		return -ENODEV;
	}

	data->ucsi.flags = ATOMIC_INIT(0);
	data->ucsi.dev = dev;
	data->x = 0;
	data->dev = dev;

	printk("\n**PDC DRIVER IS STARTED\n");

	rv = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	if (rv < 0) {
		printk("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pdc_power_gpio_callback, BIT(cfg->int_gpio.pin));

	k_work_init(&data->work, ccgx_ucsi_handler);

	rv = gpio_add_callback(cfg->int_gpio.port, &data->gpio_cb);
	if (rv < 0) {
		printk("Unable to add callback");
		return rv;
	}

	rv = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_FALLING);
	if (rv < 0) {
		printk("Unable to configure interrupt");
		return rv;
	}

	/* Set initial state */
	smf_set_initial(SMF_CTX(&data->ucsi), &opm_states[OPM_INIT]);
	return 0;
}

#define PDC_POWER_DEFINE(inst)                                            \
	static struct pdc_power_data pdc_power_data_##inst;               \
                                                                          \
	static const struct pdc_power_config pdc_power_config##inst = {	  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
	};                                                                \
                                                                          \
	DEVICE_DT_INST_DEFINE(inst, pdc_power_init, NULL,                 \
			      &pdc_power_data_##inst,                     \
			      &pdc_power_config##inst, POST_KERNEL,       \
			      CONFIG_APPLICATION_INIT_PRIORITY,           \
			      &pdc_power_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_POWER_DEFINE)
