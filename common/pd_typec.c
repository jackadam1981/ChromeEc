#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "util.h"
#include <string.h>

static enum ec_status host_typec_control(struct host_cmd_handler_args *args) {
	const struct ec_params_typec_control *p = args->params;
	struct typec_ctrl_resp *resp = (struct typec_ctrl_resp *)args->response;
	args->response_size = sizeof(struct typec_ctrl_resp);
	ccprints("In host typec control command\n");
	switch(p->command) {
		case TYPEC_CONTROL_COMMAND_EXIT_MODES:
			ccprints("Exit the mode\n");
			strzcpy(resp->operation, "Exit", sizeof(resp->operation));
			break;
		case TYPEC_CONTROL_COMMAND_CLEAR_EVENTS:
			ccprints("Clear the events\n");
			strzcpy(resp->operation, "Clear", sizeof(resp->operation));
			break;
		case TYPEC_CONTROL_COMMAND_ENTER_MODE:
			ccprints("Enter mode: %u\n", p->mode_to_enter);
			strzcpy(resp->operation, "Enter", sizeof(resp->operation));
			resp->mode = p->mode_to_enter;
			break;
		case TYPEC_CONTROL_COMMAND_TBT_UFP_REPLY:
			ccprints("TBT ufp reply\n");
			strzcpy(resp->operation, "UFPReply", sizeof(resp->operation));
			break;
		default:
			ccprints("Operation not recongnized\n");
			strzcpy(resp->operation, "Noop", sizeof(resp->operation));
	}
	ccprints("Mode: %u\t operation:%s\n", resp->mode, resp->operation);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_TYPEC_CMD, host_typec_control, EC_VER_MASK(0) | EC_VER_MASK(1) | EC_VER_MASK(2));
