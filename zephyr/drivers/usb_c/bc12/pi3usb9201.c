/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PI3USB9201 USB BC 1.2 Charger Detector driver. */

#define DT_DRV_COMPAT pericom_pi3usb9201

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

#include <drivers/usb_c/usbc_bc12.h>
#include "pi3usb9201.h"

LOG_MODULE_REGISTER(PI3USB9201, CONFIG_USBC_LOG_LEVEL);

/* Constant configuration data */
struct pi3usb9201_config {
	struct i2c_dt_spec i2c;

	struct gpio_dt_spec irq_gpio;
	struct gpio_dt_spec enable_gpio;

	uint8_t flags;
};

#define PI3USB9201_CLIENT_ONLY BIT(0)

/* Run-time configuratino data */
struct pi3usb9201_data {
	struct k_work work;
	enum bc12_type type;
	struct gpio_callback gpio_cb;

	bc12_callback_t result_callback;
	void *callback_data;
};

enum pi3usb9201_client_sts {
	CHG_OTHER = 0,
	CHG_2_4A,
	CHG_2_0A,
	CHG_1_0A,
	CHG_RESERVED,
	CHG_CDP,
	CHG_SDP,
	CHG_DCP,
};

struct bc12_status {
	enum bc12_type type;
	int current_limit;
};

/*
 * The USB Type-C specification limits the maximum amount of current from BC 1.2
 * suppliers to 1.5A.  Technically, proprietary methods are not allowed, but we
 * will continue to allow those.
 */
static const struct bc12_status bc12_chg_limits[] = {
	[CHG_OTHER] = { BC12_TYPE_PROPRIETARY, 500 },
	[CHG_2_4A] = { BC12_TYPE_PROPRIETARY, BC12_CURR_MA(2400) },
	[CHG_2_0A] = { BC12_TYPE_PROPRIETARY, BC12_CURR_MA(2000) },
	[CHG_1_0A] = { BC12_TYPE_PROPRIETARY, BC12_CURR_MA(1000) },
	[CHG_RESERVED] = { BC12_TYPE_NONE, 0 },
	[CHG_CDP] = { BC12_TYPE_SDP, BC12_CURR_MA(1500) },
	[CHG_SDP] = { BC12_TYPE_SDP, BC12_CURR_MA(500) },
	[CHG_DCP] = { BC12_TYPE_DCP, BC12_CURR_MA(1500) },
};

static inline int raw_read8(const struct device *dev, uint8_t offset,
			    uint8_t *value)
{
	const struct pi3usb9201_config *cfg = dev->config;

	return i2c_reg_read_byte_dt(&cfg->i2c, offset, value);
}

static int pi3usb9201_raw(const struct device *dev, uint8_t reg, uint8_t mask,
			  uint8_t val)
{
	const struct pi3usb9201_config *cfg = dev->config;
	int ret;
	uint8_t old_val;

	/* Clear mask and then set val in i2c reg value */
	ret = i2c_reg_read_byte_dt(&cfg->i2c, reg, &old_val);
	if (!ret) {
		return ret;
	}

	old_val &= ~mask;
	old_val |= val;
	return i2c_reg_write_byte_dt(&cfg->i2c, reg, old_val);
}

static int pi3usb9201_interrupt_enable(const struct device *dev, bool enable)
{
	/* Clear the interrupt mask bit to enable the interrupt */
	return pi3usb9201_raw(dev, PI3USB9201_REG_CTRL_1,
			      PI3USB9201_REG_CTRL_1_INT_MASK,
			      enable ? 0 : PI3USB9201_REG_CTRL_1_INT_MASK);
}

static int pi3usb9201_bc12_detect_ctrl(const struct device *dev, bool enable)
{
	return pi3usb9201_raw(dev, PI3USB9201_REG_CTRL_2,
			      PI3USB9201_REG_CTRL_2_START_DET,
			      enable ? PI3USB9201_REG_CTRL_2_START_DET : 0);
}

static int pi3usb9201_set_mode(const struct device *dev,
			       enum pi3usb9201_mode desired_mode)
{
	return pi3usb9201_raw(dev, PI3USB9201_REG_CTRL_1,
			      PI3USB9201_REG_CTRL_1_MODE_MASK,
			      desired_mode << PI3USB9201_REG_CTRL_1_MODE_SHIFT);
}

static __maybe_unused int pi3usb9201_get_mode(const struct device *dev,
					      enum pi3usb9201_mode *mode)
{
	int rv;
	uint8_t ctrl1;

	rv = raw_read8(dev, PI3USB9201_REG_CTRL_1, &ctrl1);
	if (rv)
		return rv;

	ctrl1 >>= PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	ctrl1 &= PI3USB9201_REG_CTRL_1_MODE_MASK;
	*mode = ctrl1;

	return 0;
}

