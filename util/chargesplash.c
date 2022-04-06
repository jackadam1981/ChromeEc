/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "comm-host.h"
#include "ec_commands.h"
#include "ectool.h"
#include "misc_util.h"

#define FRECON_PATH "/sbin/frecon"
#define FRECON_PIDFILE "/run/frecon/pid"
#define FRECON_VT_FILE "/run/frecon/vt0"

static int read_pidfile(const char *path)
{
	int rv = -1;
	FILE *pidfile;

	pidfile = fopen(path, "r");
	if (!pidfile) {
		fprintf(stderr, "%s: failed to open file: %s\n", path,
			strerror(errno));
		return -1;
	}

	if (fscanf(pidfile, "%d", &rv) != 1) {
		fprintf(stderr, "%s: file format error\n", path);
	}

	fclose(pidfile);
	return rv;
}

static void kill_frecon(void)
{
	int pid;

	pid = read_pidfile(FRECON_PIDFILE);
	if (pid < 0) {
		return;
	}

	if (kill(pid, SIGTERM) < 0) {
		perror("kill failed");
		return;
	}
}

static FILE *start_frecon(void)
{
	int child_pid;
	int wstatus;
	FILE *frecon_vt;

	/* If frecon is already running, we kill it to take over */
	kill_frecon();

	child_pid = fork();
	if (child_pid < 0) {
		perror("fork failed");
		return NULL;
	}

	if (child_pid == 0) {
		execl(FRECON_PATH, FRECON_PATH, "--daemon", "--no-login",
		      "--enable-vt1", "--enable-osc", "--pre-create-vts", NULL);
		perror("exec failed");
		exit(1);
	}

	if (waitpid(child_pid, &wstatus, 0) < 0) {
		perror("waitpid failed");
		return NULL;
	}

	if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus)) {
		fprintf(stderr, "frecon daemon startup exited with status %d\n",
			WEXITSTATUS(wstatus));
		return NULL;
	}

	frecon_vt = fopen(FRECON_VT_FILE, "r+");
	if (!frecon_vt) {
		fprintf(stderr, "failed to open %s\n", FRECON_VT_FILE);
		return NULL;
	}

	return frecon_vt;
}

static int chargesplash_communicate(struct ec_response_charge_state *response)
{
	int rv = 0;
	struct ec_params_charge_state param = {
		.cmd = CHARGE_STATE_CMD_GET_STATE,
	};

	rv = ec_command(EC_CMD_CHARGE_STATE, 2, &param, sizeof(param), response,
			sizeof(*response));
	if (rv < 0) {
		fprintf(stderr, "Charge state command failed\n");
		return -1;
	}

	if (!response->get_state.boot_for_chargesplash) {
		fprintf(stderr, "Full boot requested\n");
		return -1;
	}

	return 0;
}

int cmd_chargesplash(int argc, char **argv)
{
	struct ec_response_charge_state response;
	FILE *frecon_vt;

	if (!ec_cmd_version_supported(EC_CMD_CHARGE_STATE, 2)) {
		fprintf(stderr, "EC too old to support chargesplash\n");
		return -1;
	}

	if (chargesplash_communicate(&response) < 0) {
		return -1;
	}

	frecon_vt = start_frecon();
	if (!frecon_vt) {
		fprintf(stderr, "Failed to start frecon!\n");
		return -1;
	}

	do {
		/* Text-based UI for now, make pretty later */
		fprintf(frecon_vt, "\r%d%% [%s]",
			response.get_state.batt_state_of_charge,
			response.get_state.ac ? "CHARGING" : "DISCHARGING");
		fflush(frecon_vt);

		/* Busy loop for now, make this a selectable-event later */
		usleep(100 * 1000);
	} while (chargesplash_communicate(&response) >= 0);

	fclose(frecon_vt);
	kill_frecon();
	return 0;
}
