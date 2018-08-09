#include "host_command.h"
#include "console.h"
#include "time.h"
#include "util.h"
#include "task.h"

static uint32_t wake_from;
static uint32_t wake_time;
static timestamp_t deadline;

static int magic_autowake_handler(struct host_cmd_handler_args *args)
{
	const struct ec_params_autowake *p = args->params;
	struct ec_response_autowake *r = args->response;
	wake_from = p->wake_from;
	wake_time = p->wake_time;

	if (wake_from == FROM_S5)
		ccprintf("wake from S5\n");
	else if (wake_from == FROM_S3_S0IX)
		ccprintf("wake from S3_S0IX\n");
	else
		ccprintf("stop autowake test\n");

	deadline = get_time();
	/* instead of deadline.val += wake_time * HOUR; */
	deadline.val += wake_time * 1000000ull;

	ccprintf("wake_time=%d\n", wake_time);
	task_wake(TASK_ID_AUTOWAKE);

	r->out_data = EC_SUCCESS;
	args->response_size = sizeof(*r);

        return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_MAGIC_AUTOWAKE,
                     magic_autowake_handler,
                     EC_VER_MASK(0));

extern int command_powerinfo(int argc, char **argv);
extern int command_powerbtn(int argc, char **argv);
void autowake_task(void *u)
{
	/*
	 * unmark to give a specific powerbtn time
	 * char *powerbtn_argv[] = {"command_powerbtn", "500"};
	 */
	while (1) {
		if (wake_time) {
			ccprints("deadline at 0x%016lx = %.6ld s", deadline.val, deadline.val);
			command_powerinfo(0, NULL);
			ccprintf("\n");

			if (timestamp_expired(deadline, NULL)) {
				ccprints("wake system and exit test!");
				command_powerbtn(0, NULL);
				/*
				 * unmark to give a specific powerbtn time
				 * command_powerbtn(sizeof(powerbtn_argv), powerbtn_argv);
				 */
				wake_time = 0;
			}

			task_wait_event(SECOND); /* secondly check is enough */
		} else {
			task_wait_event(-1);
		}
	}
}
