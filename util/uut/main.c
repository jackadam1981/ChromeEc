/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "com_port.h"
#include "main.h"
#include "misc_util.h"
#include "opr.h"

/*----------------------------------------------------------------------------
 * Constant definitions
 *---------------------------------------------------------------------------
 */

#define MAX_FILE_NAME_SIZE 512
#define MAX_PARAM_SIZE 32
#define MAX_MSG_SIZE 128

/* Default values */
#define DEFAULT_BAUD_RATE 115200
#define DEFAULT_PORT_NAME "ttyS0"
#define DEFAULT_DEV_NUM 0

#define GET_BASE(str) \
	(((str[0] == 'x') || (str[1] == 'x')) ? BASE_HEXADECIMAL : BASE_DECIMAL)

/*---------------------------------------------------------------------------
 * Global variables
 *---------------------------------------------------------------------------
 */

char port_name[MAX_PARAM_SIZE];
struct comport_fields port_cfg;
uint32_t baudrate;

char opr_name[MAX_PARAM_SIZE];
uint32_t dev_num;

char file_name[MAX_FILE_NAME_SIZE];
char addr_str[MAX_PARAM_SIZE];
char size_str[MAX_PARAM_SIZE];

bool verbose;
bool console;

/*---------------------------------------------------------------------------
 * Local variables
 *---------------------------------------------------------------------------
 */
static const char tool_name[] = {"LINUX UART Update Tool"};
static const char tool_version[] = {"2.0.1"};

/*---------------------------------------------------------------------------
 * Functions prototypes
 *---------------------------------------------------------------------------
 */

static void param_parse_cmd_line(int argc, char *argv[]);
static void param_check_port_num(char *port_name);
static void param_check_opr_num(const char *opr);
static uint32_t param_get_file_size(const char *file_name);
static uint32_t param_get_str_size(char *string);
static void main_print_version(void);
static void tool_usage(void);
static void exit_uart_app(uint32_t exit_status);
static int str_cmp_no_case(const char *s1, const char *s2);

enum EXIT_CODE {
	EC_OK = 0x00,
	EC_PORT_ERR = 0x01,
	EC_BAUDRATE_ERR = 0x02,
	EC_SYNC_ERR = 0x03,
	EC_DEV_NUM_ERR = 0x04,
	EC_OPR_MUM_ERR = 0x05,
	EC_ALIGN_ERR = 0x06,
	EC_FILE_ERR = 0x07,
	EC_UNSUPPORTED_CMD_ERR = 0x08
};

