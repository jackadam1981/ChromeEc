/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Filename: rtkupdate.c         For Chipset: RTK EC
 *
 * Function: RTK Flash Utility
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define TOOL_VERSION 0.13
#define SUB_VERSION 0.2

#define DEFAULT_BAUD_RATE 115200
#define BAUDRATE B115200
#define UART_SYNC_BYTE 0x5A
#define UART_SYNC_RESPONSE 0xA5
#define UPLOAD_HEADER_COMMAND 0x09
#define WP_COMMAND 0x0C
#define MAGIC_NUMBER_0 0x4B
#define MAGIC_NUMBER_1 0x54
#define MAGIC_NUMBER_2 0x52
#define MAGIC_NUMBER_3 0x43
#define FUNCTION_POINTER 0x20010020
#define SRAM_BASE_ADDRESS 0x20020000
#define SRAM_CMD_BASE_ADDRESS 0x2005F000
#define UPLOAD_HEADER_SRAM_ADDRESS 0x20010000
#define PAGE_SIZE 256
#define PAGES_PER_ROUND 16
#define SPI_INCREMENT 0x1000
#define UART_TIMEOUT 10 /* Read timeout in tenths of seconds */
#define MIN_READ_BYTES 1 /* Minimum bytes to read */
#define WAIT_RESPONSE_TIMEOUT 5000 /* in milliseconds */
#define WAIT_INTERVAL 100 /* in milliseconds */
#define PACKET_HEADER_LENGTH 6
#define CRC_LENGTH 2
#define DEFAULT_VALUE 0x00
#define PAGE_SIZE 256
#define PAGES_PER_ROUND 16
#define SRAM_BASE_ADDRESS 0x20020000
#define UPLOAD_HEADER_SRAM_ADDRESS 0x20010000
#define UPLOAD_FUNCTION_POINTER 0x20010020
#define SPI_INCREMENT 0x1000
#define RESPONSE_TIMEOUT 5
#define FRAME_SRAM_BASE_ADDRESS 0x20010020
#define FRAME_PAGE_SIZE 256
#define FRAME_RESPONSE_TIMEOUT 5
#define WP_COMMAND 0x0C /* Write protect command */
#define WP_RESPONSE_TIMEOUT 2 /* Timeout in seconds for response */
#define TOOL_DBG 0
#define SYNC_RETRY_CNT 3 /* Retry count of UART synchronization */
#define OPR_TIMEOUT 10L /* 10 seconds */

#if TOOL_DBG
#define DBG_PRINT(format, args...) printf(format, ##args)
#else
#define DBG_PRINT(format, args...)
#endif

/* Function to calculate Tool-side CRC (sum of all bytes) */
uint16_t calculate_crc_tool(const unsigned char *data, size_t length)
{
	uint16_t crc_chk = 0;
	for (size_t i = 0; i < length; i++) {
		crc_chk += data[i]; /* Sum each byte */
	}
	return crc_chk;
}

/* Function: Set up UART */
int configure_uart(int fd)
{
	struct termios tty;
	speed_t baudrate;
	if (tcgetattr(fd, &tty) != 0) {
		perror("tcgetattr failed");
		return -1;
	}

	/* Set baud rate */
	baudrate = BAUDRATE;
	cfsetospeed(&tty, baudrate);
	cfsetispeed(&tty, baudrate);
	tty.c_cflag |= baudrate;
	tty.c_cflag |= CS8;

	/* Enable software flow control (XON/XOFF) for pseudoterminals */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
			 ICRNL | IXON);

	/* Set raw mode */
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* Disable output processing */
	tty.c_oflag = ~OPOST;

	/* Set read timeout: 1 second */
	tty.c_cc[VTIME] = 5;
	tty.c_cc[VMIN] = 0;
	tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */

	/* enable reading */
	tty.c_cflag &= ~(PARENB | PARODD); /* shut off parity */
	tcflush(fd, TCIFLUSH);

	/* Apply settings */
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("tcsetattr failed");
		return -1;
	}

	return 0;
}