static int pi3usb9201_get_status(const struct device *dev, uint8_t *client,
				 uint8_t *host)
{
	int rv;
	uint8_t status;

	rv = raw_read8(dev, PI3USB9201_REG_CLIENT_STS, &status);
	if (rv) {
		return rv;
	}

	if (client) {
		*client = status;
	}

	rv = raw_read8(dev, PI3USB9201_REG_HOST_STS, &status);
	if (rv) {
		return rv;
	}

	if (host) {
		*host = status;
	}

	return 0;
}

static void pi3usb9201_notify_callback(const struct device *dev,
				       struct bc12_state *state)
{
	struct pi3usb9201_data *pi3usb9201_data = dev->data;

	if (pi3usb9201_data->result_callback) {
		pi3usb9201_data->result_callback(
			dev, state, pi3usb9201_data->callback_data);
	}
}

static void bc12_update_supplier(const struct device *dev,
				 struct bc12_state *state)
{
	struct pi3usb9201_data *pi3usb9201_data = dev->data;

	/*
	 * If most recent supplier type is not BC12_TYPE_NONE, then notify
	 * the callbacks to clear out the BC1.2 state.
	 */
	if (pi3usb9201_data->type != BC12_TYPE_NONE) {
		pi3usb9201_notify_callback(dev, NULL);
	}

	if (state) {
		/* Now update the current charger type */
		pi3usb9201_data->type = state->type;
		/* If new type != NONE, then notify charge manager */
		if (state->type != BC12_TYPE_NONE) {
			pi3usb9201_notify_callback(dev, state);
		}
	}
}

static void bc12_update_charge_manager(const struct device *dev,
				       int client_status)
{
	struct bc12_state new_chg;
	int bit_pos;

	/* Set charge voltage to 5V */
	new_chg.voltage = BC12_CHARGER_VOLTAGE_MV;

	/*
	 * Find set bit position. Note that this function is only called if a
	 * bit was set in client_status, so bit_pos won't be negative.
	 */
	bit_pos = __builtin_ffs(client_status) - 1;

	new_chg.current = bc12_chg_limits[bit_pos].current_limit;
	new_chg.type = bc12_chg_limits[bit_pos].type;

	LOG_DBG("sts = 0x%x, lim = %d mA, type = %d", client_status,
		new_chg.current, new_chg.type);
	/* bc1.2 is complete and start bit does not auto clear */
	pi3usb9201_bc12_detect_ctrl(dev, false);
	/* Inform charge manager of new supplier type and current limit */
	bc12_update_supplier(dev, &new_chg);
}

static int bc12_detect_start(const struct device *dev)
{
	int rv;

	/*
	 * Read both status registers to ensure that all interrupt indications
	 * are cleared prior to starting bc1.2 detection.
	 */
	pi3usb9201_get_status(dev, NULL, NULL);

	/* Put pi3usb9201 into client mode */
	rv = pi3usb9201_set_mode(dev, PI3USB9201_CLIENT_MODE);
	if (rv)
		return rv;
	/* Have pi3usb9201 start bc1.2 detection */
	rv = pi3usb9201_bc12_detect_ctrl(dev, true);
	if (rv)
		return rv;
	/* Enable interrupt to wake task when detection completes */
	return pi3usb9201_interrupt_enable(dev, true);
}

static void bc12_power_down(const struct device *dev)
{
	/* Put pi3usb9201 into its power down mode */
	pi3usb9201_set_mode(dev, PI3USB9201_POWER_DOWN);
	/* The start bc1.2 bit does not auto clear */
	pi3usb9201_bc12_detect_ctrl(dev, false);
	/* Mask interrupts unitl next bc1.2 detection event */
	pi3usb9201_interrupt_enable(dev, false);
	/*
	 * Let charge manager know there's no more charge available for the
	 * supplier type that was most recently detected.
	 */
	bc12_update_supplier(dev, NULL);

#if 0
/* TODO - move this into the common bc1.2 handling */
	/* There's nothing else to do if the part is always powered. */
	if (pi3usb9201_bc12_chips[port].flags & PI3USB9201_ALWAYS_POWERED)
		return;

#if defined(CONFIG_POWER_PP5000_CONTROL) && defined(CONFIG_AP_POWER_CONTROL)
	/* Indicate PP5000_A rail is not required by USB_CHG task. */
	power_5v_enable(task_get_current(), 0);
#endif
#endif
}

static void bc12_power_up(const struct device *dev)
{
#if 0
/* TODO - move to common bc1.2 handler */
	if (IS_ENABLED(CONFIG_POWER_PP5000_CONTROL) &&
	    IS_ENABLED(CONFIG_AP_POWER_CONTROL) &&
	    !(pi3usb9201_bc12_chips[port].flags & PI3USB9201_ALWAYS_POWERED)) {
		/* Turn on the 5V rail to allow the chip to be powered. */
		power_5v_enable(task_get_current(), 1);
		/*
		 * Give the pi3usb9201 time so it's ready to receive i2c
		 * messages
		 */
		msleep(1);
	}
#endif

	pi3usb9201_interrupt_enable(dev, false);
}

