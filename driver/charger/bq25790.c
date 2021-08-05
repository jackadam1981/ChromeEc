#include "driver/charger/bq25790.h"
#include "endian.h"
#include "i2c.h"

static int bq25790_update8(int chgnum, int offset, uint8_t mask,
			   enum mask_update_action action)
{
	return i2c_update8(chg_chips[chgnum].i2c_port,
			   chg_chips[chgnum].i2c_addr_flags,
			   offset, mask, action);
}

static int bq25790_read8(int chgnum, int offset, int *val)
{
	return i2c_read8(chg_chips[chgnum].i2c_port,
			 chg_chips[chgnum].i2c_addr_flags,
			 offset, val);
}

static int bq25790_read16(int chgnum, int offset, int *val)
{
	RETURN_ERROR(i2c_read16(chg_chips[chgnum].i2c_port,
				chg_chips[chgnum].i2c_addr_flags,
				offset, val));
	*val = be16toh(*val);

	return EC_SUCCESS;
}

static int bq25790_write8(int chgnum, int offset, int val)
{
	return i2c_write8(chg_chips[chgnum].i2c_port,
			  chg_chips[chgnum].i2c_addr_flags,
			  offset, val);
}

static int bq25790_write16(int chgnum, int offset, int val)
{
	val = htobe16(val);
	return i2c_write16(chg_chips[chgnum].i2c_port,
			   chg_chips[chgnum].i2c_addr_flags,
			   offset, val);
}

static int bq25790_closest_reg(int min, int max, int step, int target)
{
	if (target < min)
		return 0;
	if (target >= max)
		return ((max - min) / step);
	return (target - min) / step;
}

static void bq25790_init(int chgnum)
{
	bq25790_update8(chgnum, BQ25790_TERM_CTRL, BQ25790_TERM_CTRL_REG_RST, 
			MASK_SET);
}

static const struct charger_info *bq25790_get_info(int chgnum)
{
	static const struct charger_info info = {
		.name = "bq25790",
		.voltage_max  = 18800,
		.voltage_min  = 3000,
		.voltage_step = 10,
		.current_max  = 5000,
		.current_min  = 0,
		.current_step = 10,
		.input_current_max  = 3300,
		.input_current_min  = 100,
		.input_current_step = 10,
	};

	return &info;
}

enum ec_error_list bq25790_enable_otg_power(int chgnum, int enabled)
{
	return bq25790_update8(chgnum, BQ25790_CHRG_CTRL_3,
			       BQ25790_CHRG_CTRL_3_EN_OTG,
			       enabled ? MASK_SET : MASK_CLR);
}

enum ec_error_list bq25790_set_otg_current_voltage(int chgnum,
						   int output_current,
						   int output_voltage)
{
	int iotg = bq25790_closest_reg(0, 3320, 40, output_current);
	int votg = bq25790_closest_reg(2800, 22000, 10, output_voltage);
	int reg;

	RETURN_ERROR(bq25790_read8(chgnum, BQ25790_IOTG_REG, &reg));
	reg = (reg & ~BQ25790_IOTG_REG_MASK) | iotg;
	RETURN_ERROR(bq25790_write8(chgnum, BQ25790_IOTG_REG, reg));

	RETURN_ERROR(bq25790_read16(chgnum, BQ25790_VOTG_REG, &reg));
	reg = (reg & ~BQ25790_VOTG_REG_MASK) | votg;
	RETURN_ERROR(bq25790_write16(chgnum, BQ25790_VOTG_REG, reg));

	return EC_SUCCESS;
}

int bq25790_is_sourcing_otg_power(int chgnum, int enabled)
{
	int reg;
	int ret = bq25790_read8(chgnum, BQ25790_CHRG_CTRL_3, &reg);

	if (ret)
		return 0;
	return !!(reg & BQ25790_CHRG_CTRL_3_EN_OTG);
}

enum ec_error_list bq25790_get_current(int chgnum, int *current)
{
	int reg;

	RETURN_ERROR(bq25790_read16(chgnum, BQ25790_CHRG_I_LIM, &reg));
	*current = (reg & 0x1FF) * 10;
	return EC_SUCCESS;
}

enum ec_error_list bq25790_set_current(int chgnum, int current)
{
	int reg = bq25790_closest_reg(0, 5000, 10, current);

	return bq25790_write16(chgnum, BQ25790_CHRG_I_LIM, reg);
}

enum ec_error_list bq25790_get_voltage(int chgnum, int *voltage)
{
	int reg;

	RETURN_ERROR(bq25790_read16(chgnum, BQ25790_CHRG_V_LIM, &reg));
	*voltage = (reg & 0x3FF) * 10;
	return EC_SUCCESS;
}

enum ec_error_list bq25790_set_voltage(int chgnum, int voltage)
{
	int reg = bq25790_closest_reg(0, 18800, 10, voltage);

	return bq25790_write16(chgnum, BQ25790_CHRG_V_LIM, reg);
}

enum ec_error_list bq25790_set_input_current_limit(int chgnum,
						  int input_current)
{
	int reg = bq25790_closest_reg(0, 3300, 10, input_current);

	return bq25790_write16(chgnum, BQ25790_INPUT_I_LIM, reg);
}

enum ec_error_list bq25790_get_input_current_limit(int chgnum,
						  int *input_current)
{
	int reg;

	RETURN_ERROR(bq25790_read16(chgnum, BQ25790_INPUT_I_LIM, &reg));
	*input_current = (reg & 0x1FF) * 10;
	return EC_SUCCESS;
}

const struct charger_drv bq25790_drv = {
	.init = &bq25790_init,
	.get_info = &bq25790_get_info,
	.enable_otg_power = &bq25790_enable_otg_power,
	.set_otg_current_voltage = &bq25790_set_otg_current_voltage,
	.is_sourcing_otg_power = &bq25790_is_sourcing_otg_power,
	.get_current = &bq25790_get_current,
	.set_current = &bq25790_set_current,
	.get_voltage = &bq25790_get_voltage,
	.set_voltage = &bq25790_set_voltage,
	.get_input_current_limit = &bq25790_get_input_current_limit,
	.set_input_current_limit = &bq25790_set_input_current_limit,
};