/* Function: UART synchronization */
int uart_sync(int uart_fd)
{
	unsigned char sync_send = UART_SYNC_BYTE;
	unsigned char sync_receive;
	int retry = SYNC_RETRY_CNT;

	printf("UART_SYNC operation initiated\n");

	while (retry--) {
		/* Send sync byte */
		if (write(uart_fd, &sync_send, 1) != 1) {
			perror("Failed to send sync byte");
			return 1;
		}
		DBG_PRINT("Sent sync byte: 0x%X\n", sync_send);

		/* Wait for response */
		ssize_t bytes_read = read(uart_fd, &sync_receive, 1);
		if (bytes_read > 0) {
			DBG_PRINT("Received response: 0x%X\n", sync_receive);
			if (sync_receive == UART_SYNC_RESPONSE) {
				printf("UART sync successful\n");
				return 0; /* Sync successful */
			} else {
				printf("Unexpected response: 0x%X (expected 0xA5)\n",
				       sync_receive);
			}
		} else if (bytes_read == 0) {
			printf("Read timeout occurred\n");
		} else {
			perror("Failed to read sync response");
		}

		/* Retry if not successful */
		if (retry > 0) {
			printf("Retrying UART sync... (%d attempts left)\n",
			       retry);
			sleep(1); /* Wait 1 second before retrying */
		}
	}

	printf("UART sync failed after 3 attempts\n");
	return 1; /* Sync failed */
}

/* Function: Build and send upload header packet */
int send_upload_header(int uart_fd, uint32_t sram_address, uint32_t spi_address,
		       uint32_t data_size_to_write)
{
	unsigned char *packet = (unsigned char *)malloc(24);

	if (!packet) {
		perror("Failed to allocate memory for upload header packet");
		return -1;
	}

	/* Construct packet (upload header command is fixed at 0x09) */
	packet[0] = UPLOAD_HEADER_COMMAND;
	packet[1] = 0x0F;

	/* Header address */
	packet[2] = (sram_address >> 24) & 0xFF;
	packet[3] = (sram_address >> 16) & 0xFF;
	packet[4] = (sram_address >> 8) & 0xFF;
	packet[5] = sram_address & 0xFF;

	/* Magic number is fixed at 0x4B, 0x54, 0x52, 0x43 */
	packet[6] = MAGIC_NUMBER_0;
	packet[7] = MAGIC_NUMBER_1;
	packet[8] = MAGIC_NUMBER_2;
	packet[9] = MAGIC_NUMBER_3;

	/* Data size to write */
	packet[10] = data_size_to_write & 0xFF;
	packet[11] = (data_size_to_write >> 8) & 0xFF;
	packet[12] = (data_size_to_write >> 16) & 0xFF;
	packet[13] = (data_size_to_write >> 24) & 0xFF;

	/* Content address: 0x20020000 */
	packet[14] = 0x00;
	packet[15] = 0x00;
	packet[16] = 0x02;
	packet[17] = 0x20;

	/* SPI address */
	packet[18] = spi_address & 0xFF;
	packet[19] = (spi_address >> 8) & 0xFF;
	packet[20] = (spi_address >> 16) & 0xFF;
	packet[21] = (spi_address >> 24) & 0xFF;

	/* Calculate CRC */
	uint16_t crc = calculate_crc_tool(packet, 22);

	/* Add CRC */
	packet[22] = (crc >> 8) & 0xFF;
	packet[23] = crc & 0xFF;

	/* Send packet */
	ssize_t bytes_written = write(uart_fd, packet, 24);
	if (bytes_written != 24) {
		perror("Failed to send upload header packet");
		free(packet);
		return -1;
	}

	DBG_PRINT("Sent Upload Header: SRAM Address=0x%08X, SPI Address=0x%08X,
		  Data Size to Write = 0x % 08X,
		  CRC = 0x % 04X\n ",
			sram_address,
		  spi_address, data_size_to_write, crc);

	free(packet);

	return 0;
}

