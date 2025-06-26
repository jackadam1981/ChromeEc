/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "miniz.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char *read_all_bytes(const char *filename, long *file_size)
{
	FILE *file = NULL;
	unsigned char *buffer = NULL;
	long size = 0;

	// 1. Open the file in binary read mode
	file = fopen(filename, "rb");
	if (file == NULL) {
		perror("Error opening file");
		return NULL;
	}

	// 2. Determine the file size
	if (fseek(file, 0, SEEK_END) != 0) {
		perror("Error seeking to end of file");
		fclose(file);
		return NULL;
	}
	size = ftell(file);
	if (size == -1) {
		perror("Error getting file size");
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		perror("Error seeking to beginning of file");
		fclose(file);
		return NULL;
	}

	// 3. Allocate memory to hold the file's content
	buffer = (unsigned char *)malloc(size);
	if (buffer == NULL) {
		perror("Error allocating memory");
		fclose(file);
		return NULL;
	}

	// 4. Read the entire file into the buffer
	size_t bytes_read = fread(buffer, 1, size, file);
	if (bytes_read != size) {
		perror("Error reading file content");
		free(buffer); // Free allocated memory on error
		fclose(file);
		return NULL;
	}

	// 5. Close the file
	fclose(file);

	// Set the file_size output parameter
	if (file_size != NULL) {
		*file_size = size;
	}

	return buffer; // Return the allocated buffer containing file data
}

void write_to_file(const char *filename, unsigned char *data_bytes,
		   size_t num_bytes_to_write_1)
{
	FILE *file = fopen(filename, "wb");
	if (file == NULL) {
		perror("Error opening file for writing (wb)");
		return;
	}

	// Write the byte array to the file
	// fwrite(pointer_to_data, size_of_each_item, number_of_items,
	// file_pointer)
	size_t bytes_written_1 =
		fwrite(data_bytes, 1, num_bytes_to_write_1, file);
	if (bytes_written_1 != num_bytes_to_write_1) {
		perror("Error writing all bytes (Example 1)");
		// Depending on your error handling, you might want to return
		// here or attempt to recover. For simplicity, we'll just print
		// and continue.
	} else {
		printf("Successfully wrote %zu bytes to '%s' (Example 1)\n",
		       bytes_written_1, filename);
	}

	// Close the file
	fclose(file);
	file = NULL; // Good practice to set to NULL after closing
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		perror("Error argc");
		return 1;
	}
	long file_size = 0;
	unsigned char *buffer = read_all_bytes(argv[1], &file_size);

	int cmp_status;
	uLong src_len = file_size;
	uLong cmp_len = compressBound(src_len);
	uLong uncomp_len = src_len;
	uint8_t *pCmp, *pUncomp;
	uint32_t total_succeeded = 0;

	// Allocate buffers to hold compressed and uncompressed data.
	pCmp = (uint8_t *)malloc((size_t)cmp_len);
	pUncomp = (uint8_t *)malloc((size_t)src_len);
	if ((!pCmp) || (!pUncomp)) {
		printf("Out of memory!\n");
		free(buffer);
		return EXIT_FAILURE;
	}

	// Compress the string.
	cmp_status = compress2(pCmp, &cmp_len, (const unsigned char *)buffer,
			       src_len, Z_BEST_COMPRESSION);
	if (cmp_status != Z_OK) {
		printf("compress() failed!\n");
		free(buffer);
		free(pCmp);
		free(pUncomp);
		return EXIT_FAILURE;
	}

	printf("Compressed from %u to %u bytes\n", (uint32_t)src_len,
	       (uint32_t)cmp_len);

	write_to_file(argv[2], pCmp, cmp_len);
	// Decompress.
	cmp_status = uncompress(pUncomp, &uncomp_len, pCmp, cmp_len);
	total_succeeded += (cmp_status == Z_OK);

	if (cmp_status != Z_OK) {
		printf("uncompress failed!\n");
		free(buffer);
		free(pCmp);
		free(pUncomp);
		return EXIT_FAILURE;
	}

	printf("Decompressed from %u to %u bytes\n", (uint32_t)cmp_len,
	       (uint32_t)uncomp_len);

	// Ensure uncompress() returned the expected data.
	if ((uncomp_len != src_len) ||
	    (memcmp(pUncomp, buffer, (size_t)src_len))) {
		printf("Decompression failed!\n");
		free(buffer);
		free(pCmp);
		free(pUncomp);
		return EXIT_FAILURE;
	}

	free(buffer);
	free(pCmp);
	free(pUncomp);

	printf("Success.\n");
	return EXIT_SUCCESS;
}
