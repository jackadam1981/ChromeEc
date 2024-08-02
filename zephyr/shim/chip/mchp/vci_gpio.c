/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio/gpio.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cros_vci_gpio, LOG_LEVEL_DBG);

#define STRUCT_VCI_REG_BASE_ADDR \
	((struct vci_regs *)(DT_REG_ADDR(DT_NODELABEL(vci0))))

#define VCI_GPIO_IRQ_NUM DT_NUM_IRQS(DT_NODELABEL(vci0))

struct vci_gpio_data {
	gpio_flags_t flags[VCI_GPIO_IRQ_NUM];
	sys_slist_t callbacks;
#ifdef CONFIG_VCI_GPIO_IN0_RISING_TIMER
	struct k_work_delayable vci_in0_work;
#endif
};

struct vci_interrupts {
	int irq;
	int girq;
	int girq_pos;
};

struct vci_gpio_config {
	struct vci_regs *regs;
	int irq_num;
	struct vci_interrupts irqs[VCI_GPIO_IRQ_NUM];
};

const struct vci_gpio_config config0 = {
	.regs = STRUCT_VCI_REG_BASE_ADDR,
	.irq_num = VCI_GPIO_IRQ_NUM,
	.irqs = {
		/* VCI_IN0 */
		{.irq = 0x7a, .girq = 0x15, .girq_pos = 0xb},
		/* VCI_IN1 */
		{.irq = 0x7b, .girq = 0x15, .girq_pos = 0xc},
		/* VCI_IN2 */
		{.irq = 0x7c, .girq = 0x15, .girq_pos = 0xd},
		/* VCI_IN3 */
		{.irq = 0x7d, .girq = 0x15, .girq_pos = 0xe},
		/* VCI_OVRD_IN */
		{.irq = 0x79, .girq = 0x15, .girq_pos = 0xa},
	},
};

#ifdef CONFIG_VCI_GPIO_IN0_RISING_TIMER
#define VCI_IN0_DELAY K_MSEC(300)

static void vci_in0_timer_handler(struct k_work *work)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(vci_gpios));
	const struct vci_gpio_config *cfg = dev->config;
	struct vci_gpio_data *data = dev->data;
	struct vci_regs *regs = cfg->regs;
	bool cur = !!(regs->CONFIG & BIT(0));

	if ((data->flags[0] & GPIO_INT_EDGE_RISING) == GPIO_INT_EDGE_RISING) {
		if (cur) {
			gpio_fire_callbacks(&data->callbacks, dev, 1);
		} else {
			k_work_schedule(&data->vci_in0_work, VCI_IN0_DELAY);
		}
	}
}
#endif

static void vci_gpio_isr(const struct device *dev)
{
	const struct vci_gpio_config *cfg = dev->config;
	struct vci_gpio_data *data = dev->data;
	struct vci_regs *regs = cfg->regs;
	uint32_t edge_det;

	edge_det = regs->PEDGE_DET & regs->INPUT_EN & MCHP_VCI_NDET_REG_MASK;
	regs->PEDGE_DET = edge_det;
	while (edge_det) {
		int i = __builtin_ffs(edge_det) - 1;

		edge_det &= ~BIT(i);
#ifdef CONFIG_VCI_GPIO_IN0_RISING_TIMER
		if (i == 0) {
			continue;
		}
#endif
		if ((data->flags[i] & GPIO_INT_EDGE_FALLING) ==
		    GPIO_INT_EDGE_FALLING) {
			regs->INPUT_EN &= ~BIT(i);
			regs->POLARITY &= ~BIT(i);
			regs->INPUT_EN |= BIT(i);
		}
		if ((data->flags[i] & GPIO_INT_EDGE_RISING) ==
		    GPIO_INT_EDGE_RISING) {
			gpio_fire_callbacks(&data->callbacks, dev, BIT(i));
		}
	}

	edge_det = regs->NEDGE_DET & regs->INPUT_EN & MCHP_VCI_NDET_REG_MASK;
	regs->NEDGE_DET = edge_det;
	while (edge_det) {
		int i = __builtin_ffs(edge_det) - 1;

		edge_det &= ~BIT(i);
		if ((data->flags[i] & GPIO_INT_EDGE_RISING) ==
		    GPIO_INT_EDGE_RISING) {
			regs->INPUT_EN &= ~BIT(i);
			regs->POLARITY |= BIT(i);
			regs->INPUT_EN |= BIT(i);
		}
		if ((data->flags[i] & GPIO_INT_EDGE_FALLING) ==
		    GPIO_INT_EDGE_FALLING) {
			gpio_fire_callbacks(&data->callbacks, dev, BIT(i));
		}
#ifdef CONFIG_VCI_GPIO_IN0_RISING_TIMER
		if (i == 0 && ((data->flags[i] & GPIO_INT_EDGE_RISING) ==
			       GPIO_INT_EDGE_RISING)) {
			/* VCI_IN0 cannot enable rising edge interruption */
			k_work_schedule(&data->vci_in0_work, K_NO_WAIT);
		}
#endif
	}
}

static int vci_gpio_pin_configure(const struct device *dev, gpio_pin_t pin,
				  gpio_flags_t flags)
{
	const struct vci_gpio_config *cfg = dev->config;
	struct vci_regs *regs = cfg->regs;

	__ASSERT((flags & GPIO_INPUT), "Pin cannot be configured as Output");

	/* Dissabling LATCH for GPIO Functionality */
	regs->LATCH_RST |= BIT(pin);
	regs->LATCH_EN &= ~(gpio_port_pins_t)BIT(pin);
	regs->INPUT_EN |= BIT(pin);

	return 0;
}