/* Function: Build and send Packet A (data packet) */
int send_packet_a(int uart_fd, uint8_t command, size_t data_size,
		  uint32_t sram_address, const unsigned char *data)
{
	size_t actual_data_size = data_size + 1; /* Use the actual data size */
	size_t packet_length =
		PACKET_HEADER_LENGTH + actual_data_size + CRC_LENGTH;
	unsigned char *packet = (unsigned char *)malloc(packet_length);
	sigset_t block_mask, old_mask;

	if (!packet) {
		perror("Failed to allocate memory for Packet A");
		return -1;
	}

	/* Build packet */
	packet[0] = command;
	packet[1] = data_size; /* Original data size */
	packet[2] = (sram_address >> 24) & 0xFF;
	packet[3] = (sram_address >> 16) & 0xFF;
	packet[4] = (sram_address >> 8) & 0xFF;
	packet[5] = sram_address & 0xFF;

	/* Copy data into the packet */
	memcpy(&packet[PACKET_HEADER_LENGTH], data, actual_data_size);

	/* Calculate CRC (simple summation) */
	uint16_t crc = calculate_crc_tool(packet, PACKET_HEADER_LENGTH +
							  actual_data_size);

	/* Add CRC to the end of the packet */
	packet[PACKET_HEADER_LENGTH + actual_data_size] = (crc >> 8) &
							  0xFF; /* High byte */
	packet[PACKET_HEADER_LENGTH + actual_data_size + 1] = crc &
							      0xFF; /* Low
								       byte
								     */

	/* Send the packet */
	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT); /* Mask SIGINT Interrupt Signal */
	sigprocmask(SIG_BLOCK, &block_mask, &old_mask);
	ssize_t bytes_written = write(uart_fd, packet, packet_length);
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	if (bytes_written != packet_length) {
		perror("Failed to send Packet A");
		free(packet);
		return -1;
	}

	DBG_PRINT("Sent Packet A: Command=0x%X, Data Size=%zu,
		  SRAM Address = 0x % 08X,
		  CRC = 0x % 04X\n ",
			command,
		  data_size, sram_address, crc);

	free(packet);
	return 0;
}

/* Function: Build and send Packet B */
int send_packet_b(int uart_fd, uint8_t command, uint32_t func_pointer)
{
	size_t packet_length = PACKET_HEADER_LENGTH + CRC_LENGTH;
	unsigned char packet[8];

	packet[0] = command;
	packet[1] = DEFAULT_VALUE; /* Fixed default value */
	packet[2] = (func_pointer >> 24) & 0xFF;
	packet[3] = (func_pointer >> 16) & 0xFF;
	packet[4] = (func_pointer >> 8) & 0xFF;
	packet[5] = func_pointer & 0xFF;

	/* Calculate CRC */
	uint16_t crc = calculate_crc_tool(packet, PACKET_HEADER_LENGTH);
	packet[PACKET_HEADER_LENGTH] = (crc >> 8) & 0xFF;
	packet[PACKET_HEADER_LENGTH + 1] = crc & 0xFF;

	/* Send the packet */
	ssize_t bytes_written = write(uart_fd, packet, packet_length);
	if (bytes_written != packet_length) {
		perror("Failed to send Packet B");
		return -1;
	}

	DBG_PRINT(
		"Sent Packet B: Command=0x%X, Function Pointer=0x%08X, CRC=0x%04X\n",
		command, func_pointer, crc);

	return 0;
}

/* Function: Set UART read blocking mode */
void set_read_blocking(int dev_drv, int block)
{
	struct termios tty;

	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(dev_drv, &tty) != 0) {
		printf("X1: Fail.");
	}
	tty.c_cc[VMIN] = block;
	tty.c_cc[VTIME] = 5; /* 0.5 seconds read timeout */
	if (tcsetattr(dev_drv, TCSANOW, &tty) != 0) {
		printf("X2: Fail.");
	}
}

/*---------------------------------------------------------------------------
 * Function: uint32_t com_port_read_bin()
 *
 * Purpose:  Read a binary data from Comport
 *
 * Params:   device_id - the opened handle returned by com_port_open()
 *           buffer - this buffer will contain the arrived data
 *           buf_size - maximum data size to read
 *
 * Returns:  The number of bytes read.
 *
 * Comments: The caller must ensure that Size is not bigger than buffer size.
 *
 *---------------------------------------------------------------------------
 */
