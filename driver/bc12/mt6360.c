#include "charger.h"
#include "charge_manager.h"
#include "console.h"
#include "crc8.h"
#include "driver/bc12/mt6360.h"
#include "ec_commands.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "util.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) \
	cprints(CC_CHARGER, "%s " format, "MT6360", ## args)

static enum ec_error_list mt6360_read8(int reg, int *val)
{
	return i2c_read8(mt6360_config.i2c_port, mt6360_config.i2c_addr_flags,
			reg, val);
}

static enum ec_error_list mt6360_write8(int reg, int val)
{
	return i2c_write8(mt6360_config.i2c_port, mt6360_config.i2c_addr_flags,
			reg, val);
}

static int mt6360_update_bits(int reg, int mask, int val)
{
	int rv;
	int reg_val = 0;

	rv = mt6360_read8(reg, &reg_val);
	if (rv)
		return rv;
	reg_val &= ~mask;
	reg_val |= (mask & val);
	rv = mt6360_write8(reg, reg_val);
	return rv;
}

static inline int mt6360_set_bit(int reg, int mask)
{
	return mt6360_update_bits(reg, mask, mask);
}

static inline int mt6360_clr_bit(int reg, int mask)
{
	return mt6360_update_bits(reg, mask, 0x00);
}


static int mt6360_get_bc12_device_type(void)
{
	int reg;

	if (mt6360_read8(MT6360_REG_USB_STATUS_1, &reg))
		return CHARGE_SUPPLIER_NONE;

	switch (reg & MT6360_MASK_USB_STATUS) {
	case MT6360_MASK_SDP:
		CPRINTS("BC12 SDP");
		return CHARGE_SUPPLIER_BC12_SDP;
	case MT6360_MASK_CDP:
		CPRINTS("BC12 CDP");
		return CHARGE_SUPPLIER_BC12_CDP;
	case MT6360_MASK_DCP:
		CPRINTS("BC12 DCP");
		return CHARGE_SUPPLIER_BC12_DCP;
	default:
		CPRINTS("BC12 NONE");
		return CHARGE_SUPPLIER_NONE;
	}
}

static int mt6360_get_bc12_ilim(int charge_supplier)
{
	switch (charge_supplier) {
	case CHARGE_SUPPLIER_BC12_DCP:
	case CHARGE_SUPPLIER_BC12_CDP:
		return USB_CHARGER_MAX_CURR_MA;
	case CHARGE_SUPPLIER_BC12_SDP:
	default:
		return USB_CHARGER_MIN_CURR_MA;
	}
}

static int mt6360_enable_bc12_detection(int en)
{
	int rv;

	if (en) {
#ifdef CONFIG_MT6360_BC12_DETECT_GPIO
		gpio_set_level(CONFIG_MT6360_BC12_DETECT_GPIO, 1);
#endif
		return mt6360_set_bit(MT6360_REG_DEVICE_TYPE,
				      MT6360_MASK_USBCHGEN);
	}

	rv = mt6360_clr_bit(MT6360_REG_DEVICE_TYPE, MT6360_MASK_USBCHGEN);
#ifdef CONFIG_MT6360_BC12_DETECT_GPIO
	gpio_set_level(CONFIG_MT6360_BC12_DETECT_GPIO, 0);
#endif
	return rv;
}

static int mt6360_ramp_allowed(int supplier)
{
	return supplier == CHARGE_SUPPLIER_BC12_DCP;
}

static int mt6360_ramp_max(int supplier, int sup_curr)
{
	return mt6360_get_bc12_ilim(supplier);
}

static void mt6360_usb_charger_task(const int port)
{
	int current_bc12_type = CHARGE_SUPPLIER_NONE;

	while (1) {
		int new_bc12_type = CHARGE_SUPPLIER_NONE;

		task_wait_event(-1);

		/* Run bc12 detection only if port is connected and is sink */
		if (pd_snk_is_vbus_provided(port)) {
			mt6360_enable_bc12_detection(1);
			/* TODO: change this to interrupt */
			usleep(SECOND);
			new_bc12_type = mt6360_get_bc12_device_type();
			mt6360_enable_bc12_detection(0);
		}

		if (current_bc12_type == new_bc12_type)
			/* skip update if type doesn't change */
			continue;

		if (new_bc12_type == CHARGE_SUPPLIER_NONE) {
			CPRINTS("VBUS detached");
			charge_manager_update_charge(
					current_bc12_type, 0, NULL);
		} else {
			struct charge_port_info chg = {
				.current = mt6360_get_bc12_ilim(new_bc12_type),
				.voltage = USB_CHARGER_VOLTAGE_MV,
			};

			charge_manager_update_charge(new_bc12_type, 0, &chg);
		}

		current_bc12_type = new_bc12_type;
	}
}

/* LDO */
static int mt6360_ldo_write8(int reg, int val)
{
	/*
	 * TODO(pihsun): The checksum from I2C_FLAG_PEC happens to be correct
	 * because the length == 1 -> the high 3 bits of the offset byte is 0.
	 * Consider moving the checksum to here for a general function that can
	 * handle 1~4 bytes of data?
	 */
	return i2c_write8(mt6360_config.i2c_port,
			  MT6360_LDO_SLAVE_ADDR_FLAGS | I2C_FLAG_PEC, reg, val);
}

static int mt6360_ldo_read8(int reg, int *val)
{
	int rv;
	uint8_t crc = 0, real_crc;
	uint8_t addr = MT6360_LDO_SLAVE_ADDR_FLAGS;
	uint8_t out[3] = {(addr << 1) | 1, reg};

	rv = i2c_read16(mt6360_config.i2c_port, addr, reg, val);
	if (rv)
		return rv;

	real_crc = (*val >> 8) & 0xFF;
	*val &= 0xFF;
	out[2] = *val;
	crc = crc8(out, ARRAY_SIZE(out));

	if (crc != real_crc)
		return EC_ERROR_CRC;

	return EC_SUCCESS;
}

static int mt6360_ldo_update_bits(int reg, int mask, int val)
{
	int rv;
	int reg_val = 0;

	rv = mt6360_ldo_read8(reg, &reg_val);
	if (rv)
		return rv;
	reg_val &= ~mask;
	reg_val |= (mask & val);
	rv = mt6360_ldo_write8(reg, reg_val);
	return rv;
}

struct mt6360_ldo_data {
	char *name;
	const uint16_t *vosel_table;
	uint16_t vosel_table_len;
	uint8_t reg_en_ctrl2;
	uint8_t reg_ctrl3;
	uint8_t mask_vosel;
	uint8_t shift_vosel;
	uint8_t mask_vocal;
};

const uint16_t MT6360_LDO3_VOSEL_TABLE[16] = {
	[0x4] = 1800,
	[0xA] = 2900,
	[0xB] = 3000,
	[0xD] = 3300,
};

const uint16_t MT6360_LDO5_VOSEL_TABLE[8] = {
	[0x2] = 2900,
	[0x3] = 3000,
	[0x5] = 3300,
};

static struct mt6360_ldo_data ldo_data[MT6360_LDO_COUNT] = {
	[MT6360_LDO3] = {
		.name = "mt6360_ldo3",
		.vosel_table = MT6360_LDO3_VOSEL_TABLE,
		.vosel_table_len = ARRAY_SIZE(MT6360_LDO3_VOSEL_TABLE),
		.reg_en_ctrl2 = 0x05,
		.reg_ctrl3 = 0x09,
		.mask_vosel = 0xF0,
		.shift_vosel = 4,
		.mask_vocal = 0x0F,
	},
	[MT6360_LDO5] = {
		.name = "mt6360_ldo5",
		.vosel_table = MT6360_LDO5_VOSEL_TABLE,
		.vosel_table_len = ARRAY_SIZE(MT6360_LDO5_VOSEL_TABLE),
		.reg_en_ctrl2 = 0x0B,
		.reg_ctrl3 = 0x0F,
		.mask_vosel = 0x70,
		.shift_vosel = 4,
		.mask_vocal = 0x0F,
	},
};

/* This is same for LDO{3,5,2,1}_EN_CTRL2 */
#define MT6360_MASK_LDO_SW_OP_EN BIT(7)
#define MT6360_MASK_LDO_SW_EN BIT(6)

#define MT6360_LDO_VOCAL_STEP_MV 10
#define MT6360_LDO_VOCAL_MAX_STEP 10

int mt6360_ldo_get_info(enum mt6360_ldo_id ldo_id, char *name,
			uint16_t *num_voltages, uint16_t *voltages_mv)
{
	int i;
	int cnt = 0;
	struct mt6360_ldo_data *data;

	if (ldo_id >= MT6360_LDO_COUNT)
		return EC_ERROR_INVAL;
	data = &ldo_data[ldo_id];

	strzcpy(name, data->name, EC_REGULATOR_NAME_MAX_LEN);
	for (i = 0; i < data->vosel_table_len; i++) {
		int mv = data->vosel_table[i];
		if (!mv)
			continue;
		if (cnt < EC_REGULATOR_VOLTAGE_MAX_COUNT) {
			voltages_mv[cnt++] = mv;
		} else {
			CPRINTS("LDO3 Voltage info overflow: %d", mv);
		}
	}
	*num_voltages = cnt;
	return EC_SUCCESS;
}

int mt6360_ldo_enable(enum mt6360_ldo_id ldo_id, uint8_t enable)
{
	struct mt6360_ldo_data *data;

	if (ldo_id >= MT6360_LDO_COUNT)
		return EC_ERROR_INVAL;
	data = &ldo_data[ldo_id];

	if (enable)
		return mt6360_ldo_update_bits(
			data->reg_en_ctrl2,
			MT6360_MASK_LDO_SW_OP_EN | MT6360_MASK_LDO_SW_EN,
			MT6360_MASK_LDO_SW_OP_EN | MT6360_MASK_LDO_SW_EN);
	else
		return mt6360_ldo_update_bits(
			data->reg_en_ctrl2,
			MT6360_MASK_LDO_SW_OP_EN | MT6360_MASK_LDO_SW_EN,
			MT6360_MASK_LDO_SW_OP_EN);
}

int mt6360_ldo_is_enabled(enum mt6360_ldo_id ldo_id, uint8_t *enabled)
{
	int rv;
	int value;
	struct mt6360_ldo_data *data;

	if (ldo_id >= MT6360_LDO_COUNT)
		return EC_ERROR_INVAL;
	data = &ldo_data[ldo_id];

	rv = mt6360_ldo_read8(data->reg_en_ctrl2, &value);
	if (rv) {
		CPRINTS("Error reading LDO3 enabled: %d", rv);
		return rv;
	}
	*enabled = !!(value & MT6360_MASK_LDO_SW_EN);
	return EC_SUCCESS;
}

int mt6360_ldo_set_voltage(enum mt6360_ldo_id ldo_id, int min_mv, int max_mv)
{
	int i;
	struct mt6360_ldo_data *data;

	if (ldo_id >= MT6360_LDO_COUNT)
		return EC_ERROR_INVAL;
	data = &ldo_data[ldo_id];

	for (i = 0; i < data->vosel_table_len; i++) {
		int mv = data->vosel_table[i];
		int step;
		if (!mv)
			continue;
		/* TODO(pihsun): Constants for the step */
		if (mv + MT6360_LDO_VOCAL_STEP_MV * MT6360_LDO_VOCAL_MAX_STEP <
		    min_mv)
			continue;
		mv = DIV_ROUND_UP(mv, MT6360_LDO_VOCAL_STEP_MV) *
		     MT6360_LDO_VOCAL_STEP_MV;
		if (mv > max_mv)
			continue;
		step = (mv - data->vosel_table[i]) / MT6360_LDO_VOCAL_STEP_MV;

		return mt6360_ldo_update_bits(
			data->reg_ctrl3,
			data->mask_vosel | data->mask_vocal,
			(i << data->shift_vosel) | step);
	}
	CPRINTS("LDO3 voltage %d - %d out of range", min_mv, max_mv);
	return EC_ERROR_INVAL;
}

int mt6360_ldo_get_voltage(enum mt6360_ldo_id ldo_id, int *voltage_mv)
{
	int value;
	int rv;
	struct mt6360_ldo_data *data;

	if (ldo_id >= MT6360_LDO_COUNT)
		return EC_ERROR_INVAL;
	data = &ldo_data[ldo_id];

	rv = mt6360_ldo_read8(data->reg_ctrl3, &value);
	if (rv) {
		CPRINTS("Error reading LDO3 ctrl3: %d", rv);
		return rv;
	}
	*voltage_mv = data->vosel_table[(value & data->mask_vosel) >>
					data->shift_vosel];
	if (*voltage_mv == 0) {
		CPRINTS("Unknown LDO3 voltage value: %d", value);
		return EC_ERROR_INVAL;
	}
	*voltage_mv +=
		MIN(MT6360_LDO_VOCAL_MAX_STEP, value & data->mask_vocal) *
		MT6360_LDO_VOCAL_STEP_MV;
	return EC_SUCCESS;
}

/* RGB LED */
int mt6360_led_enable(enum mt6360_led_id led_id, int enable)
{
	if (!IN_RANGE(led_id, 0, MT6360_LED_COUNT))
		return EC_ERROR_INVAL;

	if (enable)
		return mt6360_set_bit(MT6360_REG_RGB_EN,
				      MT6360_MASK_ISINK_EN(led_id));
	return mt6360_clr_bit(MT6360_REG_RGB_EN, MT6360_MASK_ISINK_EN(led_id));
}

int mt6360_led_set_brightness(enum mt6360_led_id led_id, int brightness)
{
	int val;

	if (!IN_RANGE(led_id, 0, MT6360_LED_COUNT))
		return EC_ERROR_INVAL;
	if (!IN_RANGE(brightness, 0, 16))
		return EC_ERROR_INVAL;

	RETURN_ERROR(mt6360_read8(MT6360_REG_RGB_ISINK(led_id), &val));
	val &= ~MT6360_MASK_CUR_SEL;
	val |= brightness;

	return mt6360_write8(MT6360_REG_RGB_ISINK(led_id), val);
}

const struct bc12_drv mt6360_drv = {
	.usb_charger_task = mt6360_usb_charger_task,
	.ramp_allowed = mt6360_ramp_allowed,
	.ramp_max = mt6360_ramp_max,
};

#ifdef CONFIG_BC12_SINGLE_DRIVER
/* provide a default bc12_ports[] for backward compatibility */
struct bc12_config bc12_ports[CHARGE_PORT_COUNT] = {
	[0 ... (CHARGE_PORT_COUNT - 1)] = {
		.drv = &mt6360_drv,
	},
};
#endif /* CONFIG_BC12_SINGLE_DRIVER */
