/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "desc_parser.h"

static int process_entry(char *entry,
			 struct boards *all_boards, size_t max_size)
{
	printf("%s", entry);
	return 0;
}

static int process_file(FILE *desc_file,
			struct boards *all_boards,
			size_t max_size)
{
	char entry[10000];  /* One entry sure should not exceed 10K in size. */
	size_t index = 0;

	while (fgets(entry + index, sizeof(entry) - index, desc_file)) {
		if (entry[index] == '#')
			continue; /* Skip the comment */

		if (entry[index] == '\n' && index) {
			if (process_entry(entry, all_boards, max_size))
				return -1;
			index = 0;
			continue;
		}

		/* Make sure the next string overwrites the newline. */
		index += strlen(entry + index) - 1;
	}
	return 0;
}

struct boards *get_board_descriptions(const char *desc_name)
{
	FILE *desc_file;
	size_t max_size;
	struct boards *all_boards = NULL;
	struct stat sbuf;

	desc_file = fopen(desc_name, "r");
	if (!desc_file) {
		fprintf(stderr, "Error:%s can not open file '%s'\n",
			strerror(errno), desc_name);
		return NULL;
	}

	if (fstat(fileno(desc_file), &sbuf)) {
		fprintf(stderr, "Error:%s can not stat file '%s'\n",
			strerror(errno), desc_name);
		goto error_exit;
	}

	max_size = sbuf.st_size * 3;
	all_boards = malloc(max_size);

	if (!all_boards) {
		fprintf(stderr, "Failed to allocate %zd bytes\n", max_size);
		goto error_exit;
	}

	if (!process_file(desc_file, all_boards, max_size))
		return all_boards;

 error_exit:
	if (desc_file)
		fclose(desc_file);

	if (all_boards)
		free(all_boards);

	return NULL;
}

#ifdef TEST_PARSER
int main(int argc, char **argv)
{
	if (argc > 1)
		get_board_descriptions(argv[1]);

	return 0;
}
#endif