uint32_t com_port_read_bin(int device_id, uint8_t *buffer, uint32_t buf_size)
{
	int32_t read_bytes;

	/* Reset read blocking mode */
	set_read_blocking(device_id, 0);
	read_bytes = read(device_id, buffer, buf_size);
	if (read_bytes == -1) {
		printf("Y1: Fail.");
	}

	return read_bytes;
}

/*---------------------------------------------------------------------------
 * Function: uint32_t com_port_wait_read()
 *
 * Purpose:  Wait until a byte is received for read
 *
 * Params:   device_id - the opened handle returned by com_port_open()
 *
 * Returns:  The number of bytes that are waiting in RX queue.
 *
 *---------------------------------------------------------------------------
 */
uint32_t com_port_wait_read(int device_id)
{
	int32_t bytes;
	int32_t ret_val;
	struct pollfd fds;

	/* Set read blocking mode */
	set_read_blocking(device_id, 1);

	/* Wait up to 10 sec until byte is received for read. */
	fds.fd = device_id;
	fds.events = POLLIN;
	ret_val = poll(&fds, 1, 10000);
	if (ret_val < 0) {
		printf("Z1: Fail.");
		return 0;
	}
	bytes = 0;

	/* If data is ready for read. */
	if (ret_val > 0) {
		/* Get number of bytes that are ready to be read. */
		if (ioctl(device_id, FIONREAD, &bytes) < 0) {
			printf("Z2: Fail.");
			return 0;
		}
	}

	return bytes;
}

/* Function: Wait for EC response with timeout */
int wait_for_response(int uart_fd, uint8_t expected_response,
		      int timeout_seconds)
{
	unsigned char response;
	int total_wait_time = 0;
	ssize_t bytes_read;
	time_t start;
	double elapsed_time;
	uint32_t read_temp = 0;
	time(&start);

	DBG_PRINT("QQ: Start %d.\n", read_temp);
	do {
		read_temp = com_port_wait_read(uart_fd);
		elapsed_time = difftime(time(NULL), start);
		DBG_PRINT("QQ: get %d.\n", read_temp);
	} while ((read_temp < 1) && (elapsed_time <= OPR_TIMEOUT));

	DBG_PRINT("Enter wait\n");

	while (total_wait_time < timeout_seconds * 1000) {
		DBG_PRINT("starting read\n");
		bytes_read = read(uart_fd, &response, 1);
		DBG_PRINT("finish read\n");
		if (bytes_read > 0) {
			DBG_PRINT("Received response: 0x%X\n", response);
			if (response == expected_response) {
				return 0; /* Success */
			} else {
				printf("Unexpected response: 0x%X (expected: 0x%X)\n",
				       response, expected_response);
				return -1;
			}
		} else if (bytes_read == -1) {
			perror("Error reading from UART");
			return -1;
		} else if (bytes_read == 0) {
			printf("No data received, retrying...\n");
			usleep(WAIT_INTERVAL * 1000); /* Wait 100ms */
			total_wait_time += WAIT_INTERVAL;
		}
	}

	printf("Timeout waiting for response: 0x%X\n", expected_response);

	return -1;
}

/* Function: Send a block of pages */
int send_pages(int uart_fd, FILE *file, uint32_t sram_address,
	       size_t *total_bytes_sent, size_t *page)
{
	unsigned char data_buffer[PAGE_SIZE];
	int retry = 0;
	size_t bytes_read = fread(data_buffer, 1, PAGE_SIZE, file);

	if (bytes_read == 0) {
		if (feof(file)) {
			return 0; /* End of file */
		}
		perror("Failed to read from file");
		return -1;
	}

	/* Send this page's data */
	if (send_packet_a(uart_fd, 0x09, (bytes_read - 1), sram_address,
			  data_buffer) != 0) {
		return -1;
	}

	/* Wait for EC to respond with 0x09 (acknowledgment for
	 * this page) */
	if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
		DBG_PRINT(
			"Failed to receive expected response for data page %zu\n",
			*page + 1);
		retry = 1;
	}

	/* try again */
	if (retry == 1) {
		sleep(1);
		tcflush(uart_fd, TCIOFLUSH);

		/* Send this page's data */
		if (send_packet_a(uart_fd, 0x09, (bytes_read - 1), sram_address,
				  data_buffer) != 0) {
			return -1;
		}

		/* Wait for EC to respond with 0x09
		 * (acknowledgment for this page) */
		if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
			printf("Failed to receive expected response for data page %zu\n",
			       *page + 1);
			return -1;
		}
	}
	*total_bytes_sent += bytes_read;
	DBG_PRINT("Page %zu sent successfully. Total bytes sent: %zu\n",
		  *page + 1, *total_bytes_sent);
	(*page)++;

	return 0;
}

