#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"
#include "timer.h"
#include "gpio.h"
#include "watchdog.h"
#include "chipset.h"
#include "registers.h"
#include "system.h"
#include "0x05.h"

/* Console output macros */
#define CPRINTS(format, args...) ccprints(format, ## args)

int SPIEnableWriteStatusRegiser(int spi_cmd);
int DisableWrite(void);
int EnableWrite(void);
int SPIWriteStatusReg(int arg);
int SPIReadStatusReg(void);
int DisableWP(void);
int EnableWP(void);
int ResetSPIIF(void);
int DisableMPU(void);
int EnableMPU(void);
int WaitSPIROMReady(void);

#define SLAVE0 0x10
#define SLAVE1 0x12
#define SLAVE2 0x14
#define SLAVE3 0x16

#define BANKNUM 0x03

#define PARADE_VERSION 5

static int tcpc_port;

static int WriteReg(int I2C_addr, int reg_offset, int data)
{
	watchdog_reload();
	if (i2c_write8(i2c_ports[tcpc_port].port, I2C_addr, reg_offset, data))
		CPRINTS("Write failed");
	return EC_SUCCESS;
}

static int ReadReg(int I2C_addr, int reg_offset)
{
	int  data = 0;

	watchdog_reload();
	if (i2c_read8(i2c_ports[tcpc_port].port, I2C_addr, reg_offset, &data))
		CPRINTS("Read failed");
	return data;
}

int SPIEnableWriteStatusRegiser(int spi_cmd)
{
	DisableWP();

	WriteReg(SLAVE2, 0x90, spi_cmd);
	WriteReg(SLAVE2, 0x92, 0x00);
	WriteReg(SLAVE2, 0x93, 0x05);

	return EnableWP();
}

int DisableWrite(void)
{
	int fail = EC_SUCCESS;

	DisableMPU();

	SPIEnableWriteStatusRegiser(0x06);

	//Enable all protection
	SPIWriteStatusReg(0x9c);
	//Wait until SPI module ready
	WaitSPIROMReady();

	if ((SPIReadStatusReg() & 0x9c) != 0x9c)
		fail = EC_ERROR_INVAL;

	WriteReg(SLAVE2, 0xda, 0x00);

	return fail;
}

int EnableWrite(void)
{
	int fail = EC_SUCCESS;
	int looptime = 0;
	int val = 0;

	DisableMPU();

	//Disable all protection
	SPIEnableWriteStatusRegiser(0x06);
	SPIWriteStatusReg(0x00);
	//Wait until SPI module ready
	WaitSPIROMReady();

	if ((SPIReadStatusReg() & 0x00) != 0x00)
		fail = EC_ERROR_INVAL;
	else {
		DisableWP();

		while ((looptime < 20) && (val != 0x01)) {
			WriteReg(SLAVE2, 0xda, 0xaa);
			WriteReg(SLAVE2, 0xda, 0x55);
			WriteReg(SLAVE2, 0xda, 0x50);
			WriteReg(SLAVE2, 0xda, 0x41);
			WriteReg(SLAVE2, 0xda, 0x52);
			WriteReg(SLAVE2, 0xda, 0x44);

			val = ReadReg(SLAVE2, 0xda);
			looptime++;
		}
		if (val == 0x00)
			fail = EC_ERROR_INVAL;
	}

	return fail;
}

int SPIWriteStatusReg(int arg)
{
	DisableWP();

	WriteReg(SLAVE2, 0x90, 0x01);
	WriteReg(SLAVE2, 0x90, arg);
	WriteReg(SLAVE2, 0x92, 0x01);
	WriteReg(SLAVE2, 0x93, 0x05);

	return EnableWP();
}

int SPIReadStatusReg(void)
{
	WriteReg(SLAVE2, 0x90, 0x05);
	WriteReg(SLAVE2, 0x92, 0x00);
	WriteReg(SLAVE2, 0x93, 0x01);

	//Read back the status value
	return ReadReg(SLAVE2, 0x91);
}

int SPIEnableWriteStatusRegister(void)
{
	DisableWP();

	WriteReg(SLAVE2, 0x90, 0x06);
	WriteReg(SLAVE2, 0x92, 0x00);
	WriteReg(SLAVE2, 0x93, 0x05);

	return EnableWP();
}

int ResetSPIIF(void)
{
	DisableWP();

	WriteReg(SLAVE2, 0x90, 0x04);
	WriteReg(SLAVE2, 0x92, 0x00);
	WriteReg(SLAVE2, 0x93, 0x05);

	EnableWP();

	watchdog_reload();
	msleep(500);

	return EC_SUCCESS;
}