/*---------------------------------------------------------------------------
 * Function implementation
 *---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 * Function:	main
 *
 * Parameters:		argc - Argument Count.
 *			argv - Argument Vector.
 * Returns:		1 for a successful operation, 0 otherwise.
 * Side effects:
 * Description:
 *		Console application main operation.
 *---------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
	char *stop_str;
	char aux_buf[MAX_FILE_NAME_SIZE];
	uint32_t size = 0;
	uint32_t addr = 0;
	enum sync_result sr;

	if (argc <= 1)
		exit(EC_UNSUPPORTED_CMD_ERR);

	/* Setup defaults */
	strncpy(port_name, DEFAULT_PORT_NAME, sizeof(port_name));
	baudrate = DEFAULT_BAUD_RATE;
	dev_num = DEFAULT_DEV_NUM;
	opr_name[0] = '\0';
	verbose = true;
	console = false;

	param_parse_cmd_line(argc, argv);

	param_check_port_num(port_name);

	/* Configure COM Port parameters */
	port_cfg.baudrate = MAX(baudrate, BR_LOW_LIMIT);
	port_cfg.byte_size = CS8;
	port_cfg.flow_control = 0;
	port_cfg.parity = 0;
	port_cfg.stop_bits = 0;

	/*
	 * Open a ComPort device. If user haven't specified such, use ComPort 1
	 */
	if (opr_open_port(port_name, port_cfg) != true)
		exit(EC_PORT_ERR);

	if (baudrate == 0) { /* Scan baud rate range */
		opr_scan_baudrate();
		exit(EC_OK);
	}

	/* Verify Host and Device are synchronized */
	DISPLAY_MSG(("Performing a Host/Device synchronization check...\n"));
	sr = opr_check_sync(baudrate);
	if (sr != SR_OK) {
		display_color_msg(FAIL,
			"Host/Device synchronization failed, error = %lu.\n",
			sr);
		exit_uart_app(EC_SYNC_ERR);
	}

	param_check_opr_num(opr_name);

	/* Write buffer data to chosen address */
	if (strcmp(opr_name, OPR_WRITE_MEM) == 0) {
		addr = strtoul(addr_str, &stop_str, GET_BASE(addr_str));

		/*
		 * Copy the input string to an auxiliary buffer, since string
		 * is altered by param_get_str_size
		 */
		memcpy(aux_buf, file_name, sizeof(file_name));

		/* Retrieve input size */
		if (console)
			size = param_get_str_size(aux_buf);
		else
			size = param_get_file_size(file_name);

		/* Ensure non-zero size */
		if (size == 0)
			exit_uart_app(EC_FILE_ERR);

		opr_write_mem(file_name, addr, size);
	} else if (strcmp(opr_name, OPR_READ_MEM) == 0) {
		/* Read data to chosen address */

		addr = strtoul(addr_str, &stop_str, GET_BASE(addr_str));
		size = strtoul(size_str, &stop_str, GET_BASE(size_str));

		opr_read_mem(file_name, addr, size);
	} else if (strcmp(opr_name, OPR_EXECUTE_EXIT) == 0) {
		/* Execute From Address a non-return code */

		addr = strtoul(addr_str, &stop_str, GET_BASE(addr_str));

		opr_execute_exit(addr);
		exit_uart_app(EC_OK);
	} else if (strcmp(opr_name, OPR_EXECUTE_CONT) == 0) {
		/* Execute From Address a returnable code */

		addr = strtoul(addr_str, &stop_str, GET_BASE(addr_str));

		opr_execute_return(addr);
	} else {
		exit_uart_app(EC_UNSUPPORTED_CMD_ERR);
	}

	exit_uart_app(EC_OK);
	return 0;
}

/*---------------------------------------------------------------------------
 * Function:	param_parse_cmd_line
 *
 * Parameters:		argc - Argument Count.
 *			argv - Argument Vector.
 * Returns:		None.
 * Side effects:
 * Description:
 *		Parse command line parameters.
 *---------------------------------------------------------------------------
 */
static void param_parse_cmd_line(int argc, char *argv[])
{
	uint32_t i = 0;

	while (--argc > 0) {
		/* Skip application name */
		i++;

		/*------------------------------------------------------------
		 * Help Message
		 *-----------------------------------------------------------
		 */
		if (str_cmp_no_case(*(argv + i), "-version") == 0) {
			main_print_version();
			exit(EC_OK);
		}

		/*-----------------------------------------------------------
		 * Help Message
		 *-----------------------------------------------------------
		 */
		if (str_cmp_no_case(*(argv + i), "-help") == 0) {
			tool_usage();
			opr_usage();
			exit(EC_OK);
		}
		/*-----------------------------------------------------------
		 * Verbose/Silent Mode
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-silent") == 0) {
			verbose = false;
			continue;
		}
		/*-----------------------------------------------------------
		 * File/Console Mode
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-console") == 0) {
			console = true;
			continue;
		}
		/*-----------------------------------------------------------
		 * Baud Rate Value
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-baudrate") == 0) {
			if (sscanf(*(argv + 1 + i), "%du", &baudrate) == 0)
				exit(EC_BAUDRATE_ERR);
		}
		/*-----------------------------------------------------------
		 * Operation Number
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-opr") == 0) {
			if (sscanf(*(argv + 1 + i), "%s", (char *)opr_name) ==
				0)
				exit(EC_OPR_MUM_ERR);
		}
		/*-----------------------------------------------------------
		 * Port Number
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-port") == 0) {
			if (sscanf(*(argv + 1 + i), "%s", (char *)port_name) ==
				0)
				exit(EC_PORT_ERR);
		}
		/*-----------------------------------------------------------
		 * File Name
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-file") == 0) {
			if (sscanf(*(argv + 1 + i), "%s", file_name) == 0)
				exit(EC_FILE_ERR);
		}
		/*-----------------------------------------------------------
		 * Start memory address
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-addr") == 0) {
			if (sscanf(*(argv + 1 + i), "%s", addr_str) == 0)
				;
		}
		/*-----------------------------------------------------------
		 * Size of data to read
		 *-----------------------------------------------------------
		 */
		else if (str_cmp_no_case(*(argv + i), "-size") == 0) {
			if (sscanf(*(argv + 1 + i), "%s", size_str) == 0)
				;
		}
		/*-----------------------------------------------------------
		 * Unknown Parameter
		 *-----------------------------------------------------------
		 */
		else {
			display_color_msg(FAIL,
				"ERROR: Parameter '%s' is not supported\n",
				*(argv + i));
			DISPLAY_MSG(("Use '-help' for menu\n"));
			exit(1);
		}

		i++;
		argc--;
	}
}