/* Function: Flash process to send data using Packet A */
int flash(int uart_fd, uint32_t spi_start, const char *file_name)
{
	FILE *file = fopen(file_name, "rb");
	int retry_round;

	if (!file) {
		perror("Failed to open binary file");
		return -1;
	}

	printf("Flash operation initiated\n");
	unsigned char data_buffer[PAGE_SIZE];
	size_t total_bytes_sent = 0;
	size_t page = 0;
	uint32_t upload_header_spi_address = spi_start;

	/* Get the total file size */
	fseek(file, 0, SEEK_END);
	size_t total_file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	DBG_PRINT("Total File Size: %zu bytes\n", total_file_size);

	while (total_bytes_sent < total_file_size) {
		printf(".");
		fflush(stdout);
		size_t remaining_data = total_file_size - total_bytes_sent;
		size_t data_size_to_write =
			remaining_data > PAGES_PER_ROUND * PAGE_SIZE ?
				PAGES_PER_ROUND * PAGE_SIZE :
				remaining_data;

		/* Send upload header packet to inform EC of the remaining data
		 * size */
		DBG_PRINT("Sending upload header for new round\n");
		if (send_upload_header(uart_fd, UPLOAD_HEADER_SRAM_ADDRESS,
				       upload_header_spi_address,
				       data_size_to_write) != 0) {
			fclose(file);
			return -1;
		}

		/* Wait for EC to respond with 0x09 */
		if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
			printf("Failed to receive expected response for upload header\n");
			fclose(file);
			return -1;
		}

		for (size_t i = 0;
		     i < PAGES_PER_ROUND && total_bytes_sent < total_file_size;
		     i++) {
			uint32_t sram_address =
				SRAM_BASE_ADDRESS + (i * PAGE_SIZE);

			/* Send pages */
			if (send_pages(uart_fd, file, sram_address,
				       &total_bytes_sent, &page) != 0) {
				fclose(file);
				return -1;
			}
		}

		retry_round = 0;
	retry_r:
		/* Send Packet B to request EC to move data, even if this round
		 * is not full 16 pages */
		DBG_PRINT(
			"Round %zu complete, sending function pointer to EC.\n",
			page / PAGES_PER_ROUND);
		if (send_packet_b(uart_fd, 0x06, UPLOAD_FUNCTION_POINTER) !=
		    0) {
			fclose(file);
			return -1;
		}

		/* Wait for EC to respond with 0x06 (acknowledgment for function
		 * pointer) */
		if (wait_for_response(uart_fd, 0x06, RESPONSE_TIMEOUT) != 0) {
			DBG_PRINT(
				"Failed to receive expected response for function pointer (first 0x06)\n");
			if (!retry_round) {
				retry_round = 1;
				goto retry_r;
			}
			fclose(file);
			return -1;
		}
		poll(NULL, 0, 100);

		/* Wait for EC to respond with 0x06 0x03 (execution success) */
		unsigned char response[2];
		ssize_t bytes_read = read(uart_fd, response, 2);
		poll(NULL, 0, 200);
		if (bytes_read != 2 || response[0] != 0x06 ||
		    response[1] != 0x03) {
			DBG_PRINT(
				"Failed to receive expected 0x06 0x03 response, received: 0x%X 0x%X\n",
				response[0], response[1]);
			if (!retry_round) {
				retry_round = 1;
				goto retry_r;
			}
			fclose(file);
			return -1;
		}
		/* Update SPI address for next round */
		upload_header_spi_address += SPI_INCREMENT;
		DBG_PRINT("EC successfully processed function pointer.\n");
		/* End loop if this is the final round */
		if (total_bytes_sent >= total_file_size) {
			break;
		}
	}

	DBG_PRINT("Final function pointer execution after all data sent.\n");
	printf("\nFlash operation finished.\n");
	fclose(file);

	return 0;
}