int SPIWriteEnable(void)
{
	DisableWP();

	WriteReg(SLAVE2, 0x90, 0x06);
	WriteReg(SLAVE2, 0x92, 0x00);
	WriteReg(SLAVE2, 0x93, 0x05);

	return EnableWP();
}

int WaitSPIROMReady(void)
{
	int status;

	//Wait SPI interface ready
	do
		status = ReadReg(SLAVE2, 0x9e);
	while (status & 0x0c);

	//Wait SPI ROM ready
	do {
		WriteReg(SLAVE2, 0x90, 0x05);
		WriteReg(SLAVE2, 0x92, 0x00);
		WriteReg(SLAVE2, 0x93, 0x01);

		//Wait SPI interface ready
		do
			status = ReadReg(SLAVE2, 0x93);
		while (status & 0x01);

		status = ReadReg(SLAVE2, 0x91);
	} while (status & 0x01);

	return EC_SUCCESS;
}

int SPISectorErase(int addr24, int addr16, int SPI_command)
{
	SPIWriteEnable();

	WriteReg(SLAVE2, 0x90, SPI_command);
	WriteReg(SLAVE2, 0x90, addr24);
	WriteReg(SLAVE2, 0x90, addr16);
	WriteReg(SLAVE2, 0x90, 0x00);
	WriteReg(SLAVE2, 0x92, 0x03);

	WriteReg(SLAVE2, 0x93, 0x05);

	WaitSPIROMReady();

	return EC_SUCCESS;
}

int EnableWP(void)
{
	if (ReadReg(SLAVE1, 0xf0))
		return WriteReg(SLAVE2, 0x2a, 0x10);
	else
		return WriteReg(SLAVE2, 0x2a, 0x00);
}

int DisableWP(void)
{
	if (ReadReg(SLAVE1, 0xf0))
		return WriteReg(SLAVE2, 0x2a, 0x00);
	else
		return WriteReg(SLAVE2, 0x2a, 0x10);
}

int DisableMPU(void)
{
	WriteReg(SLAVE2, 0xd6, 0xc0);
	WriteReg(SLAVE2, 0xd6, 0x40);

	return ResetSPIIF();
}

int EnableMPU(void)
{
	return WriteReg(SLAVE2, 0xd6, 0x00);
}

int ShowVersionInfo(void)
{
	/* int ver = ReadReg(SLAVE0, 0x90); */
	int ver = ReadReg(SLAVE3, 0x82);

	ccprintf("ParadeTech Firmware Version: 0x%02X\n", ver);

	return ver;
}

int ROMIdentify(void)
{
	int status;
	int i;
	int buf[2] = {0};

	WriteReg(SLAVE2, 0x90, 0x90);
	WriteReg(SLAVE2, 0x90, 0x00);
	WriteReg(SLAVE2, 0x90, 0x00);
	WriteReg(SLAVE2, 0x90, 0x00);
	WriteReg(SLAVE2, 0x92, 0x13);
	WriteReg(SLAVE2, 0x93, 0x01);

	do
		status = ReadReg(SLAVE2, 0x9e);
	while (status & 0x01);

	for (i = 0; i < 2; i++)
		buf[i] = ReadReg(SLAVE2, 0x91);

	if (((buf[0] != 0x1C) && (buf[1] != 0x11)) &&
	    ((buf[0] != 0xEF) && (buf[1] != 0x11))) {
		CPRINTS("INVALID ParadeTech SPI ROM - Contact EC Team!\n");
		ccprintf("chip id: 0x%02x, 0x%02x\n", buf[0], buf[1]);
		while(1) {
			watchdog_reload();
			msleep(1);
		}
	}

	return EC_SUCCESS;
}

int parade_reset(void)
{

	gpio_set_level(GPIO_USB_PD_RST_L, 0);
	watchdog_reload();
	msleep(5);
	gpio_set_level(GPIO_USB_PD_RST_L, 1);
	watchdog_reload();
	msleep(500);

	return 0;
}

