/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export_taskinfo.h"

int main(int argc, char **argv)
{
	/* Default section is RW */
	const char *section = "RW";
	const char *output_path = NULL;
	/* Default output is standard output */
	FILE *output = stdout;
	FILE *output_file = NULL;
	int nopt;
	const char * const short_opt = "hs:o:";
	const struct option long_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "section", 1, NULL, 's' },
		{ "out", 1, NULL, 'o' },
		{ NULL }
	};
	uint32_t i;
	const struct taskinfo *taskinfo_list;
	uint32_t taskinfo_num;

	do {
		nopt = getopt_long(argc, argv, short_opt, long_opts, NULL);
		switch (nopt) {
		case 'h': /* -h or --help */
			printf("USAGE: %s -s <RO | RW (default)> "
				"-o <output file>\n", argv[0]);
			return 1;

		case 's': /* -s or --section */
			section = optarg;
			break;

		case 'o': /* -o or --out */
			output_path = optarg;
			break;

		case -1:
			break;

		default:
			abort();
		}
	} while (nopt != -1);

	if (!strcmp(section, "RO")) {
		taskinfo_num = get_ro_taskinfos(&taskinfo_list);
	} else if (!strcmp(section, "RW")) {
		taskinfo_num = get_rw_taskinfos(&taskinfo_list);
	} else {
		fprintf(stderr, "ERROR: Please specify section = RO | RW.\n");
		return 1;
	}

	if (output_path != NULL) {
		output_file = fopen(output_path, "w");
		if (output_file == NULL) {
			fprintf(stderr,
				"ERROR: Can not create the output file.\n");
			return 1;
		}
		output = output_file;
	}

	for (i = 0; i < taskinfo_num; i++) {
		const struct taskinfo *info = &taskinfo_list[i];

		fprintf(output, "\"%s\",%s,%u\n", info->name, info->routine,
			info->stack_size);
	}

	if (output_file != NULL) {
		/* Close the output file */
		fclose(output_file);
	}

	return 0;
}