static void pi3usb9201_isr_work(struct k_work *item)
{
	struct pi3usb9201_data *pi3usb9201_data =
		CONTAINER_OF(item, struct pi3usb9201_data, work);
	struct device *dev = CONTAINER_OF(pi3usb9201_data, struct device, data);
	uint8_t client;
	uint8_t host;
	int rv;

	rv = pi3usb9201_get_status(dev, &client, &host);
	if (!rv && client) {
		/*
		 * Any bit set in client status register indicates that
		 * BC1.2 detection has completed.
		 */
		bc12_update_charge_manager(dev, client);
	}

	if (!rv && host) {
#ifdef CONFIG_BC12_CLIENT_MODE_ONLY_PI3USB9201
		pi3usb9201_set_mode(dev, PI3USB9201_USB_PATH_ON);
#else
		/*
		 * Switch to SDP after device is plugged in to avoid
		 * noise (pulse on D-) causing USB disconnect
		 * (b/156014140).
		 */
		if (host & PI3USB9201_REG_HOST_STS_DEV_PLUG)
			pi3usb9201_set_mode(dev, PI3USB9201_SDP_HOST_MODE);
		/*
		 * Switch to CDP after device is unplugged so we
		 * advertise higher power available for next device.
		 */
		if (host & PI3USB9201_REG_HOST_STS_DEV_UNPLUG)
			pi3usb9201_set_mode(dev, PI3USB9201_CDP_HOST_MODE);
#endif
	}
	/*
	 * TODO(b/124061702): Use host status to allocate power more
	 * intelligently.
	 */
}

static void pi3usb9201_gpio_callback(const struct device *dev,
				     struct gpio_callback *cb, uint32_t pins)
{
	struct pi3usb9201_data *pi3usb9201_data =
		CONTAINER_OF(cb, struct pi3usb9201_data, gpio_cb);

	/*
	 * TODO - post an even to the bc1.2 task instead of the system
	 * workqueue.
	 */
	k_work_submit(&pi3usb9201_data->work);
}

static int pi3usb9201_set_role(const struct device *dev, enum bc12_role role)
{
#if 0
	if (!IS_ENABLED(CONFIG_USB_PD_VBUS_DETECT_TCPC) &&
	    (evt & USB_CHG_EVENT_VBUS))
		CPRINTS("VBUS p%d %d", port, pd_snk_is_vbus_provided(port));
#endif

	switch (role) {
	case BC12_DISCONNECTED:
		bc12_power_down(dev);
		break;
	case BC12_UFP:
		bc12_power_up(dev);
		if (bc12_detect_start(dev)) {
			struct bc12_state new_result;

			/*
			 * VBUS is present, but starting bc1.2 detection failed
			 * for some reason. So limit charge current to default
			 * 500 mA for this case.
			 */

			new_result.voltage = BC12_CHARGER_VOLTAGE_MV;
			new_result.current = BC12_CHARGER_MIN_CURR_MA;
			new_result.type = BC12_TYPE_PROPRIETARY;
			/* Save supplier type and notify chg manager */
			bc12_update_supplier(dev, &new_result);
			LOG_ERR("bc1.2 failed use defaults");

			return -EIO;
		}
		break;

	case BC12_DFP: {
#ifdef CONFIG_BC12_CLIENT_MODE_ONLY_PI3USB9201
		pi3usb9201_set_mode(port, PI3USB9201_USB_PATH_ON);
#else
		enum pi3usb9201_mode mode;
		int rv;

		/*
		 * Update the charge manager if bc1.2 client mode is currently
		 * active.
		 */
		bc12_update_supplier(dev, NULL);
		/*
		 * If the port is in DFP mode, then need to set mode to
		 * CDP_HOST which will auto close D+/D- switches.
		 */
		bc12_power_up(dev);
		rv = pi3usb9201_get_mode(dev, &mode);
		if (!rv && (mode != PI3USB9201_CDP_HOST_MODE)) {
			LOG_DBG("CDP_HOST mode");
			/*
			 * Read both status registers to ensure that all
			 * interrupt indications are cleared prior to starting
			 * DFP CDP host mode.
			 */
			pi3usb9201_get_status(dev, NULL, NULL);
			pi3usb9201_set_mode(dev, PI3USB9201_CDP_HOST_MODE);
			/*
			 * Enable interrupt to wake up when host status
			 * changes.
			 */
			pi3usb9201_interrupt_enable(dev, true);
		}
#endif
		break;
	}
	default:
		LOG_ERR("unsupported BC12 role: %d", role);
		return -EINVAL;
	}

	return 0;
}