static int vci_gpio_port_get_raw(const struct device *dev,
				 gpio_port_value_t *value)
{
	const struct vci_gpio_config *cfg = dev->config;
	struct vci_regs *regs = cfg->regs;

	*value = regs->CONFIG & regs->INPUT_EN;
	return 0;
}

static int vci_gpio_port_set_masked_raw(const struct device *dev,
					gpio_port_pins_t mask,
					gpio_port_value_t value)
{
	return -EOPNOTSUPP;
}

static int vci_gpio_port_set_bits_raw(const struct device *dev,
				      gpio_port_pins_t pins)
{
	return -EOPNOTSUPP;
}

static int vci_gpio_port_clear_bits_raw(const struct device *dev,
					gpio_port_pins_t pins)
{
	return -EOPNOTSUPP;
}

static int vci_gpio_port_toggle_bits(const struct device *dev,
				     gpio_port_pins_t pins)
{
	return -EOPNOTSUPP;
}

static int vci_gpio_pin_interrupt_configure(const struct device *dev,
					    gpio_pin_t pin,
					    enum gpio_int_mode mode,
					    enum gpio_int_trig trig)
{
	const struct vci_gpio_config *cfg = dev->config;
	struct vci_gpio_data *data = dev->data;
	struct vci_regs *regs = cfg->regs;

	if (mode == GPIO_INT_MODE_DISABLED) {
		data->flags[pin] &= ~GPIO_INT_ENABLE;
		irq_disable(cfg->irqs[pin].irq);
		return 0;
	}
	if (mode == GPIO_INT_MODE_LEVEL) {
		return -EOPNOTSUPP;
	}

	data->flags[pin] |= mode;
	data->flags[pin] |= trig;

	regs->INPUT_EN &= ~BIT(pin);
	if ((data->flags[pin] & GPIO_INT_EDGE_BOTH) == GPIO_INT_EDGE_BOTH) {
		if (regs->CONFIG & BIT(pin)) {
			regs->POLARITY &= ~(gpio_port_pins_t)BIT(pin);
		} else {
			regs->POLARITY |= (gpio_port_pins_t)BIT(pin);
		}
	} else if ((data->flags[pin] & GPIO_INT_EDGE_FALLING) ==
		   GPIO_INT_EDGE_FALLING) {
		regs->POLARITY &= ~(gpio_port_pins_t)BIT(pin);
	} else if ((data->flags[pin] & GPIO_INT_EDGE_RISING) ==
		   GPIO_INT_EDGE_RISING) {
		regs->POLARITY |= (gpio_port_pins_t)BIT(pin);
	}

	mchp_soc_ecia_girq_src_en(cfg->irqs[pin].girq, cfg->irqs[pin].girq_pos);
	switch (pin) {
	case 0:
		IRQ_CONNECT(DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 1, irq),
			    DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 1, priority),
			    vci_gpio_isr,
			    DEVICE_DT_GET(DT_NODELABEL(vci_gpios)), 0);
		break;
	case 1:
		IRQ_CONNECT(DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 2, irq),
			    DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 2, priority),
			    vci_gpio_isr,
			    DEVICE_DT_GET(DT_NODELABEL(vci_gpios)), 0);
		break;
	case 3:
		IRQ_CONNECT(DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 3, irq),
			    DT_IRQ_BY_IDX(DT_NODELABEL(vci0), 3, priority),
			    vci_gpio_isr,
			    DEVICE_DT_GET(DT_NODELABEL(vci_gpios)), 0);
		break;
	default:
		return -EIO;
	}
	irq_enable(cfg->irqs[pin].irq);

	return 0;
}

static int vci_gpio_init(const struct device *dev);

static int vci_gpio_manage_callback(const struct device *dev,
				    struct gpio_callback *callback, bool enable)
{
	struct vci_gpio_data *data = dev->data;

	return gpio_manage_callback(&data->callbacks, callback, enable);
}

static const struct gpio_driver_api api_table = {
	.pin_configure = vci_gpio_pin_configure,
	.port_get_raw = vci_gpio_port_get_raw,
	.port_set_masked_raw = vci_gpio_port_set_masked_raw,
	.port_set_bits_raw = vci_gpio_port_set_bits_raw,
	.port_clear_bits_raw = vci_gpio_port_clear_bits_raw,
	.port_toggle_bits = vci_gpio_port_toggle_bits,
	.pin_interrupt_configure = vci_gpio_pin_interrupt_configure,
	.manage_callback = vci_gpio_manage_callback,
};

struct vci_gpio_data data0;

DEVICE_DT_DEFINE(DT_NODELABEL(vci_gpios), vci_gpio_init, NULL, &data0, &config0,
		 POST_KERNEL, CONFIG_INTC_INIT_PRIORITY, &api_table);

static int vci_gpio_init(const struct device *dev)
{
#ifdef CONFIG_VCI_GPIO_IN0_RISING_TIMER
	struct vci_gpio_data *data = dev->data;

	k_work_init_delayable(&data->vci_in0_work, vci_in0_timer_handler);
#endif
	return 0;
}