/*---------------------------------------------------------------------------
 * Function:        param_check_port_num
 *
 * Parameters:      port_name - Pointer to port name.
 * Returns:         none.
 * Side effects:
 * Description:
 *                  Verify the validity of input port Number.
 *---------------------------------------------------------------------------
 */
static void param_check_port_num(char *port_name)
{
	if ((strncmp(port_name, COMP_PORT_PREFIX_1,
		     strlen(COMP_PORT_PREFIX_1)) != 0) &&
		(strncmp(port_name, COMP_PORT_PREFIX_2,
			 strlen(COMP_PORT_PREFIX_2)) != 0) &&
		(strncmp(port_name, COMP_PORT_PREFIX_3,
			 strlen(COMP_PORT_PREFIX_3)) != 0)) {
		display_color_msg(FAIL, "ERROR: %s is an invalid serial port\n",
			port_name);
		DISPLAY_MSG(("Serial port %s will be used as default\n\n",
			DEFAULT_PORT_NAME));
		strcpy(port_name, DEFAULT_PORT_NAME);
	}
}

/*---------------------------------------------------------------------------
 * Function:	param_check_opr_num
 *
 * Parameters:	opr - Operation Number.
 * Returns:	none.
 * Side effects:
 * Description:
 *		Verify the validity of operation Number.
 *---------------------------------------------------------------------------
 */
static void param_check_opr_num(const char *opr)
{
	if ((strcasecmp(opr, OPR_WRITE_MEM) != 0) &&
		(strcasecmp(opr, OPR_READ_MEM) != 0) &&
		(strcasecmp(opr, OPR_EXECUTE_EXIT) != 0) &&
		(strcasecmp(opr, OPR_EXECUTE_CONT) != 0)) {
		display_color_msg(FAIL,
			"ERROR: Operation %s not supported, Supported "
			"operations are %s, %s, %s & %s\n",
			opr, OPR_WRITE_MEM, OPR_READ_MEM, OPR_EXECUTE_EXIT,
			OPR_EXECUTE_CONT);
		exit_uart_app(EC_OPR_MUM_ERR);
	}
}

/*---------------------------------------------------------------------------
 * Function:	param_get_file_size
 *
 * Parameters:	file_name - input file name.
 * Returns:	size of file (in bytes).
 * Side effects:
 * Description:
 *		Retrieve the size (in bytes) of a given file.
 *--------------------------------------------------------------------------
 */
static uint32_t param_get_file_size(const char *file_name)
{
	FILE *h_file;
	uint32_t file_size = 0;

	/* Open the source file */
	h_file = fopen(file_name, "rb");
	if (h_file == NULL) {
		display_color_msg(FAIL,
			"ERROR: Could not open source file [%s]\n", file_name);
		return 0;
	}

	/* Get the file size */
	fseek(h_file, 0, SEEK_END);
	file_size = ftell(h_file);

	fclose(h_file);
	return file_size;
}

