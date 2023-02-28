#include "ec_commands.h"

int clock_get_freq(void)
{
	return 48000000;
}

/*
void clear_typematic_key(void)
{
}

void keyboard_clear_buffer(void)
{
}

void keyboard_state_changed(int row, int col, int is_pressed)
{
}
*/

void *memcpy_to_usbram(void *dest, const void *src, size_t n)
{
	return NULL;
}

int usb_is_suspended(void)
{
	return 0;
}

static const struct ec_response_keybd_config kb = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_BRIGHTNESS_DOWN,	/* T5 */
		TK_BRIGHTNESS_UP,	/* T6 */
		TK_MICMUTE,		/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY,
};

__override const struct ec_response_keybd_config *
board_vivaldi_keybd_config(void)
{
	return &kb;
}