/* Function: Frame operation (stub) */
int frame(int uart_fd, uint32_t spi_start, const char *file_name)
{
	printf("Frame operation initiated\n");
	FILE *file = fopen(file_name, "rb");
	if (!file) {
		perror("Failed to open binary file");
		return -1;
	}
	unsigned char data_buffer[FRAME_PAGE_SIZE]; /* Data buffer for each page
						     */
	size_t total_bytes_sent = 0;
	size_t page = 0;
	/* Get the total file size */
	fseek(file, 0, SEEK_END);
	size_t total_file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	DBG_PRINT("Total File Size: %zu bytes\n", total_file_size);

	while (total_bytes_sent < total_file_size) {
		for (size_t i = 0; total_bytes_sent < total_file_size; i++) {
			uint32_t sram_address =
				FRAME_SRAM_BASE_ADDRESS + (i * FRAME_PAGE_SIZE);

			/* Send pages */
			if (send_pages(uart_fd, file, sram_address,
				       &total_bytes_sent, &page) != 0) {
				fclose(file);
				return -1;
			}
		}

		DBG_PRINT("EC successfully processed function pointer.\n");
		/* End loop if this is the final round */
		if (total_bytes_sent >= total_file_size) {
			break;
		}
	}

	printf("Frame operation finished.\n");
	fclose(file);

	return 0;
}

