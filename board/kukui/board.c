/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc_chip.h"
#include "adc.h"
#include "backlight.h"
#include "bootblock.h"
#include "button.h"
#include "charge_manager.h"
#include "charger.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "clock.h"
#include "console.h"
#include "dma.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/charger/rt946x.h"
#include "driver/sync.h"
#include "driver/tcpm/fusb302.h"
#include "driver/temp_sensor/tmp432.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "power_button.h"
#include "power.h"
#include "pwm_chip.h"
#include "pwm.h"
#include "registers.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "tcpm.h"
#include "temp_sensor_chip.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static void tcpc_alert_event(enum gpio_signal signal)
{
#ifdef HAS_TASK_PDCMD
	/* Exchange status with TCPCs */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
#endif
}

static void warm_reset_request_interrupt(enum gpio_signal signal)
{
	CPRINTS("AP wants warm reset");
	chipset_reset();
}

static void ap_watchdog_interrupt(enum gpio_signal signal)
{
	CPRINTS("AP watchdog triggered.");
	cflush();
	/* TODO(b:109900671): Handle AP watchdog, when necessary. */
}

#include "gpio_list.h"

/******************************************************************************/
/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	[ADC_BOARD_ID] = {"BOARD_ID", 3300, 4096, 0, STM32_AIN(10)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/******************************************************************************/
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"charger",   I2C_PORT_CHARGER,   400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"tcpc0",     I2C_PORT_TCPC0,     400, GPIO_I2C1_SCL, GPIO_I2C1_SDA},
	{"battery",   I2C_PORT_BATTERY,   400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
	{"accelgyro", I2C_PORT_ACCEL,     400, GPIO_I2C2_SCL, GPIO_I2C2_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_AP_IN_SLEEP_L,   POWER_SIGNAL_ACTIVE_LOW,  "AP_IN_S3_L"},
	{GPIO_PMIC_EC_RESETB,  POWER_SIGNAL_ACTIVE_HIGH, "PMIC_PWR_GOOD"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#ifdef CONFIG_TEMP_SENSOR_TMP432
/* Temperature sensors data; must be in same order as enum temp_sensor_id. */
const struct temp_sensor_t temp_sensors[] = {
	{"TMP432_Internal", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_LOCAL, 4},
	{"TMP432_Sensor_1", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_REMOTE1, 4},
	{"TMP432_Sensor_2", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_REMOTE2, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/*
 * Thermal limits for each temp sensor. All temps are in degrees K. Must be in
 * same order as enum temp_sensor_id. To always ignore any temp, use 0.
 */
struct ec_thermal_config thermal_params[] = {
	{{0, 0, 0}, 0, 0}, /* TMP432_Internal */
	{{0, 0, 0}, 0, 0}, /* TMP432_Sensor_1 */
	{{0, 0, 0}, 0, 0}, /* TMP432_Sensor_2 */
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);
#endif

/******************************************************************************/
/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/******************************************************************************/
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{I2C_PORT_TCPC0, FUSB302_I2C_SLAVE_ADDR, &fusb302_tcpm_drv},
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
};

void board_reset_pd_mcu(void)
{
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;

	if (!gpio_get_level(GPIO_USB_C0_PD_INT_ODL))
		status |= PD_STATUS_TCPC_ALERT_0;

	return status;
}

int board_set_active_charge_port(int charge_port)
{
	CPRINTS("New chg p%d", charge_port);

	switch (charge_port) {
	case 0:
		/* Don't charge from a source port */
		if (board_vbus_source_enabled(charge_port))
			return -1;
		break;
	case CHARGE_PORT_NONE:
		/*
		 * To ensure the fuel gauge (max17055) is always powered
		 * even when battery is disconnected, keep VBAT rail on but
		 * set the charging current to minimum.
		 */
		charger_set_current(0);
		break;
	default:
		panic("Invalid charge port\n");
		break;
	}

	return EC_SUCCESS;
}

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	charge_set_input_current_limit(MAX(charge_ma,
			       CONFIG_CHARGER_INPUT_CURRENT), charge_mv);
}

int extpower_is_present(void)
{
	/*
	 * The charger will indicate VBUS presence if we're sourcing 5V,
	 * so exclude such ports.
	 */
	if (board_vbus_source_enabled(0))
		return 0;
	else
		return tcpm_get_vbus_level(0);
}

int pd_snk_is_vbus_provided(int port)
{
	if (port)
		panic("Invalid charge port\n");

	return rt946x_is_vbus_ready();
}

static void board_init(void)
{
	/* Enable TCPC alert interrupts */
	gpio_enable_interrupt(GPIO_USB_C0_PD_INT_ODL);

	/* Enable charger interrupts */
	gpio_enable_interrupt(GPIO_CHARGER_INT_ODL);

	/* Enable reboot / shutdown / sleep control inputs from AP */
	gpio_enable_interrupt(GPIO_WARM_RESET_REQ);
	gpio_enable_interrupt(GPIO_AP_EC_WATCHDOG_L);
	gpio_enable_interrupt(GPIO_AP_IN_SLEEP_L);

	/* Enable interrupts from BMI160 sensor. */
	gpio_enable_interrupt(GPIO_ACCEL_INT_ODL);

	/* Enable interrupt for the camera vsync. */
	gpio_enable_interrupt(GPIO_SYNC_INT);

	/* Enable interrupt from PMIC. */
	gpio_enable_interrupt(GPIO_PMIC_EC_RESETB);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_config_pre_init(void)
{
	STM32_RCC_AHBENR |= STM32_RCC_HB_DMA1;
	/*
	 * Remap USART1:
	 *
	 * Ch4: USART1_TX / Ch5: USART1_RX
	 */
	STM32_DMA_CSELR(STM32_DMAC_CH4) = (1 << 15) | (1 << 19);
}

#ifdef SECTION_IS_RO
void board_init_spi2(void)
{
	/* bootblock */
	/* Set SPI2 PB13/14/15 pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xfc000000;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR |= STM32_RCC_PB1_SPI2;
	STM32_RCC_APB1RSTR &= ~STM32_RCC_PB1_SPI2;

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;

	clock_wait_bus_cycles(BUS_APB, 1);
	gpio_config_module(MODULE_SPI, 1);

	STM32_SPI2_REGS->cr2 = STM32_SPI_CR2_FRXTH | STM32_SPI_CR2_DATASIZE(8) |
			       STM32_SPI_CR2_RXDMAEN | STM32_SPI_CR2_TXDMAEN;

	/* Manual CS, disable. */
	STM32_SPI2_REGS->cr1 = STM32_SPI_CR1_SPE;

	STM32_SPI2_REGS->dr = 0xff;
	STM32_SPI2_REGS->dr = 0xff;
	STM32_SPI2_REGS->dr = 0xff;
	STM32_SPI2_REGS->dr = 0xff;

	/* Enable the SPI peripheral */
	STM32_SPI2_REGS->cr1 |= STM32_SPI_CR1_SPE;
}
DECLARE_HOOK(HOOK_INIT, board_init_spi2, HOOK_PRIO_INIT_PWM - 1);
#endif

enum kukui_board_version {
	BOARD_VERSION_UNKNOWN = -1,
	BOARD_VERSION_REV0 = 0,
	BOARD_VERSION_REV1 = 1,
	BOARD_VERSION_REV2 = 2,
	BOARD_VERSION_REV3 = 3,
	BOARD_VERSION_REV4 = 4,
	BOARD_VERSION_REV5 = 5,
	BOARD_VERSION_REV6 = 6,
	BOARD_VERSION_REV7 = 7,
	BOARD_VERSION_REV8 = 8,
	BOARD_VERSION_REV9 = 9,
	BOARD_VERSION_REV10 = 10,
	BOARD_VERSION_REV11 = 11,
	BOARD_VERSION_REV12 = 12,
	BOARD_VERSION_REV13 = 13,
	BOARD_VERSION_REV14 = 14,
	BOARD_VERSION_REV15 = 15,
	BOARD_VERSION_COUNT,
};

struct {
	enum kukui_board_version version;
	int expect_mv;
} const kukui_boards[] = {
	{ BOARD_VERSION_REV0, 109 },   /* 51.1K , 2.2K(gru 3.3K) ohm */
	{ BOARD_VERSION_REV1, 211 },   /* 51.1k , 6.8K ohm */
	{ BOARD_VERSION_REV2, 319 },   /* 51.1K , 11K ohm */
	{ BOARD_VERSION_REV3, 427 },   /* 56K   , 17.4K ohm */
	{ BOARD_VERSION_REV4, 542 },   /* 51.1K , 22K ohm */
	{ BOARD_VERSION_REV5, 666 },   /* 51.1K , 30K ohm */
	{ BOARD_VERSION_REV6, 781 },   /* 51.1K , 39.2K ohm */
	{ BOARD_VERSION_REV7, 900 },   /* 56K   , 56K ohm */
	{ BOARD_VERSION_REV8, 1023 },  /* 47K   , 61.9K ohm */
	{ BOARD_VERSION_REV9, 1137 },  /* 47K   , 80.6K ohm */
	{ BOARD_VERSION_REV10, 1240 }, /* 56K   , 124K ohm */
	{ BOARD_VERSION_REV11, 1343 }, /* 51.1K , 150K ohm */
	{ BOARD_VERSION_REV12, 1457 }, /* 47K   , 200K ohm */
	{ BOARD_VERSION_REV13, 1576 }, /* 47K   , 330K ohm */
	{ BOARD_VERSION_REV14, 1684 }, /* 47K   , 680K ohm */
	{ BOARD_VERSION_REV15, 1800 }, /* 56K   , NC */
};
BUILD_ASSERT(ARRAY_SIZE(kukui_boards) == BOARD_VERSION_COUNT);

#define THRESHOLD_MV 56 /* Simply assume 1800/16/2 */

int board_get_version(void)
{
	static int version = BOARD_VERSION_UNKNOWN;
	int mv;
	int i;

	if (version != BOARD_VERSION_UNKNOWN)
		return version;

	gpio_set_level(GPIO_EC_BOARD_ID_EN_L, 0);
	/* Wait to allow cap charge */
	msleep(10);
	mv = adc_read_channel(ADC_BOARD_ID);

	if (mv == ADC_READ_ERROR)
		mv = adc_read_channel(ADC_BOARD_ID);

	gpio_set_level(GPIO_EC_BOARD_ID_EN_L, 1);

	for (i = 0; i < BOARD_VERSION_COUNT; ++i) {
		if (mv < kukui_boards[i].expect_mv + THRESHOLD_MV) {
			version = kukui_boards[i].version;
			break;
		}
	}

	/*
	 * Disable ADC module after we detect the board version,
	 * since this is the only thing ADC module needs to do
	 * for this board.
	 */
	if (version != BOARD_VERSION_UNKNOWN)
		adc_disable();

	return version;
}

/* Motion sensors */
/* Mutexes */
static struct mutex g_base_mutex;

static struct bmi160_drv_data_t g_bmi160_data;

/* Matrix to rotate accelerometer into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ FLOAT_TO_FP(-1), 0,  0},
	{ 0,  FLOAT_TO_FP(-1),  0},
	{ 0,  0, FLOAT_TO_FP(1)}
};

struct motion_sensor_t motion_sensors[] = {
	/*
	 * Note: bmi160: supports accelerometer and gyro sensor
	 * Requirement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	[LID_ACCEL] = {
	 .name = "Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .rot_standard_ref = &base_standard_ref,
	 .default_range = 4,  /* g */
	 .min_frequency = BMI160_ACCEL_MIN_FREQ,
	 .max_frequency = BMI160_ACCEL_MAX_FREQ,
	 .config = {
		 /* Enable accel in S0 */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 10000 | ROUND_UP_FLAG,
			 .ec_rate = 100 * MSEC,
		 },
	 },
	},
	[LID_GYRO] = {
	 .name = "Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_BMI160,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &bmi160_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_bmi160_data,
	 .port = I2C_PORT_ACCEL,
	 .addr = BMI160_ADDR0,
	 .default_range = 1000, /* dps */
	 .rot_standard_ref = &base_standard_ref,
	 .min_frequency = BMI160_GYRO_MIN_FREQ,
	 .max_frequency = BMI160_GYRO_MAX_FREQ,
	},
	[VSYNC] = {
	 .name = "Camera vsync",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_GPIO,
	 .type = MOTIONSENSE_TYPE_SYNC,
	 .location = MOTIONSENSE_LOC_CAMERA,
	 .drv = &sync_drv,
	 .default_range = 0,
	 .min_frequency = 0,
	 .max_frequency = 1,
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

int board_allow_i2c_passthru(int port)
{
	return (port == I2C_PORT_VIRTUAL_BATTERY);
}

int tablet_get_mode(void)
{
	/* Always in tablet mode */
	return 1;
}

void usb_charger_set_switches(int port, enum usb_switch setting)
{
}

/******************************************************************************/
/* Transfer bootblock over SPI by emulating eMMC protocol. */
#ifdef SECTION_IS_RO

static stm32_spi_regs_t *const spi_emmc = STM32_SPI2_REGS;
/* 1024 bytes of buffer is enough for ~0.6ms @ 13Mhz */
#define SPI_RX_BUF_SIZE 1024
#define SPI_RX_BUF_SIZE_32 (SPI_RX_BUF_SIZE/4)
static uint32_t in_msg[SPI_RX_BUF_SIZE_32] __aligned(4);

/* Macros to advance in the circular buffer */
#define RX_BUF_NEXT_32(i) (((i) + 1) & (SPI_RX_BUF_SIZE_32 - 1))
#define RX_BUF_DEC_32(i, j) (((i) - (j)) & (SPI_RX_BUF_SIZE_32 - 1))
#define RX_BUF_PREV_32(i) RX_BUF_DEC_32((i), 1)

static int bootblock_transfer_try;

enum emmc_cmd {
	EMMC_ERROR = -1,
	EMMC_IDLE = 0,
	EMMC_PRE_IDLE,
	EMMC_BOOT,
};

static const struct dma_option dma_tx_option = {
	STM32_DMAC_SPI2_TX, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
};

static const struct dma_option dma_rx_option = {
	STM32_DMAC_SPI2_RX, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT |
	STM32_DMA_CCR_CIRC
};

static void bootblock_transfer(void)
{
	const uint32_t bootblock_size = &bootblock_end - &bootblock_start;
	dma_chan_t *txdma = dma_get_channel(STM32_DMAC_SPI2_TX);

	dma_prepare_tx(&dma_tx_option, bootblock_size, &bootblock_start);
	dma_go(txdma);

	bootblock_transfer_try++;
	CPRINTS("eMMC transfer(%d)", bootblock_transfer_try);
}

static void bootblock_stop(void)
{
	dma_disable(STM32_DMAC_SPI2_TX);
	while ((spi_emmc->sr & STM32_SPI_SR_FTLVL) != 0)
		;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
}

static uint32_t letobe(uint32_t val)
{
	return (val & 0x000000ff) << 24 | (val & 0x0000ff00) << 8 |
	       (val & 0x00ff0000) >> 8  | (val & 0xff000000) >> 24;
}

static enum emmc_cmd emmc_parse_command(int index)
{
	int32_t shift0, mask1;
	uint32_t data[3];

	mask1 = 0x00000000;

	/* Figure out alignment (cmd starts with 01) */
	if (in_msg[index] == 0xffffffff)
		return EMMC_ERROR;

	data[0] = letobe(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[1] = letobe(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[2] = letobe(in_msg[index]);

	/* Number of leading ones. */
	shift0 = __builtin_clz(~data[0]);

	if (shift0 > 0)
		mask1 = (int32_t)0x80000000 >> (shift0-1);

	data[0] = (data[0] << shift0) | ((data[1] & mask1) >> (32-shift0));
	data[1] = (data[1] << shift0) | ((data[2] & mask1) >> (32-shift0));

	if (data[0] == 0x40000000 && data[1] == 0x0095ffff) {
		/* 400000000095 GO_IDLE_STATE */
		CPRINTS("eMMC goIdle");
		return EMMC_IDLE;
	}

	if (data[0] == 0x40f0f0f0 && data[1] == 0xf0fdffff) {
		/* 40f0f0f0f0fd GO_PRE_IDLE_STATE */
		CPRINTS("eMMC goPreIdle");
		return EMMC_PRE_IDLE;
	}

	if (data[0] == 0x40ffffff && data[1] == 0xfae5ffff) {
		/* 40fffffffae5 BOOT_INITIATION */
		CPRINTS("eMMC bootInit");
		return EMMC_BOOT;
	}

	CPRINTS("eMMC error");
	return EMMC_ERROR;
}

void emmc_cmd_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_EMMC);
}

void emmc_task(void *u)
{
	/* Both are 32-bit indexes. */
	int dma_pos, i;
	dma_chan_t *rxdma = dma_get_channel(STM32_DMAC_SPI2_RX);
	int tx = 0;
	enum emmc_cmd cmd;

	gpio_enable_interrupt(GPIO_EMMC_CMD);

	dma_start_rx(&dma_rx_option, sizeof(in_msg), in_msg);

	/* Enable internal chip select */
	spi_emmc->cr1 &= ~STM32_SPI_CR1_SSI;

	while (1) {
		/* Wait for a command */
		task_wait_event(-1);

		/* Since we round down, in theory we sould not  */
		dma_pos = dma_bytes_done(rxdma, sizeof(in_msg)) / 4;
		i = RX_BUF_PREV_32(dma_pos);

		/*
		 * By now, bus should be idle again (it takes <10us to transmit
		 * a command).
		 */
		if (in_msg[i] != 0xffffffff) {
			CPRINTF("?");
			/* TODO: We should probably just retry. */
			continue;
		}

		/* Look for a command, from the end of the buffer. */
		while (i != dma_pos && in_msg[i] == 0xffffffff)
			i = RX_BUF_PREV_32(i);

		/* We missed the command? */
		if (i == dma_pos) {
			CPRINTF("!");
			continue;
		}

		/* We found the end, now find the beginning. */
		i = RX_BUF_DEC_32(i, 2);
		while (i != dma_pos && in_msg[i] == 0xffffffff)
			i = RX_BUF_NEXT_32(i);

		cmd = emmc_parse_command(i);

		/* When not transferring, all we care about is EMMC_BOOT. */
		if (!tx) {
			if (cmd == EMMC_BOOT) {
				tx = 1;
				bootblock_transfer();
			}
		} else {
			/* Abort (should be Idle, but react to Pre-Idle too) */
			if (cmd == EMMC_IDLE || cmd == EMMC_PRE_IDLE) {
				bootblock_stop();
				tx = 0;
			}
		}
	}
}
#endif /* SECTION_IS_RO */
