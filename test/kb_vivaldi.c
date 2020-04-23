#include <ec_commands.h>
#include <util.h>
#include <test_util.h>

static const struct ec_response_keybd_config keybd1 = {
	.num_top_row_keys = 13,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_KBD_BKLIGHT_DOWN,	/* T8 */
		TK_KBD_BKLIGHT_UP,	/* T9 */
		TK_PLAY_PAUSE,		/* T10 */
		TK_VOL_MUTE,		/* T11 */
		TK_VOL_DOWN,		/* T12 */
		TK_VOL_UP,		/* T13 */
	},
};

__override const struct ec_response_keybd_config
*board_vivaldi_keybd_config(void)
{
	return &keybd1;
}

static int test_ec_cmd_get_keybd_config(void)
{
	struct ec_response_keybd_config resp;
	int rv;

	ccprintf("%s: enter\n", __func__);

	rv = test_send_host_command(EC_CMD_GET_KEYBD_CONFIG, 0, NULL, 0,
				    &resp, sizeof(resp));
	
	ccprintf("%s: rv = %d enter\n", __func__, rv );
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	wait_for_task_started();
	RUN_TEST(test_ec_cmd_get_keybd_config);
	test_print_result();
}