int pi3usb9201_result_cb(const struct device *dev, bc12_callback_t cb,
			 void *user_data)
{
	struct pi3usb9201_data *pi3usb9201_data = dev->data;

	pi3usb9201_data->result_callback = cb;
	pi3usb9201_data->callback_data = user_data;

	return 0;
}

#if 0 && defined(CONFIG_CHARGE_RAMP_SW) || defined(CONFIG_CHARGE_RAMP_HW)
static int pi3usb9201_ramp_allowed(int supplier)
{
	/* Don't allow ramp if charge supplier is OTHER, SDP, or NONE */
	return !(supplier == CHARGE_SUPPLIER_OTHER ||
		 supplier == CHARGE_SUPPLIER_BC12_SDP ||
		 supplier == CHARGE_SUPPLIER_BC12_DCP ||
		 supplier == CHARGE_SUPPLIER_NONE);
}

static int pi3usb9201_ramp_max(int supplier, int sup_curr)
{
	/*
	 * Use the level from the bc12_chg_limits table above except for
	 * proprietary or CDP and in those cases the charge current from the
	 * charge manager is already set at the max determined by bc1.2
	 * detection.
	 */
	switch (supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
		return USB_CHARGER_MAX_CURR_MA;
	case CHARGE_SUPPLIER_BC12_CDP:
	case CHARGE_SUPPLIER_PROPRIETARY:
		return sup_curr;
	case CHARGE_SUPPLIER_BC12_SDP:
	default:
		return 500;
	}
}
#endif /* CONFIG_CHARGE_RAMP_SW || CONFIG_CHARGE_RAMP_HW */

static const struct bc12_driver_api pi3usb9201_driver_api = {
	.set_role = pi3usb9201_set_role,
	.result_cb = pi3usb9201_result_cb,
};

#if 0
const struct bc12_drv pi3usb9201_driver_api = {
	.usb_charger_task_init = pi3usb9201_usb_charger_task_init,
	.usb_charger_task_event = pi3usb9201_usb_charger_task_event,
#if defined(CONFIG_CHARGE_RAMP_SW) || defined(CONFIG_CHARGE_RAMP_HW)
	.ramp_allowed = pi3usb9201_ramp_allowed,
	.ramp_max = pi3usb9201_ramp_max,
#endif /* CONFIG_CHARGE_RAMP_SW || CONFIG_CHARGE_RAMP_HW */
};
#endif

int pi3usb9201_init(const struct device *dev)
{
	const struct pi3usb9201_config *cfg = dev->config;
	struct pi3usb9201_data *pi3usb9201_data = dev->data;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	/*
	 * Set most recent bc1.2 detection type result to
	 * BC12_TYPE_NONE for the port.
	 */
	pi3usb9201_data->type = BC12_TYPE_NONE;

	if (cfg->irq_gpio.port) {
		gpio_pin_configure_dt(&cfg->irq_gpio, GPIO_INT_LEVEL_INACTIVE);
		gpio_init_callback(&pi3usb9201_data->gpio_cb,
				   pi3usb9201_gpio_callback,
				   BIT(cfg->irq_gpio.pin));
		gpio_pin_interrupt_configure_dt(&cfg->irq_gpio,
						GPIO_INT_EDGE_TO_ACTIVE);

		k_work_init(&pi3usb9201_data->work, pi3usb9201_isr_work);
	} else {
		/* Polling mode not supported */
		k_oops();
	}

	/*
	 * The is no specific initialization required for the pi3usb9201 other
	 * than disabling the interrupt.
	 */
	pi3usb9201_interrupt_enable(dev, false);

	return 0;
}

#define PI2USB9201_FLAGS(inst)                                \
	0 | COND_CODE_1(DT_INST_PROP(inst, client_mode_only), \
			(PI3USB9201_CLIENT_ONLY), (0))

#define PI2USB9201_DEFINE(inst)                                               \
	static struct pi3usb9201_data pi3usb9201_data_##inst;                 \
                                                                              \
	static const struct pi3usb9201_config pi3usb9201_config_##inst = {    \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                            \
		.irq_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, irq_gpios, { 0 }), \
		.enable_gpio =                                                \
			GPIO_DT_SPEC_INST_GET_OR(inst, enable_gpios, { 0 }),  \
		.flags = PI2USB9201_FLAGS(inst),                              \
	};                                                                    \
                                                                              \
	DEVICE_DT_INST_DEFINE(inst, pi3usb9201_init, NULL,                    \
			      &pi3usb9201_data_##inst,                        \
			      &pi3usb9201_config_##inst, POST_KERNEL,         \
			      KERNEL_INIT_PRIORITY_DEVICE,                    \
			      &pi3usb9201_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PI2USB9201_DEFINE)
