/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "comm-host.h"
#include "cros_ec_dev.h"
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

static void terminate_frecon(void)
{
	int pid;

	pid = read_pidfile(FRECON_PIDFILE);
	if (pid < 0) {
		return;
	}

	fprintf(stderr, "Terminating frecon process with pid %d\n", pid);

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

	/* If frecon is already running, take over */
	terminate_frecon();

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

	fprintf(stderr, "Started frecon with pid %d\n",
		read_pidfile(FRECON_PIDFILE));

	frecon_vt = fopen(FRECON_VT_FILE, "r+");
	if (!frecon_vt) {
		fprintf(stderr, "failed to open %s\n", FRECON_VT_FILE);
		return NULL;
	}

	return frecon_vt;
}

static int get_charge_state(struct ec_response_charge_state *response)
{
	struct ec_params_charge_state params = {
		.cmd = CHARGE_STATE_CMD_GET_STATE,
	};

	return ec_command(EC_CMD_CHARGE_STATE, 0, &params, sizeof(params),
			  response, sizeof(*response));
}

static int chargesplash_hostcmd(enum ec_chargesplash_cmd cmd,
				struct ec_response_chargesplash *response)
{
	struct ec_params_chargesplash params = {
		.cmd = cmd,
	};

	return ec_command(EC_CMD_CHARGESPLASH, 0, &params, sizeof(params),
			  response, sizeof(*response));
}

static int display_loop(void)
{
	struct ec_response_chargesplash response;
	struct ec_response_charge_state charge_state;
	FILE *frecon_vt;

	if (chargesplash_hostcmd(EC_CHARGESPLASH_DISPLAY_READY, &response) <
	    0) {
		fprintf(stderr, "EC does not support chargesplash\n");
		return -1;
	}

	if (!response.requested) {
		fprintf(stderr, "Chargesplash not requested\n");
		return 0;
	}

	frecon_vt = start_frecon();
	if (!frecon_vt) {
		fprintf(stderr, "Failed to start frecon!\n");
		return -1;
	}

	do {
		if (get_charge_state(&charge_state) < 0) {
			fprintf(stderr, "Failed to get charge state");
			break;
		}

		/* Text-based UI for now, make pretty later */
		fprintf(frecon_vt, "\r%d%% [%s]",
			charge_state.get_state.batt_state_of_charge,
			charge_state.get_state.ac ? "CHARGING" : "DISCHARGING");
		fflush(frecon_vt);

		usleep(250 * 1000);

		if (chargesplash_hostcmd(EC_CHARGESPLASH_GET_STATE, &response) <
		    0) {
			fprintf(stderr, "Failed to get splash state");
			break;
		}
	} while (response.requested);

	fclose(frecon_vt);
	terminate_frecon();
	return 0;
}

int main(int argc, char **argv)
{
	if (comm_init_dev(CROS_EC_DEV_NAME) != 0) {
		fprintf(stderr, "Unable to initialize EC dev\n");
		return 1;
	}

	return display_loop();
}