void ParadeFirmwareMain(int forced)
{
	int ver;
	int address;
	int status;
	int i, j;

	//enable page2 access
	ReadReg(SLAVE3, 0xa0);
	WriteReg(SLAVE3, 0xa0, 0x30 );

	ver = ShowVersionInfo();

	if ((ver < PARADE_VERSION) || forced) {
		if (system_get_reset_flags() != RESET_FLAG_RESET_PIN) {
			ccprintf("Please remove all power source " \
				 "and then apply power via Type-C " \
				 "Port 0 only again!\n");
		} else {
			if (forced)
				ccprintf("Will attempt to force update.\n");
			DisableMPU();
			ROMIdentify();

			ccprintf("ParadeTech TCPC Firmware Upgrade In Progress!\n");

			DisableWP();
			SPIWriteEnable();
			EnableWrite();

			/* Erase */
			for (address = 0x30000; address < 0x33000; address += 0x1000) {
				ccprintf("Erasing Address 0x%06X - 0x%06X ",address & 0xFFF000, (address & 0xFFF000) + 0x1000 - 1);
				SPISectorErase((address >> (8 * 2)) & 0xFF, (address >> (8 * 1)) & 0xFF, 0x20);
				ccprintf("- Done\n");
			}

			/* Program */
			address = 0x30000;
			for (j = 0; j < sizeof(fwcode); j+=8) {
				ccprintf("Programming Address 0x%06X - 0x%06X: ", address, address + 0x8 - 1);

				SPIWriteEnable();

				WriteReg(SLAVE2, 0x90, 0x02);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 2)) & 0xFF);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 1)) & 0xFF);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 0)) & 0xFF);

				for (i = 0; i < 8; i++) {
					WriteReg(SLAVE2, 0x90, fwcode[address - 0x30000 + i]);
					ccprintf("%02X ", fwcode[address - 0x30000 + i]);
				}

				WriteReg(SLAVE2, 0x92, 0x0b);
				WriteReg(SLAVE2, 0x93, 0x05);

				WaitSPIROMReady();

				ccprintf("- Done\n");

				address+=8;
			}

			/* Verify */
			address = 0x30000;
			for (j = 0; j < sizeof(fwcode); j+=8) {
				ccprintf("Verifing Address 0x%06X - 0x%06X ", address, address + 0x8 - 1);

				SPIWriteEnable();

				WriteReg(SLAVE2, 0x90, 0x03);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 2)) & 0xFF);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 1)) & 0xFF);
				WriteReg(SLAVE2, 0x90, (address >> (8 * 0)) & 0xFF);
				WriteReg(SLAVE2, 0x92, 0x73);
				WriteReg(SLAVE2, 0x93, 0x01);

				do
					status = ReadReg(SLAVE2, 0x9e);
				while (status & 0x01);

				for (i = 0; i < 8; i++) {
					if (ReadReg(SLAVE2, 0x91) != fwcode[address - 0x30000 + i]) {
						ccprintf("- Program Mistach\n");
						ccprintf("Please remove all power source " \
							 "and then apply power via Type-C " \
							 "again!\n");
						while(1) {
							watchdog_reload();
							msleep(1);
						}
					}
				}

				ccprintf("- Done\n");

				address+=8;
			}

			DisableWrite();
			EnableWP();
			EnableMPU();

			ccprintf("Please remove all power source " \
				 "and then apply power via Type-C.\n");

			ccprintf("Use the 'parade <port>' command to verify"
				 " the running version. \n");
		}
	} else {
		ccprintf("Parade Firmware is already up-to-date! :)\n");
	}

	parade_reset();
}

static void report_all_versions(void)
{
	int i;

	msleep(1000);

	ccprintf("\nExpected Firmware Version: %d\n", PARADE_VERSION);
	ccprintf("TCPC Firmware Versions:\n");
	for (i=0; i<i2c_ports_used; i++) {
		tcpc_port = i;
		ccprintf("Port %d:\n\t", i);
		ShowVersionInfo();
		ccprintf("\n");
	}

	ccprintf("To update, use the 'parade <port> update' command.\n");
}
DECLARE_HOOK(HOOK_INIT, report_all_versions, HOOK_PRIO_LAST);

int parade_update(int argc, char **argv)
{
	int port;

	if ((argc < 2) || (argc > 4))
		return EC_ERROR_PARAM_COUNT;

	port = atoi(argv[1]);
	if ((port < 0) || (port > 2))
		return EC_ERROR_PARAM1;

	tcpc_port = port;

	if (argc == 2) {
		ShowVersionInfo();
	} else if (strcasecmp(argv[2], "update") == 0) {
		if ((argc == 4) && (strcasecmp(argv[3], "force") == 0))
			ParadeFirmwareMain(1);
		else
			ParadeFirmwareMain(0);
	} else {
		return EC_ERROR_PARAM2;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(parade, parade_update,
			"<TCPC port number> [update [force] ]",
			"Updates TCPC FW");