/*---------------------------------------------------------------------------
 * Function:	param_get_str_size
 *
 * Parameters:	string - input string.
 * Returns:	size of double-words (in bytes).
 * Side effects:
 * Description:
 *	Retrieve the size (in bytes) of double-word values in a given string.
 *	E.g., given the string "1234 AB5678 FF", return 12 (for three
 *	double-words).
 *---------------------------------------------------------------------------
 */
static uint32_t param_get_str_size(char *string)
{
	uint32_t str_size = 0;
	char seps[] = " ";
	char *token = NULL;

	/* Verify string is non-NULL */
	if ((strlen(string) == 0) || (string == NULL)) {
		display_color_msg(FAIL,
			"ERROR: Zero length input string provided\n", string);
		return 0;
	}

	/* Read first token from string */
	token = strtok(string, seps);

	/* Loop while there are tokens in "string" */
	while (token != NULL) {
		str_size++;
		token = strtok(NULL, seps);
	}

	/* Refer to each token as a double-word */
	str_size *= sizeof(uint32_t);
	return str_size;
}

/*--------------------------------------------------------------------------
 * Function:	tool_usage
 *
 * Parameters:	none.
 * Returns:	none.
 * Side effects:
 * Description:
 *		Prints the console application help menu.
 *--------------------------------------------------------------------------
 */
static void tool_usage(void)
{
	printf("%s version %s\n\n", tool_name, tool_version);
	printf("General switches:\n");
	printf("       -version         - Print version\n");
	printf("       -help            - Help menu\n");
	printf("       -silent          - Suppress verbose mode (default is "
	       "verbose ON)\n");
	printf("       -console         - Print data to console (default is "
	       "print to file)\n");
	printf("       -port <name>     - Serial port name (default is %s)\n",
		DEFAULT_PORT_NAME);
	printf("       -baudrate <num>  - COM Port baud-rate (default is %d)\n",
		DEFAULT_BAUD_RATE);
	printf("\n");
	printf("Operation specific switches:\n");
	printf("       -opr   <name>    - Operation number (see list below)\n");
	printf("       -file  <name>    - Input/output file name\n");
	printf("       -addr  <num>     - Start memory address\n");
	printf("       -size  <num>     - Size of data to read\n");
	printf("\n");
}

/*--------------------------------------------------------------------------
 * Function:	main_print_version
 *
 * Parameters:	none
 * Returns:	none
 * Side effects:
 * Description:
 *		This routine prints the tool version
 *--------------------------------------------------------------------------
 */
static void main_print_version(void)
{
	printf("%s version %s\n\n", tool_name, tool_version);
}

/*---------------------------------------------------------------------------
 * Function:	exit_uart_app
 *
 * Parameters:	none.
 * Returns:	none.
 * Side effects:
 * Description:
 *		Exit "nicely" the application.
 *---------------------------------------------------------------------------
 */
static void exit_uart_app(uint32_t exit_status)
{
	if (opr_close_port() != true)
		display_color_msg(FAIL, "ERROR: Port close failed.\n");

	exit(exit_status);
}

/*---------------------------------------------------------------------------
 * Function:	display_color_msg
 *
 * Parameters:
 *		success - SUCCESS for successful message, FAIL for erroneous
 *			  massage.
 *		fmt     - Massage to dispaly (format and arguments).
 *
 * Returns:	none
 * Side effects: Using DISPLAY_MSG macro.
 * Description:
 *		This routine displays a message using color attributes:
 *		In case of a successful message, use green foreground text on
 *		black background.
 *		In case of an erroneous message, use red foreground text on
 *		black background.
 *---------------------------------------------------------------------------
 */
void display_color_msg(bool success, char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

/*--------------------------------------------------------------------------
 * Function:	 str_cmp_no_case
 * Parameters:	 s1, s2: Strings to compare.
 * Return:	 function returns an integer less than, equal to, or
 *		 greater than zero if s1 (or the first n bytes thereof) is
 *		 found, respectively, to be less than, to match, or be
 *		 greater than s2.
 * Description:	 Compare two string without case sensitive.
 *--------------------------------------------------------------------------
 */
static int str_cmp_no_case(const char *s1, const char *s2)
{
	return strcasecmp(s1, s2);
}