/* Function: Write protect (WP) operation (stub) */
int write_protect(int uart_fd, uint32_t spi_start, const char *file_name)
{
	printf("Write Protect (WP) operation initiated\n");
	unsigned char packet[2];
	packet[0] = WP_COMMAND;
	/* Only using the lower byte of spi_start */
	packet[1] = spi_start & 0x000000FF;

	DBG_PRINT("WP1\n");

	/* Send WP command */
	if (write(uart_fd, packet, 2) != 2) {
		perror("Failed to send WP command");
		return -1;
	}

	DBG_PRINT("WP2\n");
	/* Flush the UART input buffer */
	tcflush(uart_fd, TCIFLUSH);

	DBG_PRINT("WP3\n");
	/* Wait for expected response from EC */
	if (wait_for_response(uart_fd, WP_COMMAND, WP_RESPONSE_TIMEOUT) != 0) {
		printf("Failed to receive expected response for WP operation\n");
		return -1;
	}
	printf("WP operation successful\n");

	return 0;
}
void out_of_read_bin(int uart_fd)
{
	uint32_t sram_address = SRAM_CMD_BASE_ADDRESS;
	uint8_t data_buffer[4];
	data_buffer[0] = 0x00;
	data_buffer[1] = 0x00;
	data_buffer[2] = 0x00;
	data_buffer[3] = 0x00;

	send_packet_a(uart_fd, 0x09, 3, sram_address, data_buffer);
}
/* Function: Write protect (WP) operation (stub) */
int read_bin(int uart_fd, uint32_t spi_start, const char *file_name,
	     uint32_t spi_offset)
{
	printf("Read Bin operation initiated\n");

	FILE *file = fopen(file_name, "w");
	if (!file) {
		perror("Failed to open binary file");
		return -1;
	}
	fseek(file, 0, SEEK_SET);

	printf("Flash operation initiated\n");
	unsigned char data_buffer[PAGE_SIZE];
	size_t total_bytes_get = 0;
	size_t page = 0;
	uint32_t upload_header_spi_address = spi_start;
	int retry = 0, retry_round;
	uint32_t sram_address = SRAM_CMD_BASE_ADDRESS;

	while (total_bytes_get < spi_offset) {
		printf(".");
		fflush(stdout);
		size_t remaining_data = spi_offset - total_bytes_get;
		size_t data_size_to_read =
			remaining_data > (PAGES_PER_ROUND * PAGE_SIZE - 1) ?
				(PAGES_PER_ROUND * PAGE_SIZE - 1) :
				remaining_data;

		/* Send upload header packet to inform EC of the remaining data
		 * size */
		DBG_PRINT("Sending upload header for new round\n");
		if (send_upload_header(uart_fd, UPLOAD_HEADER_SRAM_ADDRESS,
				       upload_header_spi_address,
				       data_size_to_read) != 0) {
			fclose(file);
			return -1;
		}

		/* Wait for EC to respond with 0x09 */
		if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
			printf("Failed to receive expected response for upload header\n");
			fclose(file);
			return -1;
		}
		/* Calculate SRAM address, incremented per page */

		data_buffer[0] = 0xA5;
		data_buffer[1] = 0xA5;
		data_buffer[2] = 0xA5;
		data_buffer[3] = 0xA5;

		/* Send this page's data */
		if (send_packet_a(uart_fd, 0x09, 3, sram_address,
				  data_buffer) != 0) {
			fclose(file);
			out_of_read_bin(uart_fd);
			return -1;
		}

		/* Wait for EC to respond with 0x09 (acknowledgment for
		 * this page) */
		if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
			DBG_PRINT(
				"Failed to receive expected response for data page %zu\n",
				page + 1);
			retry = 1;
		}

		if (send_packet_b(uart_fd, 0x06, UPLOAD_FUNCTION_POINTER) !=
		    0) {
			fclose(file);
			out_of_read_bin(uart_fd);
			return -1;
		}

		/* Wait for EC to respond with 0x06 (acknowledgment for function
		 * pointer) */
		if (wait_for_response(uart_fd, 0x06, RESPONSE_TIMEOUT) != 0) {
			DBG_PRINT(
				"Failed to receive expected response for function pointer (first 0x06)\n");
			fclose(file);
			out_of_read_bin(uart_fd);
			return -1;
		}
		uint32_t bytes = 0;
		uint32_t timeout_cnt = 0;
		while (bytes < (data_size_to_read) && timeout_cnt < 100) {
			poll(NULL, 0, 100);
			ioctl(uart_fd, FIONREAD, &bytes);
			timeout_cnt++;
		}

		if (timeout_cnt == 100) {
			DBG_PRINT(
				"wait data till timeout data return %d data need %ld\r\n",
				bytes, data_size_to_read);
			out_of_read_bin(uart_fd);
			return -1;
		}
		unsigned char response[PAGES_PER_ROUND * PAGE_SIZE];
		ssize_t bytes_read =
			read(uart_fd, response, (data_size_to_read));

		if (bytes_read != (data_size_to_read)) {
			DBG_PRINT(
				"data size no match data return %ld data need %ld\r\n",
				bytes_read, data_size_to_read);
			out_of_read_bin(uart_fd);
			return -1;
		}

		total_bytes_get += bytes_read;
		DBG_PRINT("Page %zu sent successfully. Total bytes sent: %zu\n",
			  page + 1, total_bytes_get);
		//    for(int i=0;i<0x20;i++){
		// 	DBG_PRINT("response[0] %x response[last] %x\n",
		// 		response[i], response[PAGES_PER_ROUND *
		// PAGE_SIZE - 0x20+i]);
		//    }

		page += PAGES_PER_ROUND;
		fwrite(response, 1, data_size_to_read, file);

		/* Wait for EC to respond with 0x06 0x03 (execution success) */
		unsigned char bootrom_response[2];
		bytes_read = read(uart_fd, bootrom_response, 2);
		poll(NULL, 0, 200);
		if (bytes_read != 2 || bootrom_response[0] != 0x06 ||
		    bootrom_response[1] != 0x05) {
			DBG_PRINT(
				"Failed to receive expected 0x06 0x03 response, received: 0x%X 0x%X\n",
				bootrom_response[0], bootrom_response[1]);
			if (!retry_round) {
				retry_round = 1;
			}
			fclose(file);
			out_of_read_bin(uart_fd);
			return -1;
		}
		/* Update SPI address for next round */
		upload_header_spi_address += data_size_to_read;
		DBG_PRINT("EC successfully processed function pointer.\n");
		/* End loop if this is the final round */
		if (total_bytes_get >= spi_offset) {
			break;
		}
	}
	DBG_PRINT("Final function pointer execution after all data sent.\n");
	printf("Flash operation finished.\n");
	fclose(file);

	data_buffer[0] = 0x00;
	data_buffer[1] = 0x00;
	data_buffer[2] = 0x00;
	data_buffer[3] = 0x00;

	/* Send this page's data */
	if (send_packet_a(uart_fd, 0x09, 3, sram_address, data_buffer) != 0) {
		fclose(file);
		out_of_read_bin(uart_fd);
		return -1;
	}

	/* Wait for EC to respond with 0x09 (acknowledgment for
	 * this page) */
	if (wait_for_response(uart_fd, 0x09, RESPONSE_TIMEOUT) != 0) {
		DBG_PRINT(
			"Failed to receive expected response for data page %zu\n",
			page + 1);
		retry = 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	const char *method = NULL;
	uint32_t spi_start = 0;
	const char *file_name = NULL;
	const char *uart_device = NULL;
	uint32_t spi_offset = 0;
	int opt;

	static struct option long_options[] = {
		{ "method", required_argument, 0, 'm' },
		{ "spi_start", required_argument, 0, 's' },
		{ "file", required_argument, 0, 'f' },
		{ "uart_device", required_argument, 0, 'u' },
		{ "spi_offset", required_argument, 0, 'o' },
		{ 0, 0, 0, 0 }
	};

	while ((opt = getopt_long(argc, argv, "m:s:f:u:o:", long_options,
				  NULL)) != -1) {
		switch (opt) {
		case 'm':
			method = optarg;
			break;
		case 's':
			spi_start = strtoul(optarg, NULL, 0);
			break;
		case 'f':
			file_name = optarg;
			break;
		case 'u':
			uart_device = optarg;
			break;
		case 'o':
			spi_offset = strtoul(optarg, NULL, 0);
			break;
		default:
			printf("Usage: %s --method <method> --spi_start <spi_start>
			       --file<binary_file> --uart_device<uart_device>\n
			       ",
			       argv[0]);
			return 1;
		}
	}

	if (!method || !file_name || !uart_device) {
		printf("Missing required arguments\n");
		printf("Usage: %s --method <method> --spi_start <spi_start>
		       --file<binary_file> --uart_device<uart_device>\n ",
		       argv[0]);
		return 1;
	}

	printf("Method: %s\n", method);
	printf("SPI Start: 0x%08X\n", spi_start);
	printf("Binary File: %s\n", file_name);
	printf("UART Device: %s\n", uart_device);
	printf("SPI Offset: 0x%08X\n", spi_offset);

	/* Open UART device */
	int uart_fd = open(uart_device, O_RDWR | O_NOCTTY);
	if (uart_fd == -1) {
		perror("Unable to open UART device");
		return 1;
	}

	/* Configure UART settings */
	if (configure_uart(uart_fd) != 0) {
		close(uart_fd);
		return 1;
	}

	/* Perform UART synchronization */
	if (uart_sync(uart_fd) != 0) {
		printf("UART sync failed\n");
		close(uart_fd);
		return 1;
	}

	/* Execute method-based operation */
	if (strcmp(method, "flash") == 0) {
		if (flash(uart_fd, spi_start, file_name) != 0) {
			printf("Flash process failed\n");
			close(uart_fd);
			return 1;
		}
	} else if (strcmp(method, "frame") == 0) {
		if (frame(uart_fd, spi_start, file_name) != 0) {
			printf("Frame process failed\n");
			close(uart_fd);
			return 1;
		}
	} else if (strcmp(method, "wp") == 0) {
		if (write_protect(uart_fd, spi_start, file_name) != 0) {
			printf("WP process failed\n");
			close(uart_fd);
			return 1;
		}
	} else if (strcmp(method, "read_bin") == 0) {
		if (read_bin(uart_fd, spi_start, file_name, spi_offset) != 0) {
			printf("Read Bin failed\n");
			close(uart_fd);
			return 1;
		}
	} else {
		printf("Unknown method: %s\n", method);
		close(uart_fd);
		return 1;
	}

	close(uart_fd);

	return 0;
}
